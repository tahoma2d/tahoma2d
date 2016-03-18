

#ifndef TREGIONOUTLINE_H
#define TREGIONOUTLINE_H

class TRegionOutline
{
public:
	typedef vector<T3DPointD> PointVector;
	typedef vector<PointVector> Boundary;

	Boundary m_exterior, m_interior;
	bool m_doAntialiasing;

	TRectD m_bbox;

	TRegionOutline() : m_doAntialiasing(false) {}

	void clear()
	{
		m_exterior.clear();
		m_interior.clear();
	}
};

#endif
