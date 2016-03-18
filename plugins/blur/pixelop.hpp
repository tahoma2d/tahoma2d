#if !defined(PIXEL_OP_HPP__)
#define PIXEL_OP_HPP__

#include <functional>

template <typename T, int component>
void hv_op(T *__restrict ptr, const T *__restrict src, int w, int h, int dst_stride, int src_stride, std::function<T(T &&, T &&, T &&)> &&f)
{
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			T c0 = src[j * component];
			T c1 = src[j * component + 1];
			T c2 = src[j * component + 2];
			ptr[j] = f(std::move(c0), std::move(c1), std::move(c2));
		}
		src += src_stride;
		ptr += dst_stride;
	}
}

template <typename T, int component>
void hv_op(const T *__restrict src, int w, int h, int stride, std::function<void(T &&, T &&, T &&, int, int)> &&f)
{
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			T c0 = src[j * component];
			T c1 = src[j * component + 1];
			T c2 = src[j * component + 2];
			f(std::move(c0), std::move(c1), std::move(c2), j, i);
		}
		src += stride;
	}
}

template <typename T>
void hv_op(T *__restrict ptr, const T *__restrict src, int w, int h, int stride, std::function<T(T &&, int x, int y)> &&f)
{
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			T c0 = src[j];
			ptr[j] = f(std::move(c0), j, i);
		}
		src += stride;
		ptr += stride;
	}
}

template <typename T>
void hv_kernel(T *__restrict ptr, const T *__restrict src, int w, int h, int stride, std::function<T(const T *, int, int, int)> &&f)
{
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			ptr[j] = f(src + j, stride, j, i);
		}
		src += stride;
		ptr += stride;
	}
}

template <typename T, int components>
void hv_kernel(T *__restrict ptr, const T *__restrict src, int w, int h, int dstride, int sstride, std::function<void(T[components], const T *, int, int, int)> &&f)
{
	for (int i = 0; i < h * components; i += components) {
		for (int j = 0; j < w * components; j += components) {
			T channels[components];
			f(channels, src + j, sstride, j, i);
			for (int k = 0; k < components; k++)
				ptr[j + k] = channels[k];
		}
		src += sstride;
		ptr += dstride;
	}
}

#endif
