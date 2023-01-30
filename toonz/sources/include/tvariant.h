#pragma once

#ifndef TVARIANT_INCLUDED
#define TVARIANT_INCLUDED

#include <tcommon.h>
#include <tstringid.h>

#include <string>
#include <vector>
#include <map>

#undef DVAPI
#undef DVVAR
#ifdef TVARIANT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------

class TVariant;
typedef std::vector<TVariant> TVariantList;
typedef std::map<TStringId, TVariant> TVariantMap;

//-------------------------------------------------------------------

class DVAPI TVariantPathEntry {
private:
  int m_index;
  TStringId m_field;
public:
  inline explicit TVariantPathEntry(int index = -1):
    m_index(index) { }
  inline explicit TVariantPathEntry(const TStringId &field):
    m_index(-1), m_field(field) { }
  inline explicit TVariantPathEntry(const std::string &fieldName):
    m_index(-1), m_field(fieldName) { }

  inline bool isIndex() const
    { return m_index >= 0; }
  inline bool isField() const
    { return !isIndex(); }
  inline int index() const
    { return m_index; }
  inline int field() const
    { return m_field; }

  inline bool operator== (const TVariantPathEntry &other) const
    { return m_index == other.m_index && m_field == other.m_field; }
  inline bool operator!= (const TVariantPathEntry &other) const
    { return m_index != other.m_index || m_field != other.m_field; }
  inline bool operator< (const TVariantPathEntry &other) const
    { return m_index < other.m_index || m_field < other.m_field; }

  inline void set(int index)
    { m_index = index; m_field.reset(); }
  inline void set(const TStringId &field)
    { m_index = 0; m_field = field; }
  inline void set(const std::string &fieldName)
    { m_index = 0; m_field.set(fieldName); }
};

//-------------------------------------------------------------------

class DVAPI TVariantPath: public std::vector<TVariantPathEntry> {
public:
  inline bool isSubPathOf(const TVariantPath &other) const
    { return compare(*this, 0, other, 0, (int)size()); }
  inline bool isBasePathOf(const TVariantPath &other) const
    { return other.isSubPathOf(*this); }
  inline int compare(const TVariantPath &other) const
    { return compare(*this, 0, other, 0, (int)std::max(size(), other.size())); }
  inline bool operator==(const TVariantPath &other) const
    { return compare(other) == 0; }
  inline bool operator!=(const TVariantPath &other) const
    { return compare(other) != 0; }
  inline bool operator<(const TVariantPath &other) const
    { return compare(other) < 0; }

  static int compare(
    const TVariantPath &a, int beginA,
    const TVariantPath &b, int beginB, int count );
};

//-------------------------------------------------------------------

class DVAPI TVariantOwner {
public:
  virtual ~TVariantOwner() { }
  virtual void onTouchVariant(const TVariant &value) { }
};

//-------------------------------------------------------------------

class DVAPI TVariant {
public:
  enum Type {
    None,
    Bool,
    Double,
    String,
    List,
    Map
  };

private:
  Type         m_type;
  bool         m_bool;
  double       m_double;
  std::string  m_string;
  TVariantList m_list;
  TVariantMap  m_map;

  TVariantOwner* m_owner;
  TVariant *m_root;
  TVariant *m_parent;
  TStringId m_parentField;

  void setParentForChilds();
  inline void setParent(TVariant &parent, const TStringId &parentField = TStringId()) {
    m_root = parent.m_root;
    m_parent = &parent;
    m_parentField = parentField;
    setParentForChilds();
  }

public:
  static const TVariant& blank();

  inline TVariant():
    m_type(None),
    m_bool(),
    m_double(),
    m_owner(),
    m_root(this),
    m_parent() { }
  inline TVariant(const TVariant &other):
    m_type(None),
    m_bool(),
    m_double(),
    m_owner(),
    m_root(this),
    m_parent() { *this = other; }
  inline explicit TVariant(TVariantOwner &owner, const TVariant &v = TVariant()):
    m_type(None),
    m_bool(),
    m_double(),
    m_owner(),
    m_root(this),
    m_parent() { *this = v; m_owner = &owner; }
  inline explicit TVariant(Type t):
    m_type(t),
    m_bool(),
    m_double(),
    m_owner(),
    m_root(this),
    m_parent() { }
  inline explicit TVariant(bool v):
    m_type(Bool),
    m_bool(v),
    m_double(),
    m_owner(),
    m_root(this),
    m_parent() { }
  inline explicit TVariant(double v):
    m_type(Double),
    m_bool(),
    m_double(v),
    m_owner(),
    m_root(this),
    m_parent() { }
  inline explicit TVariant(const std::string &v):
    m_type(String),
    m_bool(),
    m_double(),
    m_string(v),
    m_owner(),
    m_root(this),
    m_parent() { }
  inline explicit TVariant(const TVariantList &v):
    m_type(None),
    m_bool(),
    m_double(),
    m_owner(),
    m_root(this),
    m_parent() { setList(v); }
  inline explicit TVariant(const TVariantMap &v):
    m_type(None),
    m_bool(),
    m_double(),
    m_owner(),
    m_root(this),
    m_parent() { setMap(v); }

  inline void touch()
    { if (m_root->m_owner) m_root->m_owner->onTouchVariant(*this); }

  inline TVariant& operator=(const TVariant &other) {
    switch(other.m_type) {
    case Bool   : setBool(other.m_bool); break;
    case Double : setDouble(other.m_double); break;
    case String : setString(other.m_string); break;
    case List   : setList(other.m_list); break;
    case Map    : setMap(other.m_map); break;
    default     : reset(); break;
    }
    return *this;
  }

  inline void clear() {
    m_bool = bool();
    m_double = double();
    m_string.clear();
    m_list.clear();
    m_map.clear();
    touch();
  }
  inline void setType(Type t)
    { if (m_type != t) { m_type = t; clear(); } }
  inline void reset()
    { setType(None); }

  inline Type getType() const
    { return m_type; }
  inline bool isNone() const
    { return m_type == None; }
  inline bool getBool() const
    { return m_bool; }
  inline double getDouble() const
    { return m_double; }
  inline const std::string& getString() const
    { return m_string; }
  inline const TVariantList& getList() const
    { return m_list; }
  inline const TVariantMap& getMap() const
    { return m_map; }

  inline void setNone()
    { reset(); }
  inline void setBool(bool v)
    { setType(Bool); m_bool = v; touch(); }
  inline void setDouble(double v)
    { setType(Double); m_double = v; touch(); }
  inline void setString(const std::string &v)
    { setType(String); m_string = v; touch(); }
  inline void setList(const TVariantList &v)
    { setType(List); m_list = v; setParentForChilds(); touch(); }
  inline void setMap(const TVariantMap &v)
    { setType(Map); m_map = v; setParentForChilds(); touch(); }

  // list methods
  void resize(int size);
  void insert(int index, const TVariant &v);
  void remove(int index);
  TVariant& operator[] (int index);
  inline int size() const
    { return (int)(m_type == List ? m_list.size() : m_map.size()); }
  inline void clearList()
    { resize(0); }
  inline void append(const TVariant &v)
    { insert((int)m_list.size(), v); }
  inline const TVariant& operator[] (int index) const {
    assert(index >= 0);
    return index < (int)m_list.size() ? m_list[index] : blank();
  }

  // map methods
  TVariant& operator[] (const TStringId &field);
  bool remove(const TStringId &field);
  inline bool contains(const TStringId &field) const
    { return m_type == Map && m_map.count(field); }
  inline const TVariant& operator[] (const TStringId &field) const {
    TVariantMap::const_iterator i = m_map.find(field);
    return i == m_map.end() ? blank() : i->second;
  }
  inline bool contains(const std::string &field) const
    { return contains(TStringId::find(field)); }
  inline const TVariant& operator[] (const std::string &field) const
    { return (*this)[TStringId::find(field)]; }
  inline TVariant& operator[] (const std::string &field)
    { return (*this)[TStringId::find(field)]; }
  inline void remove(const std::string &field)
    { remove(TStringId::find(field)); }

  // path methods
  const TVariant& byPath(const TVariantPath &path, int begin, int end) const;
  TVariant& byPath(const TVariantPath &path, int begin, int end);
  inline const TVariant& byPath(const TVariantPathEntry &entry) const {
    return entry.isIndex()
         ? (m_type == List ? (*this)[entry.index()] : blank())
         : (m_type == Map  ? (*this)[entry.field()] : blank());
  }
  inline TVariant& byPath(const TVariantPathEntry &entry)
    { return entry.isIndex() ? (*this)[entry.index()] : (*this)[entry.field()]; }
  inline const TVariant& byPath(const TVariantPath &path, int begin = 0) const
    { return byPath(path, begin, (int)path.size()); }
  inline TVariant& byPath(const TVariantPath &path, int begin = 0)
    { return byPath(path, begin, (int)path.size()); }
  inline const TVariant& operator[] (const TVariantPath &path) const
    { return byPath(path); }
  inline TVariant& operator[] (const TVariantPath &path)
    { return byPath(path); }

  // hierarchy methods
  inline const TVariantOwner* owner() const
    { return m_root->m_owner; }
  inline TVariantOwner* owner()
    { return m_root->m_owner; }
  inline const TVariant& root() const
    { return *m_root; }
  inline TVariant& root()
    { return *m_root; }
  inline const TVariant* parent() const
    { return m_parent; }
  inline TVariant* parent()
    { return m_parent; }
  inline int parentIndex() const
    { return m_parent || !m_parentField ? this - &m_parent->m_list.front() : 0; }
  inline const TStringId& parentField() const
    { return m_parentField; }

  int getPathSize() const;
  void getParentPath(TVariantPath &outPath) const;
  inline TVariantPathEntry parentPathEntry() const {
    return !m_parent               ? TVariantPathEntry()
         : m_parent->m_type == Map ? TVariantPathEntry(m_parentField)
         : TVariantPathEntry( this - &m_parent->m_list.front() );
  }
  inline TVariantPath getParentPath() const
    { TVariantPath path; getParentPath(path); return path; }

  bool isChildOf(const TVariant &other) const;
  bool isChildOrEqual(const TVariant &other) const;
  inline bool isParentOf(const TVariant &other) const
    { return other.isChildOf(*this); }
  inline bool isParentOrEqual(const TVariant &other) const
    { return other.isChildOrEqual(*this); }
  inline bool isChildOrParent(const TVariant &other) const
    { return isChildOrEqual(other) || isParentOrEqual(other); }

  const TVariant* findCommonParent(const TVariant &other) const;

  // serialization
  // TODO:
};

#endif
