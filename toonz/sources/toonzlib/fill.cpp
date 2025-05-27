

#include "trastercm.h"
#include "toonz/fill.h"
#include "toonz/ttilesaver.h"
#include "tpalette.h"
#include "tpixelutils.h"
#include "toonz/autoclose.h"
#include "tenv.h"
#include "tropcm.h"

#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"

#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"

#include <stack>

#include <QDebug>

#include "tsystem.h"

extern TEnv::DoubleVar AutocloseDistance;
extern TEnv::DoubleVar AutocloseAngle;
extern TEnv::IntVar AutocloseInk;
extern TEnv::IntVar AutocloseOpacity;

#define IGNORECOLORSTYLE 4093
#define GAP_CLOSE_TEMP 4094
#define GAP_CLOSE_USED 4095

//-----------------------------------------------------------------------------
namespace {  // Utility Function
//-----------------------------------------------------------------------------

inline TPoint nearestInkNotDiagonal(const TRasterCM32P &r, const TPoint &p) {
  TPixelCM32 *buf = (TPixelCM32 *)r->pixels(p.y) + p.x;

  if (p.x < r->getLx() - 1 && (!(buf + 1)->isPurePaint()))
    return TPoint(p.x + 1, p.y);

  if (p.x > 0 && (!(buf - 1)->isPurePaint())) return TPoint(p.x - 1, p.y);

  if (p.y < r->getLy() - 1 && (!(buf + r->getWrap())->isPurePaint()))
    return TPoint(p.x, p.y + 1);

  if (p.y > 0 && (!(buf - r->getWrap())->isPurePaint()))
    return TPoint(p.x, p.y - 1);

  return TPoint(-1, -1);
}

//
// from point x, y expands to the right and left.
// the redrawn line goes from* xa to* xb inclusive
// x1 <= *xa <= *xb <= x2
// N.B. if not even one pixel is drawn* xa > * xb
//
// "prevailing" is set to false on revert-filling the border of
// region in the Rectangular, Freehand and Polyline fill procedures
// in order to make the paint extend behind the line.

// Calculates the endpoints for the line of pixels in which to fill
bool calcFillRow(const TRasterCM32P &r, const TPoint &p, int &xa, int &xb,
                 int paint, TPalette *palette, bool prevailing = true,
                 bool emptyOnly = false) {
  int tone, oldtone;
  TPixelCM32 *pix, *pix0, *limit, *tmp_limit;

  /* vai a destra */
  TPixelCM32 *line = r->pixels(p.y);

  pix0    = line + p.x;
  pix     = pix0;
  limit   = line + r->getBounds().x1;
  oldtone = pix->getTone();
  tone    = oldtone;
  for (; pix <= limit; pix++) {
    if (pix->getPaint() == paint) {
        //break;
    }
    if (emptyOnly && pix->getPaint() != 0) {
        //break;
    }
    tone = pix->getTone();
    if (tone == 0) {
        break;
    }
    // prevent fill area from protruding behind the colored line
    if (tone > oldtone) {
      // not-yet-colored line case
      if (prevailing && !pix->isPurePaint() && pix->getInk() != pix->getPaint())
        break;
      while (pix != pix0) {
        // iterate back in order to leave the pixel with the lowest tone
        // unpainted
        pix--;
        // make the one-pixel-width semi-transparent line to be painted
        if (prevailing && pix->getInk() != pix->getPaint()) break;
        if (pix->getTone() > oldtone) {
          // check if the current pixel is NOT with the lowest tone among the
          // vertical neighbors as well
          if (p.y > 0 && p.y < r->getLy() - 1) {
            TPixelCM32 *upPix   = pix - r->getWrap();
            TPixelCM32 *downPix = pix + r->getWrap();
            if (upPix->getTone() > pix->getTone() &&
                downPix->getTone() > pix->getTone()) continue;
          }
          break;
        }
      }
      pix++;
      break;
    }
    oldtone = tone;
  }
  if (tone == 0) {
    tmp_limit = pix + 10;  // edge stop fill == 10 per default
    if (limit > tmp_limit) limit = tmp_limit;
    for (; pix <= limit; pix++) {
      //if (pix->getPaint() == paint) break; // commented out for issue 1151
      if (pix->getTone() != 0) {
          break;
      }
    }
  }

  xb = p.x + pix - pix0 - 1; //go backward one pixel from the current pixel which triggered the boundary condition.

  /* go left */

  pix     = pix0;
  limit   = line + r->getBounds().x0;
  oldtone = pix->getTone();
  tone    = oldtone;
  for (pix--; pix >= limit; pix--) {
      if (pix->getPaint() == paint) { 
          break; 
      }
      if (emptyOnly && pix->getPaint() != 0) {
          break; 
      }
    tone = pix->getTone();
    if (tone == 0) {
        break;
    }
    // prevent fill area from protruding behind the colored line
    if (tone > oldtone) {
      // not-yet-colored line case
        if (prevailing && !pix->isPurePaint() && pix->getInk() != pix->getPaint()){
            break;
        }
      while (pix != pix0) {
        // iterate forward in order to leave the pixel with the lowest tone
        // unpainted
        pix++;
        // make the one-pixel-width semi-transparent line to be painted
        if (prevailing && pix->getInk() != pix->getPaint()) break;
        if (pix->getTone() > oldtone) {
          // check if the current pixel is NOT with the lowest tone among the
          // vertical neighbors as well
          if (p.y > 0 && p.y < r->getLy() - 1) {
            TPixelCM32 *upPix   = pix - r->getWrap();
            TPixelCM32 *downPix = pix + r->getWrap();
            if (upPix->getTone() > pix->getTone() &&
                downPix->getTone() > pix->getTone()) {
                continue;
            }
          }
          break;
        }
      }
      pix--;
      break;
    }
    oldtone = tone;
  }
  if (tone == 0) {
    tmp_limit = pix - 10;
    if (limit < tmp_limit) limit = tmp_limit;
    for (; pix >= limit; pix--) {
      //if (pix->getPaint() == paint) break; // commented out for issue 1151
        if (pix->getTone() != 0) { 
            break; 
        }
    }
  }

  xa = p.x + pix - pix0 + 1; //go backward one pixel from the current pixel which triggered the boundary condition.

  return (xb >= xa);
}

//-----------------------------------------------------------------------------

void findSegment(const TRaster32P &r, const TPoint &p, int &xa, int &xb,
                 const TPixel32 &color, const int fillDepth = 254) {
  int matte, oldmatte;
  TPixel32 *pix, *pix0, *limit, *tmp_limit;

  /* vai a destra */
  TPixel32 *line = r->pixels(p.y);

  pix0     = line + p.x;
  pix      = pix0;
  limit    = line + r->getBounds().x1;
  oldmatte = pix->m;
  matte    = oldmatte;
  for (; pix <= limit; pix++) {
    if (*pix == color) break;
    matte = pix->m;
    if (matte < oldmatte || matte > fillDepth) break;
    oldmatte = matte;
  }
  if (matte == 0) {
    tmp_limit = pix + 10;  // edge stop fill == 10 per default
    if (limit > tmp_limit) limit = tmp_limit;
    for (; pix <= limit; pix++) {
      if (*pix == color) break;
      if (pix->m != 255) break;
    }
  }
  xb = p.x + pix - pix0 - 1;

  /* vai a sinistra */
  pix      = pix0;
  limit    = line + r->getBounds().x0;
  oldmatte = pix->m;
  matte    = oldmatte;
  for (; pix >= limit; pix--) {
    if (*pix == color) break;
    matte = pix->m;
    if (matte < oldmatte || matte > fillDepth) break;
    oldmatte = matte;
  }
  if (matte == 0) {
    tmp_limit = pix - 10;
    if (limit < tmp_limit) limit = tmp_limit;
    for (; pix >= limit; pix--) {
      if (*pix == color) break;
      if (pix->m != 255) break;
    }
  }
  xa = p.x + pix - pix0 + 1;
}

//-----------------------------------------------------------------------------
// Used when the clicked pixel is solid or semi-transparent.
// Check if the fill is stemmed at the target pixel.
// Note that RGB values are used for checking the difference, not Alpha value.

bool doesStemFill(const TPixel32 &clickColor, const TPixel32 *targetPix,
                  const int fillDepth2) {
  // stop if the target pixel is transparent
  if (targetPix->m == 0) return true;
  // check difference of RGB values is larger than fillDepth
  int dr = (int)clickColor.r - (int)targetPix->r;
  int dg = (int)clickColor.g - (int)targetPix->g;
  int db = (int)clickColor.b - (int)targetPix->b;
  return (dr * dr + dg * dg + db * db) >
         fillDepth2;  // condition for "stem" the fill
}

//-----------------------------------------------------------------------------

void fullColorFindSegment(const TRaster32P &r, const TPoint &p, int &xa,
                          int &xb, const TPixel32 &color,
                          const TPixel32 &clickedPosColor,
                          const int fillDepth) {
  if (clickedPosColor.m == 0) {
    findSegment(r, p, xa, xb, color, fillDepth);
    return;
  }

  TPixel32 *pix, *pix0, *limit;
  // check to the right
  TPixel32 *line = r->pixels(p.y);

  pix0  = line + p.x;  // seed pixel
  pix   = pix0;
  limit = line + r->getBounds().x1;  // right end

  TPixel32 oldPix = *pix;

  int fillDepth2 = fillDepth * fillDepth;

  for (; pix <= limit; pix++) {
    // break if the target pixel is with the same as filling color
    if (*pix == color) break;
    // continue if the target pixel is the same as the previous one
    if (*pix == oldPix) continue;

    if (doesStemFill(clickedPosColor, pix, fillDepth2)) break;

    // store pixel color in case if the next pixel is with the same color
    oldPix = *pix;
  }
  xb = p.x + pix - pix0 - 1;

  // check to the left
  pix    = pix0;                      // seed pixel
  limit  = line + r->getBounds().x0;  // left end
  oldPix = *pix;
  for (; pix >= limit; pix--) {
    // break if the target pixel is with the same as filling color
    if (*pix == color) break;
    // continue if the target pixel is the same as the previous one
    if (*pix == oldPix) continue;

    if (doesStemFill(clickedPosColor, pix, fillDepth2)) break;

    // store pixel color in case if the next pixel is with the same color
    oldPix = *pix;
  }
  xa = p.x + pix - pix0 + 1;
}

//-----------------------------------------------------------------------------

bool calcRefFillRow(TRaster32P &refRas, const TPoint &p, int &xa, int &xb,
                    const TPixel32 &color, const TPixel32 &clickedPosColor,
                    const int fillDepth = 254) {
  fullColorFindSegment(refRas, p, xa, xb, color, clickedPosColor, fillDepth);

  return (xb >= xa);
}

//-----------------------------------------------------------------------------

class FillSeed {
public:
  int m_xa, m_xb;
  int m_y, m_dy;
  FillSeed(int xa, int xb, int y, int dy)
      : m_xa(xa), m_xb(xb), m_y(y), m_dy(dy) {}
};

//-----------------------------------------------------------------------------

inline int threshTone(const TPixelCM32 &pix, int fillDepth) {
  if (fillDepth == TPixelCM32::getMaxTone())
    return pix.getTone();
  else
    return ((pix.getTone()) > fillDepth) ? TPixelCM32::getMaxTone()
                                         : pix.getTone();
}

void fillRow(const TRasterCM32P &r, const TPoint &p, int xa, int xb, int paint,
             TPalette *palette, TTileSaverCM32 *saver) {
  /* vai a destra */
  TPixelCM32 *line = r->pixels(p.y);
  TPixelCM32 *pix  = line + p.x;

  if (saver) saver->save(TRect(xa, p.y, xb, p.y));

  if (xb >= xa) {
    pix = line + xa;
    int n;
    for (n = 0; n < xb - xa + 1; n++, pix++) {
      if (palette && pix->isPurePaint()) {
        TPoint pInk = nearestInkNotDiagonal(r, TPoint(xa + n, p.y));
        if (pInk != TPoint(-1, -1)) {
          TPixelCM32 *pixInk =
              (TPixelCM32 *)r->getRawData() + (pInk.y * r->getWrap() + pInk.x);
          if (pixInk->getInk() != paint &&
              palette->getStyle(pixInk->getInk())->getFlags() != 0)
            inkFill(r, pInk, paint, 0, saver);
        }
      }

      pix->setPaint(paint);
    }
  }
}

//-----------------------------------------------------------------------------

inline int threshMatte(int matte, int fillDepth) {
  if (fillDepth == 255)
    return matte;
  else
    return (matte < fillDepth) ? 255 : matte;
}

//-----------------------------------------------------------------------------

bool isPixelInSegment(const std::vector<std::pair<int, int>> &segments, int x) {
  for (int i = 0; i < (int)segments.size(); i++) {
    std::pair<int, int> segment = segments[i];
    if (segment.first <= x && x <= segment.second) return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void insertSegment(std::vector<std::pair<int, int>> &segments,
                   const std::pair<int, int> segment) {
  for (int i = segments.size() - 1; i >= 0; i--) {
    std::pair<int, int> app = segments[i];
    if (segment.first <= app.first && app.second <= segment.second)
      segments.erase(segments.begin() + i);
  }
  segments.push_back(segment);
}

//-----------------------------------------------------------------------------

bool floodCheck(const TPixel32 &clickColor, const TPixel32 *targetPix,
                const TPixel32 *oldPix, const int fillDepth) {
  auto fullColorThreshMatte = [](int matte, int fillDepth) -> int {
    return (matte <= fillDepth) ? matte : 255;
  };

  if (clickColor.m == 0) {
    int oldMatte = fullColorThreshMatte(oldPix->m, fillDepth);
    int matte    = fullColorThreshMatte(targetPix->m, fillDepth);
    return matte >= oldMatte && matte != 255;
  }
  int fillDepth2 = fillDepth * fillDepth;
  return !doesStemFill(clickColor, targetPix, fillDepth2);
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

TRasterCM32P convertRaster2CM(const TRasterP &inputRaster) {
  int lx = inputRaster->getLx();
  int ly = inputRaster->getLy();

  TRaster32P r = inputRaster;

  TRasterCM32P rout(lx, ly);

  for (int y = 0; y < ly; y++) {
    TPixel32 *pixin    = r->pixels(y);
    TPixel32 *pixinEnd = pixin + lx;
    TPixelCM32 *pixout = rout->pixels(y);
    while (pixin < pixinEnd) {
      if (*pixin == TPixel32(0, 0, 0, 0)) {
        ++pixin;
        *pixout++ = TPixelCM32(0, 0, 255);
      } else {
        int v = (pixin->r + pixin->g + pixin->b) / 3;
        ++pixin;
        *pixout++ = TPixelCM32(1, 0, v);
      }
    }
  }
  return rout;
}

//-----------------------------------------------------------------------------

void outputPixels(const std::string _str, const TRasterCM32P &r) {
    r->lock();
    TPixelCM32* tempPix = r->pixels(0);
    std::cout << "\noutputPixels for:";
    std::cout << _str;
    std::cout << ", bbox is y : ";
    std::cout << r->getLy();
    std::cout << ",x:";
    std::cout << r->getLx();
    std::cout << "----";
    tempPix = tempPix + ((r->getLy()-1) * r->getLx());
    for (int tempY = r->getLy()-1; tempY >= 0; tempY--) {
        if (tempY < r->getLy() - 1) {
            tempPix = tempPix - 2 * r->getLx(); 
        }
        std::cout << "\ny:";
        std::cout << tempY;
        for (int tempX = 0; tempX < r->getLx();
            tempX++, tempPix++) {
            std::cout << "|x:";
            std::cout << tempX;
            std::cout << ":";
            std::cout << tempPix->getInk();
            std::cout << ".";
            std::cout << tempPix->getPaint();
            std::cout << ".";
            std::cout << tempPix->getTone();
        }
    }
    r->unlock();
    std::cout << "\n";
    return;
}

//-----------------------------------------------------------------------------

/*-- The return value is whether the saveBox has been updated or not. --*/
bool fill(const TRasterCM32P &r, const FillParameters &params,
          TTileSaverCM32 *saver, bool fillGaps, bool closeGaps,
          int closeStyleIndex, double autoCloseDistance, TXsheet *xsheet,
          int frameIndex) {
  auto fullColorThreshMatte = [](int matte, int fillDepth) -> int {
    return (matte <= fillDepth) ? matte : 255;
  };

  TPixelCM32 *pix, *limit, *pix0, *oldpix;
  TPixel32 *refpix, *oldrefpix;
  int oldy, xa, xb, xc, xd, dy;
  int oldxc, oldxd;
  int tone, oldtone;
  TPoint p = params.m_p;
  int x = p.x, y = p.y;
  int paint = params.m_styleId;
  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;
  TRasterCM32P tempRaster, cr, refCMRaster;
  int styleIndex                                 = GAP_CLOSE_TEMP;
  int fakeStyleIndex                             = GAP_CLOSE_USED;
  if (autoCloseDistance < 0.0) autoCloseDistance = AutocloseDistance;

  bool gapsClosed = false, refGapsClosed = false;

  if (fillGaps) {
    tempRaster = r->clone();
    gapsClosed = TAutocloser(tempRaster, autoCloseDistance, AutocloseAngle,
                             styleIndex, AutocloseOpacity)
                     .exec();
  }
  if (!gapsClosed) {
    tempRaster = r;
  }

  /*-- getBounds returns the entire image --*/
  TRect bbbox = tempRaster->getBounds();

  /*- Return if clicked outside the screen -*/
  if (!bbbox.contains(p)) return false;
  /*- If the same color has already been painted, return -*/
  int paintAtClickedPos = (tempRaster->pixels(p.y) + p.x)->getPaint();
  
  if (paintAtClickedPos == paint) {
      return false;
  }
  /*- If the "paint only transparent areas" option is enabled and the area is
   * already colored, return
   * -*/
  if (params.m_emptyOnly && (tempRaster->pixels(p.y) + p.x)->getPaint() != 0)
    return false;

  TRaster32P refRaster(bbbox.getSize());
  TPixel32 clickedPosColor, color(255, 255, 255);

  std::map<int, std::vector<std::pair<int, int>>> segments;

  if (xsheet) {
    ToonzScene *scene = xsheet->getScene();
    TCamera *camera   = scene->getCurrentCamera();
    TRaster32P tmpRaster(camera->getRes());

    // Render for reference
    scene->renderFrame(tmpRaster, frameIndex, xsheet, false, false, true);

    refRaster->lock();

    TPoint offset((params.m_imageSize.lx - tmpRaster->getLx()) / 2,
                  (params.m_imageSize.ly - tmpRaster->getLy()) / 2);
    offset -= params.m_imageOffset;

    refRaster->fill(color);
    refRaster->copy(tmpRaster, offset);

    refpix          = refRaster->pixels(p.y) + p.x;
    clickedPosColor = *refpix;

    if (params.m_emptyOnly && clickedPosColor != TPixel32(0, 0, 0, 0)) {
      refRaster->unlock();
      return false;
    }

    if (fillGaps) {
      cr = convertRaster2CM(refRaster);
      refCMRaster = cr->clone();

      refGapsClosed = TAutocloser(refCMRaster, autoCloseDistance,
                                  AutocloseAngle, styleIndex, AutocloseOpacity)
                          .exec();
      if (refGapsClosed) {
        if (!gapsClosed) tempRaster = r->clone();

        // Transfer the gap segments to the refRaster
        TPixelCM32 *tempPix  = tempRaster->pixels(0);
        TPixelCM32 *refCMPix = refCMRaster->pixels(0);
        TPixel32 *refPix     = refRaster->pixels(0);
        for (int refCMY = 0; refCMY < refCMRaster->getLy(); refCMY++) {
          for (int refCMX = 0; refCMX < refCMRaster->getLx();
               refCMX++, refCMPix++, refPix++, tempPix++) {
            if (refCMPix->getInk() != styleIndex) continue;
            *refPix = color;
            if (fillGaps) {
              tempPix->setInk(refCMPix->getInk());
              tempPix->setTone(refCMPix->getTone());
            }
          }
        }
      }
    }
  }

  if (fillGaps && !gapsClosed && !refGapsClosed) {
    fillGaps = false;
  }

  assert(fillDepth >= 0 && fillDepth < 16);

  // For refer visible on Smart Raster, use Raster fill depth calculation
  // since refer visible uses a raster image
  if (xsheet)  // convert fillDepth range from [0 - 15] to [0 - 255]
    fillDepth = (fillDepth << 4) | fillDepth;
  else
    switch (TPixelCM32::getMaxTone()) {
    case 15:
      fillDepth = (15 - fillDepth);
      break;
    case 255:
      fillDepth = ((15 - fillDepth) << 4) | (15 - fillDepth);
      break;
    default:
      assert(false);
    }
  /*--Look at the colors in the four corners and update the saveBox if any of
   * the colors change. --*/
  TPixelCM32 borderIndex[4];
  TPixelCM32 *borderPix[4];
  pix            = tempRaster->pixels(0);
  borderPix[0]   = pix;
  borderIndex[0] = *pix;
  pix += tempRaster->getLx() - 1;
  borderPix[1]   = pix;
  borderIndex[1] = *pix;
  pix            = tempRaster->pixels(tempRaster->getLy() - 1);
  borderPix[2]   = pix;
  borderIndex[2] = *pix;
  pix += tempRaster->getLx() - 1;
  borderPix[3]   = pix;
  borderIndex[3] = *pix;

  std::stack<FillSeed> seeds;

  bool fillIt =
      !xsheet ? calcFillRow(tempRaster, p, xa, xb, paint, params.m_palette,
                            params.m_prevailing, params.m_emptyOnly)
              : calcRefFillRow(refRaster, p, xa, xb, color, clickedPosColor,
                               fillDepth);

  if (fillIt) fillRow(tempRaster, p, xa, xb, paint, params.m_palette, saver);
  if (xsheet) segments[y].push_back(std::pair<int, int>(xa, xb));
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));

  while (!seeds.empty()) {
    FillSeed fs = seeds.top();
    seeds.pop();

    xa   = fs.m_xa;
    xb   = fs.m_xb;
    oldy = fs.m_y;
    dy   = fs.m_dy;
    y    = oldy + dy;
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    pix = pix0 = tempRaster->pixels(y) + xa;
    limit      = tempRaster->pixels(y) + xb;
    oldpix     = tempRaster->pixels(oldy) + xa;
    x          = xa;
    oldxd      = (std::numeric_limits<int>::min)();
    oldxc      = (std::numeric_limits<int>::max)();

    if (xsheet) {
      refpix    = refRaster->pixels(y) + xa;
      oldrefpix = refRaster->pixels(oldy) + xa;
    }
    while (pix <= limit) {
      bool canPaint = false;
      if (!xsheet) {
        oldtone = threshTone(*oldpix, fillDepth);
        tone    = threshTone(*pix, fillDepth);
        // the last condition is added in order to prevent fill area from
        // protruding behind the colored line
        canPaint = pix->getPaint() != paint && tone <= oldtone &&
                   (!params.m_emptyOnly || pix->getPaint() == 0) && tone != 0 &&
                   (pix->getPaint() != pix->getInk() ||
                    pix->getPaint() == paintAtClickedPos);
      } else {
        bool test = false;
        if (segments.find(y) != segments.end())
          test   = isPixelInSegment(segments[y], x);
        canPaint = *refpix != color && !test &&
                   floodCheck(clickedPosColor, refpix, oldrefpix, fillDepth);
      }
      if (canPaint) {
        bool fillIt = !xsheet
                          ? calcFillRow(tempRaster, TPoint(x, y), xc, xd, paint,
                                        params.m_palette, params.m_prevailing)
                          : calcRefFillRow(refRaster, TPoint(x, y), xc, xd,
                                           color, clickedPosColor, fillDepth);
        if (fillIt)
          fillRow(tempRaster, TPoint(x, y), xc, xd, paint, params.m_palette,
                  saver);
        if (xsheet) insertSegment(segments[y], std::pair<int, int>(xc, xd));

        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        if (xsheet) {
          refpix += xd - x + 1;
          oldrefpix += xd - x + 1;
        }
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
        if (xsheet) {
          refpix++;
          oldrefpix++;
        }
      }
    }
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
  }

  bool saveBoxChanged = false;

  if (xsheet) {
    refRaster->unlock();
    saveBoxChanged = true;
  } else {
    for (int i = 0; i < 4; i++) {
      if (!((*borderPix[i]) == borderIndex[i])) {
        saveBoxChanged = true;
        break;
      }
    }
  }

  if (fillGaps) {
    finishGapLines(tempRaster, bbbox, r, refCMRaster, params.m_palette, paintAtClickedPos, paint, closeStyleIndex, closeGaps);
    TPixelCM32 *tempPix, *tempPixRestart;
    tempPixRestart = tempRaster->pixels();
    TPixelCM32 *keepPix, *keepPixRestart;
    keepPixRestart = r->pixels();
    int fillNeighbors = 0;
    int paintableNeighbors = 0;

    for (int tempY = 0; tempY < tempRaster->getLy(); tempY++) {
      tempPix = tempPixRestart;
      keepPix = keepPixRestart;
      for (int tempX = 0; tempX < tempRaster->getLx();
           tempX++, tempPix++, keepPix++) {
        if (tempPix->getInk() >= IGNORECOLORSTYLE || tempPix->getPaint() >= IGNORECOLORSTYLE) {
          continue;
        }
        keepPix->setInk(tempPix->getInk());
        keepPix->setPaint(tempPix->getPaint());
        keepPix->setTone(tempPix->getTone());
      }
      tempPixRestart += tempRaster->getWrap();
      keepPixRestart += r->getWrap();
    }
  }

  return saveBoxChanged;
}

//-----------------------------------------------------------------------------

void fill(const TRaster32P &ras, const TRaster32P &ref,
          const FillParameters &params, TTileSaverFullColor *saver) {
  TPixel32 *pix, *limit, *pix0, *oldpix;
  int oldy, xa, xb, xc, xd, dy;
  int oldxc, oldxd;
  int matte, oldMatte;
  int x = params.m_p.x, y = params.m_p.y;
  TRaster32P workRas = ref ? ref : ras;

  TRect bbbox = workRas->getBounds();

  if (!bbbox.contains(params.m_p)) return;

  TPaletteP plt  = params.m_palette;
  TPixel32 color = plt->getStyle(params.m_styleId)->getMainColor();
  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;

  assert(fillDepth >= 0 && fillDepth < 16);
  fillDepth = ((15 - fillDepth) << 4) | (15 - fillDepth);

  // looking for any  pure transparent pixel along the border; if after filling
  // that pixel will be changed,
  // it means that I filled the bg and the savebox needs to be recomputed!
  TPixel32 borderIndex;
  TPixel32 *borderPix = 0;
  pix                 = workRas->pixels(0);
  int i;
  for (i = 0; i < workRas->getLx(); i++, pix++)  // border down
    if (pix->m == 0) {
      borderIndex = *pix;
      borderPix   = pix;
      break;
    }
  if (borderPix == 0)  // not found in border down...try border up (avoid left
                       // and right borders...so unlikely)
  {
    pix = workRas->pixels(workRas->getLy() - 1);
    for (i = 0; i < workRas->getLx(); i++, pix++)  // border up
      if (pix->m == 0) {
        borderIndex = *pix;
        borderPix   = pix;
        break;
      }
  }

  std::stack<FillSeed> seeds;
  std::map<int, std::vector<std::pair<int, int>>> segments;

  // fillRow(r, params.m_p, xa, xb, color ,saver);
  findSegment(workRas, params.m_p, xa, xb, color);
  segments[y].push_back(std::pair<int, int>(xa, xb));
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));

  while (!seeds.empty()) {
    FillSeed fs = seeds.top();
    seeds.pop();

    xa   = fs.m_xa;
    xb   = fs.m_xb;
    oldy = fs.m_y;
    dy   = fs.m_dy;
    y    = oldy + dy;
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    pix = pix0 = workRas->pixels(y) + xa;
    limit      = workRas->pixels(y) + xb;
    oldpix     = workRas->pixels(oldy) + xa;
    x          = xa;
    oldxd      = (std::numeric_limits<int>::min)();
    oldxc      = (std::numeric_limits<int>::max)();
    while (pix <= limit) {
      oldMatte  = threshMatte(oldpix->m, fillDepth);
      matte     = threshMatte(pix->m, fillDepth);
      bool test = false;
      if (segments.find(y) != segments.end())
        test = isPixelInSegment(segments[y], x);
      if (*pix != color && !test && matte >= oldMatte && matte != 255) {
        findSegment(workRas, TPoint(x, y), xc, xd, color);
        // segments[y].push_back(std::pair<int,int>(xc, xd));
        insertSegment(segments[y], std::pair<int, int>(xc, xd));
        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
      }
    }
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
  }

  std::map<int, std::vector<std::pair<int, int>>>::iterator it;
  for (it = segments.begin(); it != segments.end(); it++) {
    TPixel32 *line    = ras->pixels(it->first);
    TPixel32 *refLine = 0;
    TPixel32 *refPix;
    if (ref) refLine = ref->pixels(it->first);
    std::vector<std::pair<int, int>> segmentVector = it->second;
    for (int i = 0; i < (int)segmentVector.size(); i++) {
      std::pair<int, int> segment = segmentVector[i];
      if (segment.second >= segment.first) {
        pix = line + segment.first;
        if (ref) refPix = refLine + segment.first;
        int n;
        for (n = 0; n < segment.second - segment.first + 1; n++, pix++) {
          if (ref) {
            *pix = *refPix;
            refPix++;
          } else
            *pix = pix->m == 0 ? color : overPix(color, *pix);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

static void rectFill(const TRaster32P &ras, const TRect &r,
                     const TPixel32 &color) {}

//-----------------------------------------------------------------------------

static TPoint nearestInk(const TRasterCM32P &r, const TPoint &p, int ray) {
  int i, j;
  TPixelCM32 *buf = (TPixelCM32 *)r->getRawData();

  for (j = std::max(p.y - ray, 0); j <= std::min(p.y + ray, r->getLy() - 1);
       j++)
    for (i = std::max(p.x - ray, 0); i <= std::min(p.x + ray, r->getLx() - 1);
         i++)
      if (!(buf + j * r->getWrap() + i)->isPurePaint()) return TPoint(i, j);

  return TPoint(-1, -1);
}

//-----------------------------------------------------------------------------

void inkFill(const TRasterCM32P &r, const TPoint &pin, int ink, int searchRay,
             TTileSaverCM32 *saver, TRect *insideRect) {
  r->lock();
  TPixelCM32 *pixels = (TPixelCM32 *)r->getRawData();
  int oldInk;
  TPoint p = pin;

  if ((pixels + p.y * r->getWrap() + p.x)->isPurePaint() &&
      (searchRay == 0 || (p = nearestInk(r, p, searchRay)) == TPoint(-1, -1))) {
    r->unlock();
    return;
  }
  TPixelCM32 *pix = pixels + (p.y * r->getWrap() + p.x);

  if (pix->getInk() == ink) {
    r->unlock();
    return;
  }

  oldInk = pix->getInk();

  std::stack<TPoint> seeds;
  seeds.push(p);

  while (!seeds.empty()) {
    p = seeds.top();
    seeds.pop();
    if (!r->getBounds().contains(p)) continue;
    if (insideRect && !insideRect->contains(p)) continue;

    TPixelCM32 *pix = pixels + (p.y * r->getWrap() + p.x);
    if (pix->isPurePaint() || pix->getInk() != oldInk) continue;

    if (saver) saver->save(p);

    pix->setInk(ink);

    seeds.push(TPoint(p.x - 1, p.y - 1));
    seeds.push(TPoint(p.x - 1, p.y));
    seeds.push(TPoint(p.x - 1, p.y + 1));
    seeds.push(TPoint(p.x, p.y - 1));
    seeds.push(TPoint(p.x, p.y + 1));
    seeds.push(TPoint(p.x + 1, p.y - 1));
    seeds.push(TPoint(p.x + 1, p.y));
    seeds.push(TPoint(p.x + 1, p.y + 1));
  }
  r->unlock();
}

//-----------------------------------------------------------------------------

void fullColorFill(const TRaster32P &ras, const FillParameters &params,
                   TTileSaverFullColor *saver, TXsheet *xsheet, int frameIndex,
                   bool fillGaps, bool closeGaps, int closeStyleIndex,
                   double autoCloseDistance) {
  int oldy, xa, xb, xc, xd, dy, oldxd, oldxc;
  TPixel32 *pix, *limit, *pix0, *oldpix, *refpix, *oldrefpix;
  TPixelCM32 *refCMpix;
  int x = params.m_p.x, y = params.m_p.y;

  TRect bbbox = ras->getBounds();
  if (!bbbox.contains(params.m_p)) return;

  TPixel32 clickedPosColor = *(ras->pixels(y) + x);

  TPaletteP plt  = params.m_palette;
  TPixel32 color = plt->getStyle(params.m_styleId)->getMainColor();

  if (clickedPosColor == color) return;

  TRaster32P refRas(bbbox.getSize());

  TPixel32 gapColor  = plt->getStyle(closeStyleIndex)->getMainColor();
  int styleIndex     = GAP_CLOSE_TEMP;
  int fakeStyleIndex = GAP_CLOSE_USED;

  if (xsheet) {
    ToonzScene *scene = xsheet->getScene();
    TCamera *camera   = scene->getCurrentCamera();
    TRaster32P tmpRaster(camera->getRes());
    scene->renderFrame(tmpRaster, frameIndex, xsheet, false, false, true);

    refRas->lock();

    TPoint offset((refRas->getLx() - tmpRaster->getLx()) / 2,
                  (refRas->getLy() - tmpRaster->getLy()) / 2);
    refRas->fill(color);
    refRas->copy(tmpRaster, offset);

    clickedPosColor = *(refRas->pixels(y) + x);
  } else if (fillGaps) {
    refRas->lock();
    TPoint offset((refRas->getLx() - ras->getLx()) / 2,
                  (refRas->getLy() - ras->getLy()) / 2);
    refRas->fill(color);
    refRas->copy(ras, offset);
  }

  TRasterCM32P refCMRaster;
  TRasterCM32P cr;

  if (fillGaps) {
    cr = convertRaster2CM(refRas);
    refCMRaster     = cr->clone();
    fillGaps = TAutocloser(refCMRaster, autoCloseDistance, AutocloseAngle,
                           styleIndex, AutocloseOpacity)
                   .exec();
    if (fillGaps) {
      // Transfer the gap segments to the refRaster
      refCMpix         = refCMRaster->pixels(0);
      TPixel32 *refPix = refRas->pixels(0);
      for (int refCMY = 0; refCMY < refCMRaster->getLy(); refCMY++) {
        for (int refCMX = 0; refCMX < refCMRaster->getLx();
             refCMX++, refCMpix++, refPix++) {
          if (refCMpix->getInk() != styleIndex) continue;
          *refPix = color;
        }
      }
    }
  }

  int fillDepth =
      params.m_shiftFill ? params.m_maxFillDepth : params.m_minFillDepth;

  assert(fillDepth >= 0 && fillDepth < 16);
  TPointD m_firstPoint, m_clickPoint;

  // convert fillDepth range from [0 - 15] to [0 - 255]
  fillDepth = (fillDepth << 4) | fillDepth;

  std::stack<FillSeed> seeds;
  std::map<int, std::vector<std::pair<int, int>>> segments;

  if (!xsheet && !fillGaps)
    fullColorFindSegment(ras, params.m_p, xa, xb, color, clickedPosColor,
                         fillDepth);
  else
    fullColorFindSegment(refRas, params.m_p, xa, xb, color, clickedPosColor,
                         fillDepth);

  segments[y].push_back(std::pair<int, int>(xa, xb));
  seeds.push(FillSeed(xa, xb, y, 1));
  seeds.push(FillSeed(xa, xb, y, -1));

  while (!seeds.empty()) {
    FillSeed fs = seeds.top();
    seeds.pop();

    xa   = fs.m_xa;
    xb   = fs.m_xb;
    oldy = fs.m_y;
    dy   = fs.m_dy;
    y    = oldy + dy;
    // continue if the fill runs over image bounding
    if (y > bbbox.y1 || y < bbbox.y0) continue;
    // left end of the pixels to be filled
    pix = pix0 = ras->pixels(y) + xa;
    // right end of the pixels to be filled
    limit = ras->pixels(y) + xb;
    // left end of the fill seed pixels
    oldpix = ras->pixels(oldy) + xa;

    if (xsheet || fillGaps) {
      refpix    = refRas->pixels(y) + xa;
      oldrefpix = refRas->pixels(oldy) + xa;
    }

    x     = xa;
    oldxd = (std::numeric_limits<int>::min)();
    oldxc = (std::numeric_limits<int>::max)();

    // check pixels to right
    while (pix <= limit) {
      bool test = false;
      // check if the target is already in the range to be filled
      if (segments.find(y) != segments.end())
        test = isPixelInSegment(segments[y], x);
      bool canPaint = false;
      if (!xsheet && !fillGaps)
        canPaint = *pix != color && !test &&
                   floodCheck(clickedPosColor, pix, oldpix, fillDepth);
      else
        canPaint = *refpix != color && !test &&
                   floodCheck(clickedPosColor, refpix, oldrefpix, fillDepth);
      if (canPaint) {
        // compute horizontal range to be filled
        if (!xsheet && !fillGaps)
          fullColorFindSegment(ras, TPoint(x, y), xc, xd, color,
                               clickedPosColor, fillDepth);
        else
          fullColorFindSegment(refRas, TPoint(x, y), xc, xd, color,
                               clickedPosColor, fillDepth);
        // insert segment to be filled
        insertSegment(segments[y], std::pair<int, int>(xc, xd));

        // create new fillSeed to invert direction, if needed
        if (xc < xa) seeds.push(FillSeed(xc, xa - 1, y, -dy));
        if (xd > xb) seeds.push(FillSeed(xb + 1, xd, y, -dy));
        if (oldxd >= xc - 1)
          oldxd = xd;
        else {
          if (oldxd >= 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
          oldxc = xc;
          oldxd = xd;
        }
        // jump to the next pixel to the right end of the range
        pix += xd - x + 1;
        oldpix += xd - x + 1;
        if (xsheet || fillGaps) {
          refpix += xd - x + 1;
          oldrefpix += xd - x + 1;
        }
        x += xd - x + 1;
      } else {
        pix++;
        oldpix++, x++;
        if (xsheet || fillGaps) {
          refpix++;
          oldrefpix++;
        }
      }
    }
    // insert filled range as new fill seed
    if (oldxd > 0) seeds.push(FillSeed(oldxc, oldxd, y, dy));
  }

  if (xsheet || fillGaps) refRas->unlock();

  // pixels are actually filled here
  TPixel32 premultiColor = premultiply(color);

  std::map<int, std::vector<std::pair<int, int>>>::iterator it;
  for (it = segments.begin(); it != segments.end(); it++) {
    TPixel32 *line                                 = ras->pixels(it->first);
    std::vector<std::pair<int, int>> segmentVector = it->second;
    for (int i = 0; i < (int)segmentVector.size(); i++) {
      std::pair<int, int> segment = segmentVector[i];
      if (segment.second >= segment.first) {
        pix = line + segment.first;
        if (saver) {
          saver->save(
              TRect(segment.first, it->first, segment.second, it->first));
        }
        int n;
        for (n = 0; n < segment.second - segment.first + 1; n++, pix++) {
          if (clickedPosColor.m == 0)
            *pix = pix->m == 0 ? color : overPix(color, *pix);
          else if (color.m == 0 || color.m == 255)  // used for erasing area
            *pix = color;
          else
            *pix = overPix(*pix, premultiColor);
        }
      }
    }
  }

// final check for close gap pixels
  if (fillGaps) {

    TPixelCM32 *tempPix = refCMRaster->pixels();
    TPixel32 *keepPix   = ras->pixels();
    int fillNeighbors;
    for (int tempY = 0; tempY < refCMRaster->getLy(); tempY++) {
      for (int tempX = 0; tempX < refCMRaster->getLx();
           tempX++, tempPix++, keepPix++) {
        // if (tempPix->getInk() == fakeStyleIndex) {
        if (tempPix->getInk() == styleIndex) {
          // how many fill pixel neighbors for the current pixel?
          fillNeighbors = 0;
          if ((tempX > 0) && *(keepPix - 1) == color) fillNeighbors++;  // west
          if (tempX < ras->getLx() - 1 && *(keepPix + 1) == color)
            fillNeighbors++;  // east
          if (tempY < ras->getLy() - 1 && *(keepPix + ras->getWrap()) == color)
            fillNeighbors++;  // north
          if (tempY > 0 && *(keepPix - ras->getWrap()) == color)
            fillNeighbors++;  // south
          if (fillNeighbors < 1) {
            // no neighboring fill pixels, this is an unused gap close pixel
            continue;
          } else if (fillNeighbors > 3) {
            // too many neighboring fill pixels to be a gap close pixel
            // , convert it to a fill pixel
            *keepPix = color;
          } else {
            // does it have at least one unpainted pixel neighbor?
            if (((tempX > 0) && (tempPix - 1)->getInk() == 0 &&
                 (*(keepPix - 1) == clickedPosColor))  // west
                || (tempX < refCMRaster->getLx() - 1 &&
                    (tempPix + 1)->getInk() == 0 &&
                    *(keepPix + 1) == clickedPosColor)  // east
                || (tempY < refCMRaster->getLy() - 1 &&
                    (tempPix + refCMRaster->getWrap())->getInk() == 0 &&
                    *(keepPix + ras->getWrap()) == clickedPosColor)  // north
                || (tempY > 0 &&
                    (tempPix - refCMRaster->getWrap())->getInk() == 0 &&
                    *(keepPix - ras->getWrap()) == clickedPosColor)  // south
            ) {
              // yes, persist it as a gap close pixel
              if (closeGaps) {
                *keepPix = gapColor;
              } else {
                *keepPix = color;
              }
            } else {
              // it is not acting as a border pixel so convert it to a fill
              // pixel
              *keepPix = color;
            }
          }
        }
      }
    }
  }
}
