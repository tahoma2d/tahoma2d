

#include "styledata.h"
#include "tcolorstyles.h"

//=============================================================================
// StyleData
//-----------------------------------------------------------------------------

StyleData::StyleData() {}

//-----------------------------------------------------------------------------

StyleData::~StyleData() {
  int i;
  for (i = 0; i < (int)m_styles.size(); i++) delete m_styles[i].second;
}

//-----------------------------------------------------------------------------

void StyleData::addStyle(int styleIndex, TColorStyle *style,
                         StyleAnimation styleAnimation) {
  m_styles.push_back(std::make_pair(styleIndex, style));
  m_styleAnimationTable.insert(std::make_pair(styleIndex, styleAnimation));
}

//-----------------------------------------------------------------------------

TColorStyle *StyleData::getStyle(int index) const {
  assert(0 <= index && index < (int)m_styles.size());
  return m_styles[index].second;
}

//-----------------------------------------------------------------------------

StyleAnimation StyleData::getStyleAnimation(int styleId) const {
  StyleAnimationTable::const_iterator sat = m_styleAnimationTable.find(styleId);

  if (sat == m_styleAnimationTable.end()) return StyleAnimation();

  return sat->second;
}

//-----------------------------------------------------------------------------

int StyleData::getStyleIndex(int index) const {
  assert(0 <= index && index < (int)m_styles.size());
  return m_styles[index].first;
}

//-----------------------------------------------------------------------------

StyleData *StyleData::clone() const {
  StyleData *data = new StyleData();
  for (int i = 0; i < getStyleCount(); i++)
    data->addStyle(getStyleIndex(i), getStyle(i)->clone(),
                   getStyleAnimation(getStyleIndex(i)));
  return data;
}
