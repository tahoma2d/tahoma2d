#include "igs_density.h"
#include <iostream>
namespace {
float accum_by_trans_(const float src_value /* 蜈・､(R,G,B) */
                      ,
                      const float transparent /* src_value縺ｮ騾乗・蠎ｦ(0...1) */
                      ,
                      const int integer_part /* 豼・ｺｦ蛟､縺ｮ謨ｴ謨ｰ驛ｨ蛻・0...) */
                      ,
                      const double fractional_part /* 豼・ｺｦ蛟､縺ｮ蟆第焚驛ｨ蛻・0...1) */
) {
  float accumulation = src_value;
  if (1 <= integer_part) {
    for (int ii = 1; ii < integer_part; ++ii) {
      accumulation = accumulation * transparent + src_value;
    }
    if (0.f < fractional_part) {
      accumulation +=
          ((accumulation * transparent + src_value) - accumulation) *
          fractional_part;
    }
  } else { /* 整数部分がゼロ以下のとき */
    if (0.f < fractional_part) {
      accumulation *= fractional_part;
    } else { /* 少数部分もゼロ以下のとき */
      accumulation = 0.f;
    }
  }
  return (1.f < accumulation) ? 1.f
                              : ((accumulation < 0.f) ? 0.f : accumulation);
}
}  // namespace
#include <limits>           /* std::numeric_limits */
#include "igs_ifx_common.h" /* igs::image::rgba */
namespace {
void change_(float *image_array, const int height, const int width,
             const int channels, const float *ref, const double density) {
  const int integer_part       = (int)density;
  const double fractional_part = density - (int)density;

  using namespace igs::image::rgba;
  const int pixsize = height * width;
  for (int ii = 0; ii < pixsize; ++ii, image_array += channels) {
    const float rr1 = image_array[red];
    const float gg1 = image_array[gre];
    const float bb1 = image_array[blu];
    const float aa1 = image_array[alp];

    float rr2 = accum_by_trans_(rr1, 1.0 - aa1, integer_part, fractional_part);
    float gg2 = accum_by_trans_(gg1, 1.0 - aa1, integer_part, fractional_part);
    float bb2 = accum_by_trans_(bb1, 1.0 - aa1, integer_part, fractional_part);
    float aa2 = accum_by_trans_(aa1, 1.0 - aa1, integer_part, fractional_part);

    /* 参照画像あればピクセル単位の画像変化 */
    if (ref != nullptr) {
      rr2 = (rr2 - rr1) * (*ref) + rr1;
      gg2 = (gg2 - gg1) * (*ref) + gg1;
      bb2 = (bb2 - bb1) * (*ref) + bb1;
      aa2 = (aa2 - aa1) * (*ref) + aa1;
      ref++;
    }

    image_array[red] = rr2;
    image_array[gre] = gg2;
    image_array[blu] = bb2;
    image_array[alp] = aa2;
  }
}
}  // namespace
#include <stdexcept> /* std::domain_error(-) */
void igs::density::change(float *image_array, const int height, const int width,
                          const int channels, /* 4(=RGBA縺ｧ縺ｪ縺代ｌ縺ｰ縺ｪ繧峨↑縺・ */
                          const float *ref, const double density) {
  if (igs::image::rgba::siz != channels) {
    throw std::domain_error("Bad channels,Not rgba");
  }

  change_(image_array, height, width, channels, ref, density);
}
