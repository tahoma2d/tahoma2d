#pragma once

#ifndef TREGIONOUTLINE_H
#define TREGIONOUTLINE_H

class TRegionOutline {
public:
  typedef std::vector<T3DPointD> PointVector;
  typedef std::vector<PointVector> Boundary;

  Boundary m_exterior, m_interior;
  bool m_doAntialiasing;

  TRectD m_bbox;

  bool m_calculating;
  int m_inUse;

  TRegionOutline()
      : m_doAntialiasing(false), m_calculating(false), m_inUse(0) {}

  void clear() {
    m_exterior.clear();
    m_interior.clear();
  }

  bool setCalculating() {
    if (m_calculating) return false;
    m_calculating = true;
    return true;
  }
  void unsetCalculating() { m_calculating = false; }
  int isCalculating() { return m_calculating; }

  void setInUse() { m_inUse++; }
  void unsetInUse() {
    m_inUse--;
    if (m_inUse < 0) m_inUse = 0;
  }
  int isInUse() { return m_inUse > 0; }
};

#endif
