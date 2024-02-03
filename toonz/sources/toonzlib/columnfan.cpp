

#include "toonz/columnfan.h"
#include "toonz/preferences.h"

// TnzCore includes
#include "tstream.h"

// STD includss
#include <assert.h>

//=============================================================================
// ColumnFan

ColumnFan::ColumnFan()
    : m_firstFreePos(0)
    , m_unfolded(74)
    , m_folded(9)
    , m_cameraActive(true)
    , m_cameraColumnDim(22) {}

//-----------------------------------------------------------------------------

void ColumnFan::setDimensions(int unfolded, int cameraColumn) {
  m_unfolded        = unfolded;
  m_cameraColumnDim = cameraColumn;
  // folded always 9
  update();
}

//-----------------------------------------------------------------------------

void ColumnFan::update() {
  int lastPos     = -m_unfolded;
  bool lastActive = true;
  bool lastVisible = true;
  int m           = m_columns.size();
  int i;
  for (i = 0; i < m; i++) {
    bool visible = m_columns[i].m_visible;
    bool active  = m_columns[i].m_active;
    if (lastVisible) {
      if (lastActive)
        lastPos += m_unfolded;
      else if (active)
        lastPos += m_folded;
    }
    m_columns[i].m_pos = lastPos;
    lastActive         = active;
    lastVisible        = visible;
  }
  m_firstFreePos = lastPos + (lastVisible ? (lastActive ? m_unfolded : m_folded) : 0);
  m_table.clear();
  for (i = 0; i < m; i++) {
    int pos = -1;
    if (m_columns[i].m_active && m_columns[i].m_visible)
      pos = m_columns[i].m_pos + m_unfolded - 1;
    else if (i + 1 < m && m_columns[i + 1].m_active &&
             m_columns[i + 1].m_visible)
      pos = m_columns[i + 1].m_pos - 1;
    else if (i + 1 == m)
      pos = m_firstFreePos - (m_columns[i].m_visible ? 1 : 0);

    if (pos >= 0 && m_table.find(pos) == m_table.end()) m_table[pos] = i;
  }
}

//-----------------------------------------------------------------------------

int ColumnFan::layerAxisToCol(int coord) const {
  if (Preferences::instance()->isXsheetCameraColumnVisible()) {
    int firstCol =
        m_cameraActive
            ? m_cameraColumnDim
            : ((m_columns.size() > 0 && (!m_columns[0].m_active || !m_columns[0].m_visible)) ? 0 : m_folded);
    if (coord < firstCol) return -1;
    coord -= firstCol;
  }
  if (coord < m_firstFreePos) {
    std::map<int, int>::const_iterator it = m_table.lower_bound(coord);
    if (it == m_table.end()) return -3;
    assert(it != m_table.end());
    return it->second;
  } else
    return m_columns.size() + (coord - m_firstFreePos) / m_unfolded;
}

//-----------------------------------------------------------------------------

int ColumnFan::colToLayerAxis(int col) const {
  int m        = m_columns.size();
  int firstCol = 0;
  if (Preferences::instance()->isXsheetCameraColumnVisible()) {
    if (col < -1) return -m_cameraColumnDim;
    if (col < 0) return 0;
    firstCol =
        m_cameraActive
            ? m_cameraColumnDim
            : ((m_columns.size() > 0 && (!m_columns[0].m_active || !m_columns[0].m_visible)) ? 0 : m_folded);
  }
  if (col >= 0 && col < m)
    return firstCol + m_columns[col].m_pos;
  else
    return firstCol + m_firstFreePos + (col - m) * m_unfolded;
}

//-----------------------------------------------------------------------------

void ColumnFan::activate(int col) {
  int m = m_columns.size();
  if (col < 0) {
    m_cameraActive = true;
    return;
  }
  if (col < m) {
    m_columns[col].m_active = true;
    int i;
    for (i = m - 1; i >= 0 && m_columns[i].m_active && m_columns[i].m_visible; i--) {
    }
    i++;
    if (i < m) {
      m = i;
      m_columns.erase(m_columns.begin() + i, m_columns.end());
    }
  }
  update();
}

//-----------------------------------------------------------------------------

void ColumnFan::deactivate(int col) {
  if (col < 0) {
    m_cameraActive = false;
    return;
  }
  while ((int)m_columns.size() <= col) m_columns.push_back(Column());
  m_columns[col].m_active = false;
  update();
}

//-----------------------------------------------------------------------------

bool ColumnFan::isActive(int col) const {
  return 0 <= col && col < (int)m_columns.size()
             ? m_columns[col].m_active
             : col < 0 ? m_cameraActive : true;
}

//-----------------------------------------------------------------------------

void ColumnFan::hide(int col) {
  if (col < 0) return;

  while ((int)m_columns.size() <= col) m_columns.push_back(Column());
  m_columns[col].m_visible = false;
  update();
}

//-----------------------------------------------------------------------------

void ColumnFan::show(int col) {
  if (col < 0) return;

  int m = m_columns.size();
  if (col < m) {
    m_columns[col].m_visible = true;
    int i;
    for (i = m - 1; i >= 0 && m_columns[i].m_visible && m_columns[i].m_active;
         i--) {
    }
    i++;
    if (i < m) {
      m = i;
      m_columns.erase(m_columns.begin() + i, m_columns.end());
    }
  }
  update();
}

//-----------------------------------------------------------------------------

bool ColumnFan::isVisible(int col) const {
  return 0 <= col && col < (int)m_columns.size()
             ? m_columns[col].m_visible
             : true;
}

//-----------------------------------------------------------------------------

void ColumnFan::initializeCol(int col) {
  if (col < 0) return;

  while ((int)m_columns.size() <= col) m_columns.push_back(Column());
  update();
}

//-----------------------------------------------------------------------------

bool ColumnFan::isEmpty() const { return m_columns.empty(); }

//-----------------------------------------------------------------------------

void ColumnFan::copyFoldedStateFrom(const ColumnFan &from) {
  m_cameraActive = from.m_cameraActive;
  for (int i = 0, n = (int)from.m_columns.size(); i < n; i++) {
    if (!from.isActive(i)) deactivate(i);
    if (!from.isVisible(i)) hide(i);
  }
}

//-----------------------------------------------------------------------------

void ColumnFan::saveData(
    TOStream &os) {  // only saves indices of folded columns
  int index, n = (int)m_columns.size();
  for (index = 0; index < n;) {
    while (index < n && m_columns[index].m_active) index++;
    if (index < n) {
      int firstIndex = index;
      os << index;
      index++;
      while (index < n && !m_columns[index].m_active) index++;
      os << index - firstIndex;
    }
  }
}

//-----------------------------------------------------------------------------

void ColumnFan::loadData(TIStream &is) {
  m_columns.clear();
  m_table.clear();
  m_firstFreePos = 0;
  while (!is.eos()) {
    int index = 0, count = 0;
    is >> index >> count;
    int j;
    for (j = 0; j < count; j++) deactivate(index + j);
  }
}

//-----------------------------------------------------------------------------

void ColumnFan::saveVisibilityData(
    TOStream &os) {  // only saves indices of hidden columns
  int index, n = (int)m_columns.size();
  bool tagOpened = false;
  for (index = 0; index < n;) {
    while (index < n && m_columns[index].m_visible) index++;
    if (index < n) {
      if (!tagOpened) {
        os.openChild("columnFanVisibility");
        tagOpened = true;
      }

      int firstIndex = index;
      os << index;
      index++;
      while (index < n && !m_columns[index].m_visible) index++;
      os << index - firstIndex;
    }
  }

  if (tagOpened) os.closeChild();
}

//-----------------------------------------------------------------------------

void ColumnFan::loadVisibilityData(TIStream &is) {
  while (!is.eos()) {
    int index = 0, count = 0;
    is >> index >> count;
    int j;
    for (j = 0; j < count; j++) hide(index + j);
  }
}

//-----------------------------------------------------------------------------

void ColumnFan::rollLeftFoldedState(int index, int count) {
  assert(index >= 0);
  int columnCount = m_columns.size();
  if (!columnCount) return; // Nothing to role
  if (count < 2) return;

  int i = index, j = index + count - 1;
  if (j >= columnCount) initializeCol(j);
  bool tmpActive  = isActive(i);
  bool tmpVisible = isVisible(i);

  for (int k = i; k < j; ++k) {
    if (isActive(k) && !isActive(k + 1))
      deactivate(k);
    else if (!isActive(k) && isActive(k + 1))
      activate(k);
    if (isVisible(k) && !isVisible(k + 1))
      hide(k);
    else if (!isVisible(k) && isVisible(k + 1))
      show(k);
  }
  if (isActive(j) && !tmpActive)
    deactivate(j);
  else if (!isActive(j) && tmpActive)
    activate(j);
  if (isVisible(j) && !tmpVisible)
    hide(j);
  else if (!isVisible(j) && tmpVisible)
    show(j);

  update();
}

//-----------------------------------------------------------------------------

void ColumnFan::rollRightFoldedState(int index, int count) {
  assert(index >= 0);

  int columnCount = m_columns.size();
  if (!columnCount) return; // Nothing to roll
  if (count < 2) return;

  int i = index, j = index + count - 1;
  if (j >= columnCount) initializeCol(j);
  bool tmpActive  = isActive(j);
  bool tmpVisible = isVisible(j);

  for (int k = j; k > i; --k) {
    if (isActive(k) && !isActive(k - 1))
      deactivate(k);
    else if (!isActive(k) && isActive(k - 1))
      activate(k);
    if (isVisible(k) && !isVisible(k - 1))
      hide(k);
    else if (!isVisible(k) && isVisible(k - 1))
      show(k);
  }
  if (isActive(i) && !tmpActive)
    deactivate(i);
  else if (!isActive(i) && tmpActive)
    activate(i);
  if (isVisible(i) && !tmpVisible)
    hide(i);
  else if (!isVisible(i) && tmpVisible)
    show(i);

  update();
}
