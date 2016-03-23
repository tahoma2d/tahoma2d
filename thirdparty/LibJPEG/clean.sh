# This script erases all intermediate and system-dependent files, while
# retaining the built library.

# The resulting folder can be submitted to version control.


cd jpeg-9

make clean
rm -rf .deps
rm -rf libs_x86_64
rm -rf libs_i386
rm -f  *.a
rm -f  *.dylib
rm -f  jconfig.h