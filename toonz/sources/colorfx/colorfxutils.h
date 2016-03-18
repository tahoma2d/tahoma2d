

#ifndef COLORFXUTILS_H
#define COLORFXUTILS_H

#include "tgeometry.h"
#include "tpixel.h"
#include "trandom.h"
#include "tregionoutline.h"
#include "tcurves.h"
using namespace std;

class TRegion;
class TFlash;

class RubberDeform
{
	vector<T3DPointD> *m_pPolyOri;
	vector<T3DPointD> m_polyLoc;

	void deformStep();
	double avgLength();
	void refinePoly(const double rf = -1.0);
	void getBBox(TRectD &bbox);

public:
	RubberDeform();
	RubberDeform(vector<T3DPointD> *pPolyOri, const double rf = -1.0);
	void copyLoc2Ori() { *m_pPolyOri = m_polyLoc; };
	void copyOri2Loc() { m_polyLoc = *m_pPolyOri; };

	virtual ~RubberDeform();
	void deform(const double n);
};

class SFlashUtils
{
	void computeOutline(const TRegion *region, TRegionOutline::PointVector &polyline) const;
	void PointVector2QuadsArray(const vector<T3DPointD> &pv,
								vector<TQuadratic *> &quadArray,
								vector<TQuadratic *> &toBeDeleted,
								const bool isRounded) const;
	int nbDiffVerts(const vector<TPointD> &pv) const;
	void Triangle2Quad(vector<TPointD> &p) const;

public:
	const TRegion *m_r;
	TRegionOutline m_ro;

	SFlashUtils(){};
	SFlashUtils(const TRegion *r) : m_r(r){};
	virtual ~SFlashUtils(){};

	void computeRegionOutline();
	void drawRegionOutline(TFlash &flash, const bool isRounded = true) const;
	void drawGradedPolyline(TFlash &flash,
							vector<TPointD> &pv,
							const TPixel32 &c1,
							const TPixel32 &c2) const;
	void drawGradedRegion(TFlash &flash,
						  vector<TPointD> &pv,
						  const TPixel32 &c1,
						  const TPixel32 &c2,
						  const TRegion &r) const;
};

#endif
