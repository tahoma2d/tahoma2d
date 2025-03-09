

#include <tvariant.h>

//---------------------------------------------------------

int
TVariantPath::compare(
  const TVariantPath &a, int beginA,
  const TVariantPath &b, int beginB, int count )
{
  assert(beginA >= 0 && beginA <= (int)a.size());
  assert(beginB >= 0 && beginB <= (int)b.size());
  if (count == 0) return 0;
  int countA = std::min(count, (int)a.size() - beginA);
  int countB = std::min(count, (int)b.size() - beginB);
  count = std::min(countA, countB);

  TVariantPath::const_iterator ia = a.begin() + beginA;
  TVariantPath::const_iterator ib = b.begin() + beginB;
  for(int i = 0; i < count; ++i, ++ia, ++ib)
    if ((*ia) < (*ib)) return -1; else
      if ((*ib) < (*ia)) return 1;
  return countA < countB ? -1
       : countB < countA ?  1 : 0;
}

//---------------------------------------------------------

void
TVariant::setParentForChilds() {
  if (m_type == List) {
    for(TVariantList::iterator i = m_list.begin(); i != m_list.end(); ++i)
      i->setParent(*this);
  } else
  if (m_type == Map) {
    for(TVariantMap::iterator i = m_map.begin(); i != m_map.end(); ++i)
      i->second.setParent(*this, i->first);
  }
}

//---------------------------------------------------------

const TVariant&
TVariant::blank() {
  static const TVariant blank;
  return blank;
}

//---------------------------------------------------------

void
TVariant::resize(int size) {
  setType(List);
  int prevSize = (int)m_list.size();
  if (prevSize == size) return;
  m_list.resize(size);
  if (prevSize < size)
    for(TVariantList::iterator i = m_list.begin() + prevSize; i != m_list.end(); ++i)
      i->setParent(*this);
  touch();
}

//---------------------------------------------------------

void
TVariant::insert(int index, const TVariant &v) {
  resize(std::max((int)m_list.size(), index));
  m_list.insert(m_list.begin() + index, v);
  m_list[index].setParent(*this);
  touch();
}

//---------------------------------------------------------

void
TVariant::remove(int index) {
  if (m_type == List && index >= 0 && index < (int)m_list.size())
    { m_list.erase(m_list.begin() + index); touch(); }
}

//---------------------------------------------------------

TVariant&
TVariant::operator[] (int index) {
  setType(List);
  assert(index >= 0);
  int prevSize = (int)m_list.size();
  if (index >= prevSize) {
    m_list.resize(index + 1);
    for(TVariantList::iterator i = m_list.begin() + prevSize; i != m_list.end(); ++i)
      i->setParent(*this);
    touch();
  }
  return m_list[index];
}

//---------------------------------------------------------

TVariant&
TVariant::operator[] (const TStringId &field) {
  setType(Map);
  TVariant &result = m_map[field];
  if (!result.m_parent) result.setParent(*this, field);
  return result;
}

//---------------------------------------------------------

bool
TVariant::remove(const TStringId &field) {
  if (m_type == Map && m_map.erase(field))
    { touch(); return true; }
  return false;
}

//---------------------------------------------------------

const TVariant&
TVariant::byPath(const TVariantPath &path, int begin, int end) const {
  if ((int)path.size() <= begin || begin >= end) return *this;
  if (isNone()) return blank();
  return byPath(path[begin]).byPath(path, begin + 1, end);
}

//---------------------------------------------------------

TVariant&
TVariant::byPath(const TVariantPath &path, int begin, int end) {
  if ((int)path.size() <= begin || begin >= end) return *this;
  return byPath(path[begin]).byPath(path, begin + 1, end);
}

//---------------------------------------------------------

int
TVariant::getPathSize() const {
  const TVariant *a = this->m_parent;
  int ac = 0;
  while(a) a = a->m_parent, ++ac;
  return ac;
}

//---------------------------------------------------------

void
TVariant::getParentPath(TVariantPath &outPath) const {
  if (m_parent) {
    m_parent->getParentPath(outPath);
    outPath.push_back(parentPathEntry());
  } else {
    outPath.clear();
  }
}

//---------------------------------------------------------

bool
TVariant::isChildOf(const TVariant &other) const {
  for(const TVariant *a = this->m_parent; a; a = a->m_parent)
    if (a == &other) return true;
  return false;
}

//---------------------------------------------------------

bool
TVariant::isChildOrEqual(const TVariant &other) const {
  for(const TVariant *a = this; a; a = a->m_parent)
    if (a == &other) return true;
  return false;
}

//---------------------------------------------------------

const TVariant*
TVariant::findCommonParent(const TVariant &other) const {
  if (m_root != other.m_root) return NULL;
  const TVariant *a = this, *b = &other;
  int ac = 0, bc = 0;
  while(a->m_parent) a = a->m_parent, ++ac;
  while(b->m_parent) b = b->m_parent, ++bc;

  a = this, b = &other;
  while(ac > bc) a = a->m_parent, --ac;
  while(bc > ac) b = b->m_parent, --bc;

  while(true) {
    if (a == b) return a;
    if (ac == 0) break;
    --ac, a = a->m_parent, b = b->m_parent;
  }

  return NULL;
}
