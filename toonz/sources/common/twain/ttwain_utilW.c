

#include <windows.h>
#include "assert.h"

#include "ttwainP.h"
#include "ttwain_utilPD.h"
#include "ttwain_global_def.h"

#ifdef __cplusplus
extern "C" {
#endif

static int getColorCount(LPBITMAPINFOHEADER pbi)
{
	if (pbi->biSize == sizeof(BITMAPCOREHEADER)) {
		LPBITMAPCOREHEADER lpbc = ((LPBITMAPCOREHEADER)pbi);
		return 1 << lpbc->bcBitCount;
	} else if (pbi->biClrUsed == 0)
		return 0xFFF & (1 << pbi->biBitCount);
	else
		return (int)pbi->biClrUsed;
}
/*---------------------------------------------------------------------------*/
static size_t BmiColorTableBytes(LPBITMAPINFOHEADER pbi)
{
	return getColorCount(pbi) * sizeof(RGBQUAD);
}
/*---------------------------------------------------------------------------*/
static LPBYTE get_buffer(LPBITMAPINFOHEADER lpbi, int *iud)
{
	LPBYTE buf = (LPBYTE)lpbi + lpbi->biSize + BmiColorTableBytes(lpbi);
	*iud = (lpbi->biHeight > 0);
	return buf;
}
/*---------------------------------------------------------------------------*/
#define PelsPerMeter2DPI(ppm) (((float)ppm * 2.54) / 100.0)
/*---------------------------------------------------------------------------*/
int TTWAIN_Native2RasterPD(void *handle, void *the_ras, int *lx, int *ly)
{
	assert(!"DAFARE");
#if 0
LPBITMAPINFOHEADER lpBIH;
LPBYTE buffer;
int is_upside_down;
int ras_type = 0;
int linesize;
float ras_bpp = 0;
TUINT32 rebound;
UCHAR *ras_out;
int y,x;
float xDpi, yDpi;
DWORD bitmap_size;
int extraX, extraY, extraByteX;
TUINT32 tot_rebound = 0;

RASTER *ras = (RASTER *) the_ras;
if (!handle)
  return FALSE;
/*DAFARE
if (!ras)
  return FALSE;
*/
lpBIH = (LPBITMAPINFOHEADER) GLOBAL_LOCK(handle);

extraX = (int)lpBIH->biWidth - ras->lx;
extraY = (int)lpBIH->biHeight- ras->ly;

*lx = lpBIH->biWidth ;
*ly = lpBIH->biHeight;

assert(extraX>=0);
assert(extraY>=0);

xDpi = PelsPerMeter2DPI(lpBIH->biXPelsPerMeter);
yDpi = PelsPerMeter2DPI(lpBIH->biYPelsPerMeter);

switch (lpBIH->biBitCount)
  {
  CASE 1:
    ras_type = RAS_BW;
    ras_bpp  = 1.0/8.0;
  CASE 8:
    ras_type = RAS_GR8;
    ras_bpp  = 1;
  CASE 24:
    ras_type = RAS_RGB;
    ras_bpp  = 3;
  DEFAULT:
    assert(0);
  }
extraByteX = CEIL(extraX * ras_bpp);
bitmap_size = ((((lpBIH->biWidth * lpBIH->biBitCount) + 31) & ~31) >> 3) * lpBIH->biHeight;
/* cfr. DIBs and Their Use
   Ron Gery
   Microsoft Developer Network Technology Group
*/
ras_out = ras->buffer;

buffer = 0;
buffer = get_buffer(lpBIH, &is_upside_down);

if (is_upside_down)
  {
  ras_out = ras->buffer;
  if (!TTwainData.transferInfo.nextImageNeedsToBeInverted)
    {
    switch (ras_type)
      {
      CASE RAS_GR8:
      __OR RAS_RGB:
      __OR RAS_BW:
      linesize = CEIL(ras->lx * ras_bpp);
      for (y =0; y<ras->ly; y++, ras_out += linesize)
        {
        memcpy(ras_out, buffer, linesize);
        buffer += linesize;
        buffer += extraByteX;
        rebound = (TUINT32) buffer;
        rebound = 4 - (rebound %4);
        tot_rebound +=rebound;
        buffer += rebound;
        }
      }
    }
  else
    {
    switch (ras_type)
      {
      CASE RAS_BW:
        linesize = CEIL(ras->lx * ras_bpp);
        for (y =0; y<ras->ly; y++, ras_out)
          {
          for (x = 0; x<linesize; x++)
            {
            UCHAR eight_bit;
            eight_bit = *buffer;
            *ras_out = (UCHAR) ~eight_bit;
            buffer++;
            ras_out++;
            }
          buffer += extraByteX;
          rebound = (TUINT32) buffer;
          rebound = 4 - (rebound %4);
          buffer += rebound;
          }
      DEFAULT:
        assert(0); /*only BW images may need to be inverted*/
      }
    }
  }
else
  {
  assert(!"Not IMPL!");
  }
GLOBAL_UNLOCK(handle);
#ifdef _DEBUG
{
char msg[1024];
sprintf(msg,"tot rebounded byte=%d\n", tot_rebound);
OutputDebugString(msg);
if (lpBIH->biSize)
  {
  assert((lpBIH->biSizeImage-CEIL(ras->lx*ras->ly*ras_bpp))==tot_rebound);
  }
}
#endif
return TRUE;
#endif
	return 1;
}

#ifdef __cplusplus
}
#endif
