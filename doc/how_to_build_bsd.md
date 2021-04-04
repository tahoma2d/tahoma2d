# Setting up the development environment for BSD Operating Systems such as NetBSD, FreeBSD, Dragonfly BSD, and OpenBSD.

## Required software

Building Tahoma2D from source requires the following dependencies:
- Git
- GCC or Clang
- CMake (3.4.1 or newer).
- Qt5 (5.9 or newer)
- Boost (1.55 or newer)
- LibPNG
- SuperLU
- Lzo2
- FreeType
- LibMyPaint (1.3 or newer)
- Jpeg-Turbo (1.4 or newer)
- OpenCV 3.2 or newer

### Dependencies example with OpenBSD

```
# pkg_add git cmake freeglut3 boost freetype6 glew glib2 jpeg json-c-dev lz4 lzo2 png pkg-conf qt5
```
It's possible we also need `libgsl2` (or maybe `libopenblas-dev`). `libgsl2` is called just gsl in BSD and `libopenblas-dev` is just blas.

Packages such as SuperLU, libegl1-mesa-dev, libgles2-mesa-dev, or Jpeg-Turbo might need to be installed manually by compiling them.

After all dependencies are met, continue the linux instructions as stated in [here.](https://github.com/jointri/tahoma2d/edit/master/doc/how_to_build_linux.md)
