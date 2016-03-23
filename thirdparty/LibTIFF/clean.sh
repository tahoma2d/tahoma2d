# This script erases all intermediate and system-dependent files, while
# retaining the built library.

# The resulting folder can be submitted to version control.

cd tiff-4.0.3

make clean
rm -rf libtiff/.deps
rm -rf port/.deps
rm -rf test/.deps
rm -rf libs_i386
rm -rf libs_x86_64
rm -f  *.a
rm -f  *.dylib
rm -f  libtiff/tif_config.h
rm -f  libtiff/tiffconf.h
