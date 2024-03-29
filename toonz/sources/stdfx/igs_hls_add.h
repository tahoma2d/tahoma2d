#pragma once

#ifndef igs_add_hls_h
#define igs_add_hls_h

#ifndef IGS_HLS_ADD_EXPORT
#define IGS_HLS_ADD_EXPORT
#endif

namespace igs {
namespace hls_add {
IGS_HLS_ADD_EXPORT void change(
    float *image_array, const int height, const int width, const int channels,
    const float *noi_image_array,
    const float *ref,    /* 豎ゅａ繧狗判蜒上→蜷後§鬮倥∝ｹ・…hannels謨ｰ */
    const int xoffset,   /* 0	INT_MIN ... INT_MAX	*/
    const int yoffset,   /* 0	INT_MIN ... INT_MAX	*/
    const int from_rgba, /* 0	0(R),1(G),2(B),3(A)	*/
    const double offset, /* 0.5	-1.0 ... 1.0	*/
    const double hue_scale, /* 0.0	-1.0 ... 1.0	*/
    const double lig_scale, /* 0.5	-1.0 ... 1.0	*/
    const double sat_scale, /* 0.0	-1.0 ... 1.0	*/
    const double alp_scale, /* 0.0	-1.0 ... 1.0	*/
    const bool add_blend_sw,
    /* 効果(変化量)をアルファブレンドするか否かのスイッチ
                add_blend_sw == true
                    アルファ値によりRGBの変化量を調整する
                    合成方法が
                            合成画 = 下絵 * (1 - alpha) + 上絵
                    の場合こちらを使う
                add_blend_sw == false
                    アルファ値に関係なくRGBが変化する
                    合成方法が
                            合成画 = 下絵 * (1 - alpha) + 上絵 * alpha
                    の場合こちらを使う
            */
    const bool cylindrical = true  // colorspace shape: cylindrical or bicone
);
}
}  // namespace igs
#endif /* !igs_add_hls_h */
