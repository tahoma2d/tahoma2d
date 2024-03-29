#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <limits>
#include <cmath>
#include "igs_ifx_common.h" /* igs::image::rgba */
#include "igs_gaussian_blur.h"
#include "igs_gauss_distribution.cpp"

namespace {
const int diameter_from_radius_(const int radius) {
  /* テーブルの半径サイズ(=中心位置)からテーブルの大きさを決める */
  return radius * 2 + 1;
}
#if 0
template <class RT>
void blur_1st_hori_(
    const double **in_plane_with_margin  // &(std::vector<double *>).at(0)
    ,
    const int height_with_margin, const int width_with_margin,
    double *brush_sequence  // &(std::vector<double *>).at(0)
    ,
    const int int_radius,
    double **out_plane_with_margin  // &(std::vector<double *>).at(0)

    /* 参照画像用情報(no margin) */
    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */
    ,
    const int ref_channels, const double real_radius, const double sigma) {
  const int brush_diameter  = diameter_from_radius_(int_radius);
  const int width_no_margin = width_with_margin - int_radius * 2;
  const int r_max           = std::numeric_limits<RT>::max();
  const RT *ref_vert        = ref;
  const RT *ref_hori        = ref;
  double before_real_radius = -1.0;

  /* 縦方向 */
  for (int yo = 0; yo < height_with_margin; ++yo) {
    if (ref != 0) {
      if (int_radius < yo && yo < (height_with_margin - int_radius)) {
        ref_vert += width_no_margin * ref_channels;
      }
      ref_hori = ref_vert;
    }

    /* 横方向 */
    for (int xx = 0, xo = int_radius; xx < width_no_margin; ++xx, ++xo) {
      if (ref != 0) {
        const double read_r =
            real_radius *
            igs::color::ref_value(ref_hori, ref_channels, r_max, ref_mode);
        ref_hori += ref_channels;

        if (read_r != before_real_radius) {
          gauss_distribution_1d_(brush_sequence, brush_diameter,
                                 igs::gaussian_blur_hv::int_radius(read_r),
                                 read_r, sigma);
          before_real_radius = read_r;
        }
      }
      /* ガウス分布で横blur */
      double accum = 0;
      /*
for (int xb=0, xi=xx; xb<brush_diameter; ++xb,++xi) {
accum += in_plane_with_margin[yo][xi] * brush_sequence[xb];
}
*/
      const double *bru_seq = brush_sequence;
      int bru_dia           = brush_diameter;
      const double *inn_pla = &in_plane_with_margin[yo][xx];
      while (0 < bru_dia--) {
        accum += (*inn_pla++) * (*bru_seq++);
      }
      out_plane_with_margin[yo][xo] = accum;
    }
  }
}
#endif

void blur_1st_hori_(
    const double** in_plane_with_margin  // &(std::vector<double *>).at(0)
    ,
    const int height_with_margin, const int width_with_margin,
    double* brush_sequence  // &(std::vector<double *>).at(0)
    ,
    const int int_radius,
    double** out_plane_with_margin  // &(std::vector<double *>).at(0)

    /* 蜿ら・逕ｻ蜒冗畑諠・ｱ(no margin) */
    ,
    const float* ref /* 豎ゅａ繧狗判蜒・out)縺ｨ蜷後§鬮倥＆縲∝ｹ・√メ繝｣繝ｳ繝阪Ν謨ｰ */
    ,
    const double real_radius, const double sigma) {
  const int brush_diameter  = diameter_from_radius_(int_radius);
  const int width_no_margin = width_with_margin - int_radius * 2;
  // const int r_max = std::numeric_limits<RT>::max();
  const float* ref_vert     = ref;
  const float* ref_hori     = ref;
  double before_real_radius = -1.0;

  /* 邵ｦ譁ｹ蜷・*/
  for (int yo = 0; yo < height_with_margin; ++yo) {
    if (ref != nullptr) {
      if (int_radius < yo && yo < (height_with_margin - int_radius)) {
        ref_vert += width_no_margin;
      }
      ref_hori = ref_vert;
    }

    /* 讓ｪ譁ｹ蜷・*/
    for (int xx = 0, xo = int_radius; xx < width_no_margin; ++xx, ++xo) {
      if (ref != 0) {
        const double read_r = real_radius * (*ref_hori);
        ref_hori++;

        if (read_r != before_real_radius) {
          gauss_distribution_1d_(brush_sequence, brush_diameter,
                                 igs::gaussian_blur_hv::int_radius(read_r),
                                 read_r, sigma);
          before_real_radius = read_r;
        }
      }
      /* 繧ｬ繧ｦ繧ｹ蛻・ｸ・〒讓ｪblur */
      double accum = 0;

      const double* bru_seq = brush_sequence;
      int bru_dia           = brush_diameter;
      const double* inn_pla = &in_plane_with_margin[yo][xx];
      while (0 < bru_dia--) {
        accum += (*inn_pla++) * (*bru_seq++);
      }
      out_plane_with_margin[yo][xo] = accum;
    }
  }
}

#if 0
template <class RT>
void blur_2nd_vert_(
    const double **in_plane_with_margin  // &(std::vector<double *>).at(0)
    ,
    const int height_with_margin, const int width_with_margin,
    double *brush_sequence  // &(std::vector<double *>).at(0)
    ,
    const int int_radius,
    double **out_plane_with_margin  // &(std::vector<double *>).at(0)

    /* 参照画像用情報(no margin) */
    ,
    const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
    ,
    const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */
    ,
    const int ref_channels, const double real_radius, const double sigma) {
  const int brush_diameter   = diameter_from_radius_(int_radius);
  const int height_no_margin = height_with_margin - int_radius * 2;
  const int width_no_margin  = width_with_margin - int_radius * 2;
  const int r_max            = std::numeric_limits<RT>::max();
  const RT *ref_vert         = ref;
  const RT *ref_hori         = ref;
  double before_real_radius  = -1.0;

  /* 左右マージン部分はもう処理しなくていい */

  /* 横方向 */
  for (int xx = 0, xo = int_radius; xx < width_no_margin; ++xx, ++xo) {
    if (ref != 0) {
      ref_hori += ref_channels;
      ref_vert = ref_hori;
    }

    /* 縦方向 */
    for (int yy = 0, yo = int_radius; yy < height_no_margin; ++yy, ++yo) {
      if (ref != 0) {
        const double read_r =
            real_radius *
            igs::color::ref_value(ref_vert, ref_channels, r_max, ref_mode);
        ref_vert += width_no_margin * ref_channels;

        if (read_r != before_real_radius) {
          gauss_distribution_1d_(brush_sequence, brush_diameter,
                                 igs::gaussian_blur_hv::int_radius(read_r),
                                 read_r, sigma);
          before_real_radius = read_r;
        }
      }
      /* ガウス分布で横blur */
      double accum = 0;
      /*
for (int yb=0,yi=yy; yb<brush_diameter; ++yb,++yi) {
accum += in_plane_with_margin[yi][xo] * brush_sequence[yb];
}
*/
      const double *bru_seq = brush_sequence;
      int bru_dia           = brush_diameter;
      const double *inn_pla = &in_plane_with_margin[yy][xo];
      while (0 < bru_dia--) {
        accum += (*inn_pla) * (*bru_seq++);
        inn_pla += width_with_margin;
      }
      out_plane_with_margin[yo][xo] = accum;
    }
  }
}
#endif

void blur_2nd_vert_(
    const double** in_plane_with_margin  // &(std::vector<double *>).at(0)
    ,
    const int height_with_margin, const int width_with_margin,
    double* brush_sequence  // &(std::vector<double *>).at(0)
    ,
    const int int_radius,
    double** out_plane_with_margin  // &(std::vector<double *>).at(0)

    /* 蜿ら・逕ｻ蜒冗畑諠・ｱ(no margin) */
    ,
    const float* ref /* 豎ゅａ繧狗判蜒・out)縺ｨ蜷後§鬮倥＆縲∝ｹ・√メ繝｣繝ｳ繝阪Ν謨ｰ */
    ,
    const double real_radius, const double sigma) {
  const int brush_diameter   = diameter_from_radius_(int_radius);
  const int height_no_margin = height_with_margin - int_radius * 2;
  const int width_no_margin  = width_with_margin - int_radius * 2;
  // const int r_max = std::numeric_limits<RT>::max();
  const float* ref_vert     = ref;
  const float* ref_hori     = ref;
  double before_real_radius = -1.0;

  /* 蟾ｦ蜿ｳ繝槭・繧ｸ繝ｳ驛ｨ蛻・・繧ゅ≧蜃ｦ逅・＠縺ｪ縺上※縺・＞ */

  /* 讓ｪ譁ｹ蜷・*/
  for (int xx = 0, xo = int_radius; xx < width_no_margin; ++xx, ++xo) {
    if (ref != nullptr) {
      ref_hori++;
      ref_vert = ref_hori;
    }

    /* 邵ｦ譁ｹ蜷・*/
    for (int yy = 0, yo = int_radius; yy < height_no_margin; ++yy, ++yo) {
      if (ref != 0) {
        const double read_r = real_radius * (*ref_vert);
        ref_vert += width_no_margin;

        if (read_r != before_real_radius) {
          gauss_distribution_1d_(brush_sequence, brush_diameter,
                                 igs::gaussian_blur_hv::int_radius(read_r),
                                 read_r, sigma);
          before_real_radius = read_r;
        }
      }
      /* 繧ｬ繧ｦ繧ｹ蛻・ｸ・〒讓ｪblur */
      double accum          = 0;
      const double* bru_seq = brush_sequence;
      int bru_dia           = brush_diameter;
      const double* inn_pla = &in_plane_with_margin[yy][xo];
      while (0 < bru_dia--) {
        accum += (*inn_pla) * (*bru_seq++);
        inn_pla += width_with_margin;
      }
      out_plane_with_margin[yo][xo] = accum;
    }
  }
}

#if 0
template <class T>
void get_(const T *in, const int height, const int width, const int channels,
          const int current_ch, double **out  // &(std::vector<double *>).at(0)
          ) {
  in += current_ch;
  const double maxi = static_cast<double>(std::numeric_limits<T>::max());
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      out[yy][xx] = static_cast<double>(*in) / maxi;
      in += channels;
    }
  }
}
#endif

void get_(const float* in, const int height, const int width,
          const int channels, const int current_ch,
          double** out  // &(std::vector<double *>).at(0)
) {
  in += current_ch;
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      out[yy][xx] = static_cast<double>(*in);
      in += channels;
    }
  }
}

#if 0
template <class T>
void put_margin_(
    const double **in_with_margin  // &(std::vector<double *>).at(0)
    ,
    const int height_with_margin, const int width_with_margin,
    const int channels, const int current_ch, const int margin,
    T *out_no_margin) {
  out_no_margin += current_ch;
  const double maxi =
      static_cast<double>(std::numeric_limits<T>::max()) + 0.999999;
  for (int yy = margin; yy < (height_with_margin - margin); ++yy) {
    for (int xx = margin; xx < (width_with_margin - margin); ++xx) {
      *out_no_margin = static_cast<T>(in_with_margin[yy][xx] * maxi);
      out_no_margin += channels;
    }
  }
}
#endif

void put_margin_(
    const double** in_with_margin,  // &(std::vector<double *>).at(0)
    const int height_with_margin, const int width_with_margin,
    const int channels, const int current_ch, const int margin,
    float* out_no_margin) {
  out_no_margin += current_ch;
  for (int yy = margin; yy < (height_with_margin - margin); ++yy) {
    for (int xx = margin; xx < (width_with_margin - margin); ++xx) {
      *out_no_margin = in_with_margin[yy][xx];
      out_no_margin += channels;
    }
  }
}

#if 0
template <class T>
bool diff_between_channel_(const T *in, const int height, const int width,
                           const int channels, const int ch1, const int ch2) {
  // if (ch1 == ch2) { return false; }
  // if (channels <= ch1) { return false; }
  // if (channels <= ch2) { return false; }
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      if (in[ch1] != in[ch2]) {
        return true;
      }
      in += channels;
    }
  }
  return false;
}
#endif
bool diff_between_channel_(const float* in, const int height, const int width,
                           const int channels, const int ch1, const int ch2) {
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      if (in[ch1] != in[ch2]) {
        return true;
      }
      in += channels;
    }
  }
  return false;
}

#if 0
template <class IT, class RT>
void convert_hv_(const IT *in_with_margin, IT *out_no_margin,
                 const int height_with_margin, const int width_with_margin,
                 const int channels

                 ,
                 double *filter  // (double *)(&filter_buf.at(0))
                 ,
                 const int int_radius,
                 double **buffer_inn  // &(std::vector<double *>).at(0)
                 ,
                 double **buffer_out  // &(std::vector<double *>).at(0)

                 /* 参照画像用情報(no margin) */
                 ,
                 const RT *ref /* 求める画像(out)と同じ高さ、幅、チャンネル数 */
                 ,
                 const int ref_mode /* 0=R,1=G,2=B,3=A,4=Luminance,5=Nothing */
                 ,
                 const double real_radius, const double sigma) {
  bool diff_sw = true; /* 1番目の画像は処理する */
  for (int cc = 0; cc < channels; ++cc) {
    if (0 < cc) { /* 2番目のチャンネル以後 */
      /* 1つ前のチャンネルと違いを調べる */
      diff_sw = diff_between_channel_(in_with_margin, height_with_margin,
                                      width_with_margin, channels, cc - 1, cc);
    }
    /* 一つ前と同じ画像なら処理せず使い回して高速化する */
    if (diff_sw) {
      get_(in_with_margin, height_with_margin, width_with_margin, channels, cc,
           buffer_inn);
      blur_1st_hori_((const double **)(buffer_inn), height_with_margin,
                     width_with_margin, filter, int_radius, buffer_out

                     ,
                     ref, ref_mode, channels, real_radius, sigma);
      blur_2nd_vert_((const double **)(buffer_out), height_with_margin,
                     width_with_margin, filter, int_radius, buffer_inn

                     ,
                     ref, ref_mode, channels, real_radius, sigma);
    }

    put_margin_((const double **)(buffer_inn), height_with_margin,
                width_with_margin, channels, cc, int_radius, out_no_margin);
  }
}
#endif

void convert_hv_(
    const float* in_with_margin, float* out_no_margin,
    const int height_with_margin, const int width_with_margin,
    const int channels,
    double* filter,  // (double *)(&filter_buf.at(0))
    const int int_radius,
    double** buffer_inn,  // &(std::vector<double *>).at(0)
    double** buffer_out,  // &(std::vector<double *>).at(0)
    /* 蜿ら・逕ｻ蜒冗畑諠・ｱ(no margin) */
    const float* ref, /* 豎ゅａ繧狗判蜒・out)縺ｨ蜷後§鬮倥＆縲∝ｹ・√メ繝｣繝ｳ繝阪Ν謨ｰ */
    const double real_radius, const double sigma) {
  bool diff_sw = true; /* 1逡ｪ逶ｮ縺ｮ逕ｻ蜒上・蜃ｦ逅・☆繧・*/
  for (int cc = 0; cc < channels; ++cc) {
    if (0 < cc) { /* 2逡ｪ逶ｮ縺ｮ繝√Ε繝ｳ繝阪Ν莉･蠕・*/
      /* 1縺､蜑阪・繝√Ε繝ｳ繝阪Ν縺ｨ驕輔＞繧定ｪｿ縺ｹ繧・*/
      diff_sw = diff_between_channel_(in_with_margin, height_with_margin,
                                      width_with_margin, channels, cc - 1, cc);
    }
    /* 荳縺､蜑阪→蜷後§逕ｻ蜒上↑繧牙・逅・○縺壻ｽｿ縺・屓縺励※鬮倬溷喧縺吶ｋ */
    if (diff_sw) {
      get_(in_with_margin, height_with_margin, width_with_margin, channels, cc,
           buffer_inn);
      blur_1st_hori_((const double**)(buffer_inn), height_with_margin,
                     width_with_margin, filter, int_radius, buffer_out, ref,
                     real_radius, sigma);
      blur_2nd_vert_((const double**)(buffer_out), height_with_margin,
                     width_with_margin, filter, int_radius, buffer_inn, ref,
                     real_radius, sigma);
    }

    put_margin_((const double**)(buffer_inn), height_with_margin,
                width_with_margin, channels, cc, int_radius, out_no_margin);
  }
}

}  // namespace
const int igs::gaussian_blur_hv::int_radius(const double real_radius) {
  /* ぼかしの半径から、pixelサイズ半径(=中心位置=margin)を決める */
  return static_cast<int>(ceil(real_radius));
}
const int igs::gaussian_blur_hv::buffer_bytes(const int height_with_margin,
                                              const int width_with_margin,
                                              const int int_radius) {
  const int int_diameter = diameter_from_radius_(int_radius);
  return int_diameter * int_diameter * sizeof(double) +
         height_with_margin * width_with_margin * sizeof(double) +
         height_with_margin * width_with_margin * sizeof(double);
}
void igs::gaussian_blur_hv::convert(
    /* 入出力画像 */
    const float* in_with_margin, float* out_no_margin,
    const int height_with_margin, const int width_with_margin,
    const int channels,
    /* Pixel毎に効果の強弱 */
    const float* ref, /* 豎ゅａ繧狗判蜒・out)縺ｨ蜷後§鬮倥∝ｹ・…h謨ｰ */
    /* 計算バッファ */
    void* buffer,
    int buffer_bytes,  // Must be igs::gaussian_blur_hv::buffer_bytes(-)
    /* Action Geometry */
    const int int_radius,                         // =margin
    const double real_radius, const double sigma  //= 0.25
) {
  /* 引数チェック */
  if (real_radius <= 0.0) {
    return;
  }

  if ((igs::image::rgba::siz != channels) &&
      (igs::image::rgb::siz != channels) && (1 != channels) /* grayscale */
  ) {
    throw std::domain_error("Bad channels,Not rgba/rgb/grayscale");
  }

  /* 変数の設定 */
  const int int_diameter = diameter_from_radius_(int_radius);
  double* double_buffer  = static_cast<double*>(buffer);

  /* メモリバッファの設定 */
  std::vector<double> filter_buf(int_diameter);

  std::vector<double*> in_plane_with_margin_dp(height_with_margin);
  for (int yy = 0; yy < height_with_margin; ++yy) {
    in_plane_with_margin_dp.at(yy) = double_buffer;
    double_buffer += width_with_margin;
    buffer_bytes -= width_with_margin * sizeof(double);
    if (buffer_bytes <= 0) {
      std::string msg("buffer_inn is empty");
      throw std::domain_error(msg);
    }
  }

  std::vector<double*> out_plane_with_margin_dp(height_with_margin);
  for (int yy = 0; yy < height_with_margin; ++yy) {
    out_plane_with_margin_dp.at(yy) = double_buffer;
    double_buffer += width_with_margin;
    buffer_bytes -= width_with_margin * sizeof(double);
    if (buffer_bytes <= 0) {
      std::string msg("buffer_out is empty");
      throw std::domain_error(msg);
    }
  }

  /* filter生成 */
  gauss_distribution_1d_(&filter_buf.at(0), int_diameter, int_radius,
                         real_radius, sigma);

  convert_hv_(in_with_margin, out_no_margin, height_with_margin,
              width_with_margin, channels, &filter_buf.at(0), int_radius,
              &in_plane_with_margin_dp.at(0), &out_plane_with_margin_dp.at(0),
              ref, real_radius, sigma);
  /*
  if ((std::numeric_limits<unsigned char>::digits == bits) &&
      ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
       (0 == ref_bits))) {
    convert_hv_(static_cast<const unsigned char *>(in_with_margin),
                static_cast<unsigned char *>(out_no_margin), height_with_margin,
                width_with_margin, channels, &filter_buf.at(0), int_radius,
                &in_plane_with_margin_dp.at(0), &out_plane_with_margin_dp.at(0)

                                                    ,
                ref, ref_mode, real_radius, sigma);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             ((std::numeric_limits<unsigned char>::digits == ref_bits) ||
              (0 == ref_bits))) {
    convert_hv_(static_cast<const unsigned short *>(in_with_margin),
                static_cast<unsigned short *>(out_no_margin),
                height_with_margin, width_with_margin, channels,
                &filter_buf.at(0), int_radius, &in_plane_with_margin_dp.at(0),
                &out_plane_with_margin_dp.at(0)

                    ,
                ref, ref_mode, real_radius, sigma);
  } else if ((std::numeric_limits<unsigned short>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    convert_hv_(static_cast<const unsigned short *>(in_with_margin),
                static_cast<unsigned short *>(out_no_margin),
                height_with_margin, width_with_margin, channels,
                &filter_buf.at(0), int_radius, &in_plane_with_margin_dp.at(0),
                &out_plane_with_margin_dp.at(0)

                    ,
                reinterpret_cast<const unsigned short *>(ref), ref_mode,
                real_radius, sigma);
  } else if ((std::numeric_limits<unsigned char>::digits == bits) &&
             (std::numeric_limits<unsigned short>::digits == ref_bits)) {
    convert_hv_(static_cast<const unsigned short *>(in_with_margin),
                static_cast<unsigned short *>(out_no_margin),
                height_with_margin, width_with_margin, channels,
                &filter_buf.at(0), int_radius, &in_plane_with_margin_dp.at(0),
                &out_plane_with_margin_dp.at(0)

                    ,
                reinterpret_cast<const unsigned char *>(ref), ref_mode,
                real_radius, sigma);
  } else {
    throw std::domain_error("Bad bits,Not uchar/ushort");
  }
  */
}
