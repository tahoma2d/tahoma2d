#include <cmath>      // pow(-)
#include <stdexcept>  // std::domain_error(-)
#include <limits>     // std::numeric_limits
#include <vector>
#include <cassert>
#include "igs_ifx_common.h" /* igs::image::rgba */
#include "igs_levels.h"

#include "tutil.h"  // areAlmostEqual

//------------------------------------------------------------
namespace {
void levels_(double &val  // 0...1
             ,
             const double in_min  // 0...1
             ,
             const double in_max  // 0...1
             ,
             const double gamma  // 0.1 ... 10.0
             ,
             const double out_min  // 0...1
             ,
             const double out_max  // 0...1
             ,
             const bool clamp_sw) {
  /* 1 入力範囲を正規化 */
  val = (in_max == in_min) ? in_max : (val - in_min) / (in_max - in_min);

  /* 2 (出力範囲に)clamp */
  if (clamp_sw || !areAlmostEqual(out_max, 1.)) {
    val = (val < 0.0) ? 0.0 : ((1.0 < val) ? 1.0 : val);
  } else {
    val = (val < 0.0) ? 0.0 : val;
  }

  /* 3 正規化の範囲でgamma変換 */
  if ((1.0 != gamma) && (0.0 != gamma)) {
    if ((0.0 < val) && (val < 1.0)) {
      val = pow(val, 1.0 / gamma);
    }
    // 繧ｬ繝ｳ繝櫁｣懈ｭ｣縺ｮ邱壹ｒ逶ｴ邱壹〒蟒ｶ髟ｷ縺吶ｋ
    else if (val > 1.0) {
      val = 1. + (val - 1.) / gamma;
    }
  }

  /* 4 正規化範囲を出力範囲に */
  val = out_min + val * (out_max - out_min);

  /* 5 clamp */
  if (clamp_sw)
    val = (val < 0.0) ? 0.0 : ((1.0 < val) ? 1.0 : val);
  else
    val = (val < 0.0) ? 0.0 : val;
}
double refchk_(const int src, const int tgt, const double refv) {
  return (src < tgt) ? (tgt - src + 0.999999) * refv + src
                     : (src - tgt + 0.999999) * (1.0 - refv) + tgt;
}
float refchk_(const float src, const float tgt, const double refv) {
  return tgt * refv + src * (1. - refv);
}
//------------------------------------------------------------
/*
        std::vector< std::vector<T> > &tables
 とするとvc2005mdで他プログラムとのリンク時、
        allocatorの２重定義でエラーとなる。
        std::vector< std::vector<int> >&tables
 ならOK
 2009-01-27
 */
template <class IT, class RT>
void change_(IT *image_array, const int height, const int width,
             const int channels

             ,
             const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
             ,
             const int ref_mode  // R,G,B,A,luminance

             ,
             const double r_in_min, const double r_in_max  // 0...1
             ,
             const double g_in_min, const double g_in_max  // 0...1
             ,
             const double b_in_min, const double b_in_max  // 0...1
             ,
             const double a_in_min, const double a_in_max  // 0...1
             ,
             const double r_gamma  // 0.1 ... 10.0
             ,
             const double g_gamma  // 0.1 ... 10.0
             ,
             const double b_gamma  // 0.1 ... 10.0
             ,
             const double a_gamma  // 0.1 ... 10.0
             ,
             const double r_out_min, const double r_out_max  // 0...1
             ,
             const double g_out_min, const double g_out_max  // 0...1
             ,
             const double b_out_min, const double b_out_max  // 0...1
             ,
             const double a_out_min, const double a_out_max  // 0...1
             ,
             const bool clamp_sw, const bool alpha_sw,
             const bool add_blend_sw) {
  /* 1 最大値、最小値から変換テーブルを求める */
  std::vector<std::vector<int>> tables;
  const unsigned int val_max = std::numeric_limits<IT>::max();
  const double div_val       = static_cast<double>(val_max);
  const double mul_val       = div_val + 0.999999;
  {
    using namespace igs::image::rgba;
    tables.resize(siz);
    tables[red].resize(val_max + 1);
    tables[gre].resize(val_max + 1);
    tables[blu].resize(val_max + 1);
    tables[alp].resize(val_max + 1);
    for (unsigned int yy = 0; yy <= val_max; ++yy) {
      double rr, gg, bb, aa;
      rr = gg = bb = aa = yy / div_val;
      levels_(rr, r_in_min, r_in_max, r_gamma, r_out_min, r_out_max, clamp_sw);
      levels_(gg, g_in_min, g_in_max, g_gamma, g_out_min, g_out_max, clamp_sw);
      levels_(bb, b_in_min, b_in_max, b_gamma, b_out_min, b_out_max, clamp_sw);
      levels_(aa, a_in_min, a_in_max, a_gamma, a_out_min, a_out_max, clamp_sw);
      /* 0〜1.0 --> 0〜mul_maxスケール変換し、整数値化 */
      tables[red][yy] = static_cast<int>(rr * mul_val);
      tables[gre][yy] = static_cast<int>(gg * mul_val);
      tables[blu][yy] = static_cast<int>(bb * mul_val);
      tables[alp][yy] = static_cast<int>(aa * mul_val);
    }
  }

  /* 2 変換テーブルを使ってlevel変換する */
  const int pixsize = height * width;
  const int r_max   = std::numeric_limits<RT>::max();
  if (igs::image::rgba::siz == channels) {
    using namespace igs::image::rgba;
    for (int ii = 0; ii < pixsize; ++ii, image_array += channels) {
      /* 変化量初期値 */
      double refv = 1.0;

      /* 参照画像あればピクセル単位の画像変化量を得る */
      if (ref != 0) {
        refv *= igs::color::ref_value(ref, channels, r_max, ref_mode);
        ref += channels; /* continue;の前に行うこと */
      }

      /* AlphaのLevel処理した値をrefvの変化量比で加える */
      if (alpha_sw) {
        image_array[alp] =
            static_cast<IT>(refchk_(static_cast<int>(image_array[alp]),
                                    tables[alp][image_array[alp]], refv));
      }

      /* 加算合成で、Alpha値ゼロならRGB値を計算する必要はない */
      if (add_blend_sw && (0 == image_array[alp])) {
        continue;
      }
      /* 加算合成でなくAlpha合成の時は、Alpha値がゼロでも
      RGB値は存在する(してもよい)の計算する */

      /* 加算合成で、その値がMaxでなければ変化量に乗算 */
      if (add_blend_sw && (image_array[alp] < val_max)) {
        refv *= static_cast<double>(image_array[alp]) / div_val;
      }

      /* RGBのLevel処理した値をrefvの変化量比で加える */
      image_array[red] =
          static_cast<IT>(refchk_(static_cast<int>(image_array[red]),
                                  tables[red][image_array[red]], refv));
      image_array[gre] =
          static_cast<IT>(refchk_(static_cast<int>(image_array[gre]),
                                  tables[gre][image_array[gre]], refv));
      image_array[blu] =
          static_cast<IT>(refchk_(static_cast<int>(image_array[blu]),
                                  tables[blu][image_array[blu]], refv));
    }
  } else if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    for (int ii = 0; ii < pixsize; ++ii, image_array += channels) {
      double refv = 1.0;
      if (ref != 0) {
        refv *= igs::color::ref_value(ref, channels, r_max, ref_mode);
        ref += channels;
      }
      /* RGBのLevel処理した値をrefvの変化量比で加える */
      image_array[red] =
          static_cast<IT>(refchk_(static_cast<int>(image_array[red]),
                                  tables[red][image_array[red]], refv));
      image_array[gre] =
          static_cast<IT>(refchk_(static_cast<int>(image_array[gre]),
                                  tables[gre][image_array[gre]], refv));
      image_array[blu] =
          static_cast<IT>(refchk_(static_cast<int>(image_array[blu]),
                                  tables[blu][image_array[blu]], refv));
    }
  } else if (1 == channels) { /* grayscale */
    for (int ii = 0; ii < pixsize; ++ii, ++image_array) {
      double refv = 1.0;
      if (ref != 0) {
        refv *= igs::color::ref_value(ref, channels, r_max, ref_mode);
        ref += channels;
      }
      image_array[0] = static_cast<IT>(refchk_(
          static_cast<int>(image_array[0]), tables[0][image_array[0]], refv));
    }
  }

  /* 1 変換テーブルのメモリ解放 */
  tables.clear();
}

template <>
void change_(float *image_array, const int height, const int width,
             const int channels,
             const float *ref, /* 豎ゅａ繧狗判蜒・out)縺ｨ蜷後§鬮倥＆縲∝ｹ・√メ繝｣繝ｳ繝阪Ν謨ｰ */
             const int ref_mode,                            // R,G,B,A,luminance
             const double r_in_min, const double r_in_max,  // 0...1
             const double g_in_min, const double g_in_max,  // 0...1
             const double b_in_min, const double b_in_max,  // 0...1
             const double a_in_min, const double a_in_max,  // 0...1
             const double r_gamma,                          // 0.1 ... 10.0
             const double g_gamma,                          // 0.1 ... 10.0
             const double b_gamma,                          // 0.1 ... 10.0
             const double a_gamma,                          // 0.1 ... 10.0
             const double r_out_min, const double r_out_max,  // 0...1
             const double g_out_min, const double g_out_max,  // 0...1
             const double b_out_min, const double b_out_max,  // 0...1
             const double a_out_min, const double a_out_max,  // 0...1
             const bool clamp_sw, const bool alpha_sw,
             const bool add_blend_sw) {
  /* 1 譛螟ｧ蛟､縲∵怙蟆丞､縺九ｉ螟画鋤繝・・繝悶Ν繧呈ｱゅａ繧・*/
  std::vector<std::vector<float>> tables;
  const unsigned int table_size =
      std::numeric_limits<unsigned short>::max() + 1;
  const double div_val = static_cast<double>(table_size - 1);
  // const double mul_val = div_val + 0.999999;
  {
    using namespace igs::image::rgba;
    tables.resize(siz);
    tables[red].resize(table_size);
    tables[gre].resize(table_size);
    tables[blu].resize(table_size);
    tables[alp].resize(table_size);
    for (unsigned int yy = 0; yy < table_size; ++yy) {
      double rr, gg, bb, aa;
      rr = gg = bb = aa = yy / div_val;
      levels_(rr, r_in_min, r_in_max, r_gamma, r_out_min, r_out_max,
              false);  // 繧ｯ繝ｩ繝ｳ繝励＠縺ｪ縺・
      levels_(gg, g_in_min, g_in_max, g_gamma, g_out_min, g_out_max,
              false);  // 繧ｯ繝ｩ繝ｳ繝励＠縺ｪ縺・
      levels_(bb, b_in_min, b_in_max, b_gamma, b_out_min, b_out_max,
              false);  // 繧ｯ繝ｩ繝ｳ繝励＠縺ｪ縺・
      levels_(aa, a_in_min, a_in_max, a_gamma, a_out_min, a_out_max, clamp_sw);
      /* 0縲・.0 --> 0縲徇ul_max繧ｹ繧ｱ繝ｼ繝ｫ螟画鋤縺励∵紛謨ｰ蛟､蛹・*/
      tables[red][yy] = static_cast<float>(rr);
      tables[gre][yy] = static_cast<float>(gg);
      tables[blu][yy] = static_cast<float>(bb);
      tables[alp][yy] = static_cast<float>(aa);
    }
  }

  // 繝・・繝悶Ν蛟､繧定ｿ斐☆繝ｩ繝繝蠑上・n縺・繧医ｊ螟ｧ縺阪＞蝣ｴ蜷医・max縺ｮ邱壹ｒ逶ｴ邱壹〒蟒ｶ髟ｷ
  auto getTableVal = [&](int channel, float in) {
    if (in <= 0.f) {  // 蜈磯ｭ縺ｮ蛟､繧定ｿ斐☆
      return tables[channel][0];
    } else if (in <=
               1.f) {  // 繝・・繝悶Ν遽・峇縺ｫ蜿弱∪縺｣縺ｦ縺・ｋ蝣ｴ蜷医∝燕蠕後・蛟､縺ｧ邱壼ｽ｢陬憺俣
      int index   = static_cast<int>(std::floor(in * div_val));
      float ratio = in * div_val - static_cast<float>(index);
      if (areAlmostEqual(ratio, 0.)) return tables[channel][index];
      return tables[channel][index] * (1.f - ratio) +
             tables[channel][index + 1] * ratio;
    }
    // in縺・繧医ｊ螟ｧ縺阪＞蝣ｴ蜷・
    // 蛯ｾ縺・
    float dv =
        (tables[channel][table_size - 1] - tables[channel][table_size - 2]) *
        div_val;
    return tables[channel][table_size - 1] + dv * (in - 1.f);
  };

  /* 2 螟画鋤繝・・繝悶Ν繧剃ｽｿ縺｣縺ｦlevel螟画鋤縺吶ｋ */
  const int pixsize = height * width;
  if (igs::image::rgba::siz == channels) {
    using namespace igs::image::rgba;
    for (int ii = 0; ii < pixsize; ++ii, image_array += channels) {
      /* 螟牙喧驥丞・譛溷､ */
      double refv = 1.0;

      /* 蜿ら・逕ｻ蜒上≠繧後・繝斐け繧ｻ繝ｫ蜊倅ｽ阪・逕ｻ蜒丞､牙喧驥上ｒ蠕励ｋ */
      if (ref != 0) {
        refv *= igs::color::ref_value(ref, channels, 1.0, ref_mode);
        // clamp 0. to 1. in case the reference is more than 1
        refv = std::min(1., std::max(0., refv));
        ref += channels; /* continue;縺ｮ蜑阪↓陦後≧縺薙→ */
      }

      /* Alpha縺ｮLevel蜃ｦ逅・＠縺溷､繧池efv縺ｮ螟牙喧驥乗ｯ斐〒蜉縺医ｋ */
      if (alpha_sw) {
        image_array[alp] =
            refchk_(image_array[alp], getTableVal(alp, image_array[alp]), refv);
      }

      /* 蜉邂怜粋謌舌〒縲、lpha蛟､繧ｼ繝ｭ縺ｪ繧嘘GB蛟､繧定ｨ育ｮ励☆繧句ｿ・ｦ√・縺ｪ縺・*/
      if (add_blend_sw && areAlmostEqual(image_array[alp], 0.)) {
        continue;
      }
      /* 蜉邂怜粋謌舌〒縺ｪ縺就lpha蜷域・縺ｮ譎ゅ・縲、lpha蛟､縺後ぞ繝ｭ縺ｧ繧・
      RGB蛟､縺ｯ蟄伜惠縺吶ｋ(縺励※繧ゅｈ縺・縺ｮ險育ｮ励☆繧・*/

      /* 蜉邂怜粋謌舌〒縲√◎縺ｮ蛟､縺勲ax縺ｧ縺ｪ縺代ｌ縺ｰ螟牙喧驥上↓荵礼ｮ・*/
      if (add_blend_sw && (image_array[alp] < 1.f)) {
        refv *= static_cast<double>(image_array[alp]);
      }

      /* RGB縺ｮLevel蜃ｦ逅・＠縺溷､繧池efv縺ｮ螟牙喧驥乗ｯ斐〒蜉縺医ｋ */
      image_array[red] =
          refchk_(image_array[red], getTableVal(red, image_array[red]), refv);
      image_array[gre] =
          refchk_(image_array[gre], getTableVal(gre, image_array[gre]), refv);
      image_array[blu] =
          refchk_(image_array[blu], getTableVal(blu, image_array[blu]), refv);
    }
  } else if (igs::image::rgb::siz == channels) {
    using namespace igs::image::rgb;
    for (int ii = 0; ii < pixsize; ++ii, image_array += channels) {
      double refv = 1.0;
      if (ref != 0) {
        refv *= igs::color::ref_value(ref, channels, 1.0, ref_mode);
        ref += channels;
      }
      /* RGB縺ｮLevel蜃ｦ逅・＠縺溷､繧池efv縺ｮ螟牙喧驥乗ｯ斐〒蜉縺医ｋ */
      image_array[red] =
          refchk_(image_array[red], getTableVal(red, image_array[red]), refv);
      image_array[gre] =
          refchk_(image_array[gre], getTableVal(gre, image_array[gre]), refv);
      image_array[blu] =
          refchk_(image_array[blu], getTableVal(blu, image_array[blu]), refv);
    }
  } else if (1 == channels) { /* grayscale */
    for (int ii = 0; ii < pixsize; ++ii, ++image_array) {
      double refv = 1.0;
      if (ref != 0) {
        refv *= igs::color::ref_value(ref, channels, 1.0, ref_mode);
        ref += channels;
      }
      image_array[0] =
          refchk_(image_array[0], getTableVal(0, image_array[0]), refv);
    }
  }

  /* 1 螟画鋤繝・・繝悶Ν縺ｮ繝｡繝｢繝ｪ隗｣謾ｾ */
  tables.clear();
}

}  // namespace
//------------------------------------------------------------
void igs::levels::change(
    unsigned char *image_array, const int height, const int width,
    const int channels, const int bits

    ,
    const unsigned char *ref /* 求める画像と同じ高、幅、ch数 */
    ,
    const int ref_bits /* refがゼロのときはここもゼロ */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */

    ,
    const double r_in_min, const double r_in_max  // 0...1
    ,
    const double g_in_min, const double g_in_max  // 0...1
    ,
    const double b_in_min, const double b_in_max  // 0...1
    ,
    const double a_in_min, const double a_in_max  // 0...1
    ,
    const double r_gamma  // 0.1 ... 10.0
    ,
    const double g_gamma  // 0.1 ... 10.0
    ,
    const double b_gamma  // 0.1 ... 10.0
    ,
    const double a_gamma  // 0.1 ... 10.0
    ,
    const double r_out_min, const double r_out_max  // 0...1
    ,
    const double g_out_min, const double g_out_max  // 0...1
    ,
    const double b_out_min, const double b_out_max  // 0...1
    ,
    const double a_out_min, const double a_out_max  // 0...1

    ,
    const bool clamp_sw, const bool alpha_sw, const bool add_blend_sw) {
  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* grayscale */
  ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  if ((std::numeric_limits<unsigned char>::digits == bits) &&
      ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
       (0 == ref_bits))) {
    change_(image_array, height, width, channels, ref, ref_mode, r_in_min,
            r_in_max, g_in_min, g_in_max, b_in_min, b_in_max, a_in_min,
            a_in_max, r_gamma, g_gamma, b_gamma, a_gamma, r_out_min, r_out_max,
            g_out_min, g_out_max, b_out_min, b_out_max, a_out_min, a_out_max,
            clamp_sw, alpha_sw, add_blend_sw);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
              (0 == ref_bits))) {
    change_(reinterpret_cast<unsigned short *>(image_array), height, width,
            channels, ref, ref_mode, r_in_min, r_in_max, g_in_min, g_in_max,
            b_in_min, b_in_max, a_in_min, a_in_max, r_gamma, g_gamma, b_gamma,
            a_gamma, r_out_min, r_out_max, g_out_min, g_out_max, b_out_min,
            b_out_max, a_out_min, a_out_max, clamp_sw, alpha_sw, add_blend_sw);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_(reinterpret_cast<unsigned short *>(image_array), height, width,
            channels, reinterpret_cast<const unsigned short *>(ref), ref_mode,
            r_in_min, r_in_max, g_in_min, g_in_max, b_in_min, b_in_max,
            a_in_min, a_in_max, r_gamma, g_gamma, b_gamma, a_gamma, r_out_min,
            r_out_max, g_out_min, g_out_max, b_out_min, b_out_max, a_out_min,
            a_out_max, clamp_sw, alpha_sw, add_blend_sw);
  } else if ((std::numeric_limits<unsigned char>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    change_(image_array, height, width, channels,
            reinterpret_cast<const unsigned short *>(ref), ref_mode, r_in_min,
            r_in_max, g_in_min, g_in_max, b_in_min, b_in_max, a_in_min,
            a_in_max, r_gamma, g_gamma, b_gamma, a_gamma, r_out_min, r_out_max,
            g_out_min, g_out_max, b_out_min, b_out_max, a_out_min, a_out_max,
            clamp_sw, alpha_sw, add_blend_sw);
  } else if (std::numeric_limits<float>::digits == bits) {
    assert(std::numeric_limits<float>::digits == ref_bits || 0 == ref_bits);
    change_(reinterpret_cast<float *>(image_array), height, width, channels,
            reinterpret_cast<const float *>(ref), ref_mode, r_in_min, r_in_max,
            g_in_min, g_in_max, b_in_min, b_in_max, a_in_min, a_in_max, r_gamma,
            g_gamma, b_gamma, a_gamma, r_out_min, r_out_max, g_out_min,
            g_out_max, b_out_min, b_out_max, a_out_min, a_out_max, clamp_sw,
            alpha_sw, add_blend_sw);
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
}
