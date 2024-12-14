

// TnzCore includes
#include "tfilepath.h"
#include "tenv.h"

// TnzLib includes
#include "toonz/txshsimplelevel.h"
#include "toonz/preferences.h"

#include "toonz/onionskinmask.h"

#include "toonz/txshcell.h"

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
TEnv::IntVar RelativeFrameMode("OnionSkinRelativeFrameMode", 1);

OnionSkinMask::OnionSkinMask() {
  m_enabled = false;
  m_wholeScene = WholeScene;
  m_everyFrame = EveryFrame;
  m_LightTableStatus = false;
  m_isRelativeFrameMode = RelativeFrameMode;
}

void OnionSkinMask::clear() {
  m_fos.clear();
  m_mos.clear();
  m_dos.clear();

  m_shiftTraceStatus = DISABLED;

  m_ghostAff[0]    = TAffine();
  m_ghostAff[1]    = TAffine();
  m_ghostCenter[0] = TPointD();
  m_ghostCenter[1] = TPointD();
  m_ghostFrame[0]  = 0;
  m_ghostFrame[1]  = 0;
}

//-------------------------------------------------------------------

void OnionSkinMask::getAll(int currentRow,
                           std::vector<std::pair<int, double>> &output,
                           TXsheet *xsh, int col) const {
  output.clear();
  output.reserve(m_fos.size() +
                 (m_isRelativeFrameMode ? m_mos.size() : m_dos.size()));

  std::vector<std::pair<int, double>>::const_iterator fosIt(m_fos.begin()), fosEnd(m_fos.end());
  std::vector<std::pair<int, double>> dos2mos;
  std::vector<std::pair<int, double>>::const_iterator dosIt(m_dos.begin()),
      dosEnd(m_dos.end());

  if (!m_isRelativeFrameMode && xsh && col >= 0) {
    // Translate relative drawing position to frame drawing position
    TXshColumn *column = xsh->getColumn(col);
    bool isLooped      = column && column->isLooped();
    int r0, r1;
    int n = xsh->getCellRange(col, r0, r1, isLooped);
    std::vector<TXshCell> cells(n);
    xsh->getCells(r0, col, n, &cells[0]);
    int row = currentRow;
    if (isLooped && (currentRow < r0 || currentRow > r1)) {
      row = column->getLoopedFrame(currentRow);
    }

    TXshCell startCell = xsh->getCell(row, col);
    for (; dosIt != dosEnd;) {
      int dist  = 0;
      int r     = row - r0;
      if (r >= n) {  // after last cell
        r         = n - 1;
        startCell = cells[r];
      } else if (r < 0) {  // Before the 1st cell
        r         = 0;
        startCell = cells[r];
      }
      TXshCell prevCell                                 = startCell;
      if (prevCell.getFrameId().isStopFrame()) prevCell = TXshCell();

      int x = dosIt->first;
      if (x < 0) {
        if ((row + x + 1) < r0) {
          ++dosIt;
          continue;
        }
        // Find Previous Drawings
        for (; r >= 0; r--) {
          if (!cells[r].isEmpty() && !cells[r].getFrameId().isStopFrame() &&
              cells[r] != prevCell) {
            prevCell = cells[r];
            x++;
            if (!x) break;
          }
          if (r == 0 && isLooped) r = n;
          dist--;
        }
      } else if (x > 0) {
        if ((row + x) < r0) {
          ++dosIt;
          continue;
        }
        // Find Next Drawings
        for (; r < n; r++) {
          if (!cells[r].isEmpty() && !cells[r].getFrameId().isStopFrame() &&
              cells[r] != prevCell) {
            prevCell = cells[r];
            x--;
            if (!x) break;
          }
          if (r == (n - 1) && isLooped) r = -1;
          dist++;
        }
      }
      if (dist) dos2mos.push_back(std::make_pair(dist, dosIt->second));
      ++dosIt;
    }
  }

  std::vector<std::pair<int, double>>::const_iterator rosIt(
      m_isRelativeFrameMode ? m_mos.begin() : dos2mos.begin()),
      rosEnd(m_isRelativeFrameMode ? m_mos.end() : dos2mos.end());

  for (; fosIt != fosEnd && rosIt != rosEnd;) {
    int fos = fosIt->first;
    int ros = rosIt->first + currentRow;

    if (fos < ros) {
      if (fos != currentRow)
        output.push_back(std::make_pair(fos, fosIt->second));

      ++fosIt;
    } else {
      double opacity = rosIt->second;
      if (fos == ros) {
        double fosFade = fosIt->second;
        double rosFade = rosIt->second;
        opacity        = fosFade == -1.0
                      ? rosFade
                      : (rosFade == -1 ? fosFade : std::min(fosFade, rosFade));
      }
      if (ros != currentRow) output.push_back(std::make_pair(ros, opacity));

      ++rosIt;
    }
  }

  for (; fosIt != fosEnd; ++fosIt)
    if (fosIt->first != currentRow)
      output.push_back(std::make_pair(fosIt->first, fosIt->second));

  for (; rosIt != rosEnd; ++rosIt) {
    int ros = rosIt->first + currentRow;
    if (ros != currentRow) output.push_back(std::make_pair(ros, rosIt->second));
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
    if (r != m_mos.end() && r->first == drow) m_mos.erase(r);
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
    if (r != m_fos.end() && r->first == row) m_fos.erase(r);
  }
}

//-------------------------------------------------------------------

void OnionSkinMask::setDos(int drow, bool on) {
  assert(drow != 0);

  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_dos.begin(); r != m_dos.end(); r++) {
    if (r->first >= drow) break;
  }

  if (on) {
    if (r == m_dos.end() || r->first != drow)
      m_dos.insert(r, std::make_pair(drow, -1));
  } else {
    if (r != m_dos.end() && r->first == drow) m_dos.erase(r);
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

void OnionSkinMask::setDosOpacity(int drow, double opacity) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_dos.begin(); r != m_dos.end(); r++) {
    if (r->first == drow) {
      r->second = opacity;
      break;
    }
  }
}

//-------------------------------------------------------------------

double OnionSkinMask::getDosOpacity(int drow) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_dos.begin(); r != m_dos.end(); r++)
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

bool OnionSkinMask::isDos(int drow) {
  std::vector<std::pair<int, double>>::iterator r;
  for (r = m_dos.begin(); r != m_dos.end(); r++)
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

bool OnionSkinMask::getDosRange(int &drow0, int &drow1) const {
  if (m_dos.empty()) {
    drow0 = 0, drow1 = -1;
    return false;
  } else {
    drow0 = m_dos.front().first, drow1 = m_dos.back().first;
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

void OnionSkinMask::setRelativeFrameMode(bool on) {
  m_isRelativeFrameMode = on;
  RelativeFrameMode     = m_isRelativeFrameMode;
}

int OnionSkinMask::getFrameIdxFromRos(int ros, TXsheet *xsh, int row, int col) {
  if (isRelativeFrameMode()) return row + ros;

  if (!ros || !xsh || col < 0) return -1;
  int r0, r1;
  xsh->getCellRange(col, r0, r1);
  
  if (r1 <= r0) return -1;

  if (ros < 0) {
    std::vector<TXshCell> prevCells(row - r0 + 1);
    xsh->getCells(r0, col, prevCells.size(), &(prevCells[0]));
    TXshCell lastCell;
    for (int x = prevCells.size() - 2; x >= 0; x--) {
      if (prevCells[x].isEmpty() || prevCells[x].getFrameId().isStopFrame() ||
          prevCells[x] == lastCell)
        continue;
      lastCell = prevCells[x];
      ros++;
      if (!ros) return x;
    }
  } else if (ros > 0) {
    std::vector<TXshCell> nextCells(r1 - row + 1);
    xsh->getCells(row, col, nextCells.size(), &(nextCells[0]));
    TXshCell lastCell = nextCells[0];
    for (int x = 1; x < nextCells.size(); x++) {
      if (nextCells[x].isEmpty() || nextCells[x].getFrameId().isStopFrame() ||
          nextCells[x] == lastCell)
        continue;
      lastCell = nextCells[x];
      ros--;
      if (!ros) return row + x;
    }
  }

  return -1;
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
    if (drow != 0 && m_curMask.isEnabled() && m_curMask.isRos(drow)) {
      m_status = 4;  // spegnere mos
      m_curMask.setRos(drow, false);
    } else if (drow == 0) {
      m_status = 8 + 4 + 1;  // accendere mos; partito da 0
    } else {
      if (!m_curMask.isEnabled()) m_curMask.enable(true);
      m_curMask.setRos(drow, true);
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
      if (r != m_curRow) m_curMask.setRos(r - m_curRow, (m_status & 1) != 0);
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
        m_curMask.setDos(-1, true);
        m_curMask.setDos(1, true);
      }
    }
  }
}
