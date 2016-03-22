
#ifndef TIF_GETIMAGE_64_H
#define TIF_GETIMAGE_64_H

#include "tiff.h"
#include "tiffio.h"
#include <stdint.h>
typedef uint64_t uint64;

/* \file libtiff_64.h

  LibTIFF reads multiple bpp formats - but transfers data to 32-bit images
  by default. This file includes support to retrieve data in 64-bit buffers,
  which must be manually provided.
*/

//**************************************************************************
//    64-bit to 64-bit  TIFF support
//**************************************************************************

int TIFFRGBAImageBegin_64(TIFFRGBAImage *img, TIFF *tif, int stop, char emsg[1024]);
int TIFFRGBAImageGet_64(TIFFRGBAImage *img, uint64 *raster, uint32 w, uint32 h);
int TIFFReadRGBAStrip_64(TIFF *tif, uint32 row, uint64 *raster);
int TIFFReadRGBATile_64(TIFF *tif, uint32 x, uint32 y, uint64 *raster);

#endif // LIBTIFF_64_H
