

// TnzCore includes
#include "tfilepath.h"
#include "tenv.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/preferences.h"

#include "toonz/onionskinmask.h"

//*****************************************************************************************
//    Macros
//*****************************************************************************************

#define MINFADE 0.35
#define MAXFADE 0.95

//*****************************************************************************************
//    Local namespace
//*****************************************************************************************

namespace {

double inline getIncrement(int paperThickness) {
  struct locals {
    inline static void fillIncrements(double *values, int a, int b) {
      double slope = (values[b] - values[a]) / (b - a);
      for (int i = a + 1; i < b; ++i) values[i] = values[i - 1] + slope;
    }
  };  // locals

  static double Incr[101] = {-1.0};

  if (Incr[0] == -1.0) {
    Incr[0]   = 0.0;
    Incr[10]  = 0.05;
    Incr[50]  = 0.12;
    Incr[90]  = 0.3;
    Incr[100] = MAXFADE - MINFADE;

    locals::fillIncrements(Incr, 0, 10);
    locals::fillIncrements(Incr, 10, 50);
    locals::fillIncrements(Incr, 50, 90);
    locals::fillIncrements(Incr, 90, 100);
  }

  return Incr[paperThickness];
}

}  // namespace

//***************************************************************************
//    OnionSkinMask  implementation
//***************************************************************************

TEnv::IntVar WholeScene("OnionSkinWholeScene", 0);
TEnv::IntVar EveryFrame("OnionSkinEveryFrame", 1);

OnionSkinMask::OnionSkinMask() {
  m_enabled = false;
  m_wholeScene = WholeScene;
  m_everyFrame = EveryFrame;
  m_LightTableStatus = false;
}

void OnionSkinMask::clear() {
  m_fos.clear();
  m_mos.clear();

  m_shiftTraceStatus = DISABLED;

  m_ghostAff[0]    = TAffine();
  m_ghostAff[1]    = TAffine();
  m_ghostCenter[0] = TPointD();
  m_ghostCenter[1] = TPointD();
  m_ghostFrame[0]  = 0;
  m_ghostFrame[1]  = 0;
}

//-------------------------------------------------------------------

void OnionSkinMask::getAll(int currentRow, std::vector<int> &output) const {
  output.clear();
  output.reserve(m_fos.size() + m_mos.size());

  std::vector<std::pair<int, double>>::const_iterator fosIt, fosEnd(m_fos.end());
  std::vector<std::pair<int, double>>::const_iterator mosIt,
      mosEnd(m_mos.end());

  for (fosIt = m_fos.begin(), mosIt = m_mos.begin();
       fosIt != fosEnd && mosIt != mosEnd;) {
    int fos = fosIt->first, mos = mosIt->first + currentRow;

    if (fos < mos) {
      if (fos != currentRow) output.push_back(fos);

      ++fosIt;
    } else {
      if (mos != currentRow) output.push_back(mos);

      ++mosIt;
    }
  }

  for (; fosIt != fosEnd; ++fosIt)
    if (fosIt->first != currentRow) output.push_back(fosIt->first);

  for (; mosIt != mosEnd; ++mosIt) {
    int mos = mosIt->first + currentRow;
    if (mos != currentRow) output.push_back(mos);
  }
}

//-------------------------------------------------------------------

void OnionSkinMask::setMos(int drow, bool on) {
  assert(drow != 0);

  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_mos.begin(); r != m_mos.end(); r++) {
    if (r->first >= drow) break;
  }

  if (on) {
    if (r == m_mos.end() || r->first != drow)
      m_mos.insert(r, std::make_pair(drow, -1));
  } else {
    if (r != m_mos.end()) m_mos.erase(r);
  }
}

//-------------------------------------------------------------------

void OnionSkinMask::setFos(int row, bool on) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_fos.begin(); r != m_fos.end(); r++) {
    if (r->first >= row) break;
  }

  if (on) {
    if (r == m_fos.end() || r->first != row)
      m_fos.insert(r, std::make_pair(row, -1));
  } else {
    if (r != m_fos.end()) m_fos.erase(r);
  }
}

//-------------------------------------------------------------------

void OnionSkinMask::setMosOpacity(int drow, double opacity) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_mos.begin(); r != m_mos.end(); r++) {
    if (r->first == drow) {
      r->second = opacity;
      break;
    }
  }
}

//-------------------------------------------------------------------

double OnionSkinMask::getMosOpacity(int drow) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_mos.begin(); r != m_mos.end(); r++)
    if (r->first == drow) return r->second;

  return -1.0;
}

//-------------------------------------------------------------------

void OnionSkinMask::setFosOpacity(int row, double opacity) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_fos.begin(); r != m_fos.end(); r++) {
    if (r->first == row) {
      r->second = opacity;
      break;
    }
  }
}

//-------------------------------------------------------------------

double OnionSkinMask::getFosOpacity(int drow) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_fos.begin(); r != m_fos.end(); r++)
    if (r->first == drow) return r->second;

  return -1.0;
}

//-------------------------------------------------------------------

bool OnionSkinMask::isFos(int row) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_fos.begin(); r != m_fos.end(); r++)
    if (r->first == row) return true;

  return false;
}

//-------------------------------------------------------------------

bool OnionSkinMask::isMos(int drow) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_mos.begin(); r != m_mos.end(); r++)
    if (r->first == drow) return true;

  return false;
}

//-------------------------------------------------------------------

bool OnionSkinMask::getMosRange(int &drow0, int &drow1) const {
  if (m_mos.empty()) {
    drow0 = 0, drow1 = -1;
    return false;
  } else {
    drow0 = m_mos.front().first, drow1 = m_mos.back().first;
    return true;
  }
}

//-------------------------------------------------------------------

void OnionSkinMask::setIsWholeScene(bool wholeScene) { 
  m_wholeScene = wholeScene; 
  WholeScene = (int)m_wholeScene;
}

//-------------------------------------------------------------------

void OnionSkinMask::setIsEveryFrame(bool everyFrame) { 
  m_everyFrame = everyFrame; 
  EveryFrame = (int)m_everyFrame;
}

//-------------------------------------------------------------------

double OnionSkinMask::getOnionSkinFade(int rowsDistance) {
  if (rowsDistance == 0) return 0.9;

  double fade =
      MINFADE +
      abs(rowsDistance) *
          getIncrement(Preferences::instance()->getOnionPaperThickness());
  return tcrop(fade, MINFADE, MAXFADE);
}

//-------------------------------------------------------------------

void OnionSkinMask::setShiftTraceGhostAff(int index, const TAffine &aff) {
  assert(0 <= index && index < 2);
  m_ghostAff[index] = aff;
}

//-------------------------------------------------------------------

void OnionSkinMask::setShiftTraceGhostCenter(int index, const TPointD &center) {
  assert(0 <= index && index < 2);
  m_ghostCenter[index] = center;
}

//***************************************************************************
//    OnionSkinMaskModifier  implementation
//***************************************************************************

OnionSkinMaskModifier::OnionSkinMaskModifier(OnionSkinMask mask, int currentRow)
    : m_oldMask(mask)
    , m_curMask(mask)
    , m_firstRow(0)
    , m_lastRow(0)
    , m_curRow(currentRow)
    , m_status(0) {}

//-------------------------------------------------------------------

void OnionSkinMaskModifier::click(int row, bool isFos) {
  assert(m_status == 0);

  m_firstRow = m_lastRow = row;
  if (isFos) {
    assert(row != m_curRow);

    if (m_curMask.isEnabled() && m_curMask.isFos(row)) {
      m_status = 2;  // spegnere fos
      m_curMask.setFos(row, false);
    } else {
      if (!m_curMask.isEnabled()) {
        m_curMask.clear();
        m_curMask.enable(true);
      }

      m_curMask.setFos(row, true);
      m_status = 3;  // accendere fos
    }
  } else {
    int drow = row - m_curRow;
    if (drow != 0 && m_curMask.isEnabled() && m_curMask.isMos(drow)) {
      m_status = 4;  // spegnere mos
      m_curMask.setMos(drow, false);
    } else if (drow == 0) {
      m_status = 8 + 4 + 1;  // accendere mos; partito da 0
    } else {
      if (!m_curMask.isEnabled()) m_curMask.enable(true);
      m_curMask.setMos(drow, true);
      m_status = 4 + 1;  // accendere mos;
    }
  }
}

//-------------------------------------------------------------------

void OnionSkinMaskModifier::drag(int row) {
  if (m_status & 128) return;

  if (row == m_lastRow) return;

  m_status |= 64;  // moved

  int n = row - m_lastRow, d = 1;
  if (n < 0) n = -n, d = -d;

  int oldr = m_lastRow, r = oldr + d;

  for (int i = 0; i < n; ++i, r += d) {
    if (m_status & 4) {
      if (!m_curMask.isEnabled()) {
        m_curMask.clear();
        m_curMask.enable(true);
      }
      if (r != m_curRow) m_curMask.setMos(r - m_curRow, (m_status & 1) != 0);
    } else
      m_curMask.setFos(r, (m_status & 1) != 0);
  }

  m_lastRow = row;
}

//-------------------------------------------------------------------

void OnionSkinMaskModifier::release(int row) {
  if (m_status & 128) return;
  if ((m_status & 64) == 0    // non si e' mosso
      && (m_status & 8) == 8  // e' partito da zero
      && row == m_curRow) {
    if (!m_curMask.isEmpty() && m_curMask.isEnabled())
      m_curMask.enable(false);
    else {
      m_curMask.enable(true);
      if (m_curMask.isEmpty()) {
        m_curMask.setMos(-1, true);
        m_curMask.setMos(-2, true);
        m_curMask.setMos(-3, true);
      }
    }
  }
}
