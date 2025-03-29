#pragma once

#ifndef TPROPERTY_INCLUDED
#define TPROPERTY_INCLUDED

#include "tconvert.h"
#include "tstringid.h"
#include "tpixel.h"

#include <cstdint>

#undef DVAPI
#undef DVVAR

#ifdef TNZCORE_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

template <class T>
class TRangeProperty;

typedef TRangeProperty<int> TIntProperty;
typedef TRangeProperty<double> TDoubleProperty;

class DVAPI TBoolProperty;
class DVAPI TStringProperty;
class DVAPI TEnumProperty;
class DVAPI TDoublePairProperty;
class DVAPI TIntPairProperty;
class DVAPI TStyleIndexProperty;
class DVAPI TPointerProperty;
class DVAPI TColorChipProperty;

class TIStream;
class TOStream;

//---------------------------------------------------------

class DVAPI TProperty {
public:
  class Visitor {
  public:
    virtual void visit(TDoubleProperty *p)     = 0;
    virtual void visit(TIntProperty *p)        = 0;
    virtual void visit(TBoolProperty *p)       = 0;
    virtual void visit(TStringProperty *p)     = 0;
    virtual void visit(TEnumProperty *p)       = 0;
    virtual void visit(TDoublePairProperty *p) = 0;
    virtual void visit(TIntPairProperty *p)    = 0;
    virtual void visit(TStyleIndexProperty *p) = 0;
    virtual void visit(TPointerProperty *p)    = 0;
    virtual void visit(TColorChipProperty *p)  = 0;
    virtual ~Visitor() {}
  };

  class Listener {
  public:
    virtual void onPropertyChanged() = 0;
    virtual ~Listener() {}
  };

  // eccezioni
  class TypeError {};
  class RangeError {};

  TProperty(std::string name):
    m_name(name),
    m_uiNameOrig(name),
    m_visible(true),
    m_qstringName(QString::fromStdString(name))
    { }

  virtual ~TProperty() {}

  virtual TProperty *clone() const = 0;

  // Used only for translation in Qt. Original not translated name.
  const std::string& getUINameOrig() const { return m_uiNameOrig; }
  void setUINameOrig(const std::string &str) { m_uiNameOrig = str; }

  // Used only for translation in Qt
  QString getQStringName() const { return m_qstringName; }
  void setQStringName(const QString &str) { m_qstringName = str; }
  virtual void assignUIName(TProperty *refP);

  std::string getName() const { return m_name.str(); }
  TStringId getNameId() const { return m_name; }
  virtual std::string getValueAsString() = 0;

  virtual void accept(Visitor &v) = 0;

  void addListener(Listener *listener);
  void removeListener(Listener *listener);
  void notifyListeners() const;

  // Used to pass action name
  std::string getId() const { return m_id; }
  void setId(std::string id) { m_id = id; }

  bool getVisible() const { return m_visible; }
  void setVisible(bool state) { m_visible = state; }

private:
  TStringId m_name;
  std::string m_uiNameOrig;
  QString m_qstringName;
  std::string m_id;
  std::vector<Listener *> m_listeners;
  bool m_visible;
};

//---------------------------------------------------------

template <class T>
class TRangeProperty final : public TProperty {
public:
  typedef std::pair<T, T> Range;

  TRangeProperty(std::string name, T minValue, T maxValue, T value,
                 bool isMaxRangeLimited = true)
      : TProperty(name)
      , m_range(minValue, maxValue)
      , m_value(minValue)
      , m_isMaxRangeLimited(isMaxRangeLimited)
      , m_isLinearSlider(true) {
    setValue(value);
  }

  TProperty *clone() const override { return new TRangeProperty<T>(*this); }

  Range getRange() const { return m_range; }

  void setValue(T v, bool cropEnabled = false) {
    if (cropEnabled && m_isMaxRangeLimited)
      v = tcrop(v, m_range.first, m_range.second);
    if (cropEnabled && !m_isMaxRangeLimited)
      v = v < m_range.first ? m_range.first : v;
    if (v < m_range.first || (v > m_range.second && m_isMaxRangeLimited))
      throw RangeError();
    m_value = v;
  }

  T getValue() const { return m_value; }

  std::string getValueAsString() override { return std::to_string(m_value); }

  void accept(Visitor &v) override { v.visit(this); }

  bool isMaxRangeLimited() const { return m_isMaxRangeLimited; }

  void setNonLinearSlider() { m_isLinearSlider = false; }
  bool isLinearSlider() { return m_isLinearSlider; }

private:
  Range m_range;
  T m_value;
  bool m_isMaxRangeLimited;
  bool m_isLinearSlider;
};

//---------------------------------------------------------
#ifdef _WIN32
template class DVAPI TRangeProperty<int>;
template class DVAPI TRangeProperty<double>;
#endif
//---------------------------------------------------------

class TDoublePairProperty final : public TProperty {
public:
  typedef std::pair<double, double> Range;
  typedef std::pair<double, double> Value;

  TDoublePairProperty(std::string name, double minValue, double maxValue,
                      double v0, double v1, bool isMaxRangeLimited = true)
      : TProperty(name)
      , m_range(Range(minValue, maxValue))
      , m_isMaxRangeLimited(isMaxRangeLimited)
      , m_isLinearSlider(true) {
    setValue(Value(v0, v1));
  }

  TProperty *clone() const override { return new TDoublePairProperty(*this); }

  Range getRange() const { return m_range; }

  bool isMaxRangeLimited() const { return m_isMaxRangeLimited; }

  void setValue(const Value &value) {
    if (value.first < m_range.first ||
        (m_isMaxRangeLimited && value.first > m_range.second) ||
        value.second < m_range.first ||
        (m_isMaxRangeLimited && value.second > m_range.second))
      throw RangeError();
    m_value = value;
  }
  Value getValue() const { return m_value; }
  std::string getValueAsString() override {
    return std::to_string(m_value.first) + "," + std::to_string(m_value.second);
  }
  void accept(Visitor &v) override { v.visit(this); };

  void setNonLinearSlider() { m_isLinearSlider = false; }
  bool isLinearSlider() { return m_isLinearSlider; }

private:
  Range m_range;
  Value m_value;
  bool m_isMaxRangeLimited;
  bool m_isLinearSlider;
};

//---------------------------------------------------------

class TIntPairProperty final : public TProperty {
public:
  typedef std::pair<int, int> Range;
  typedef std::pair<int, int> Value;

  TIntPairProperty(std::string name, int minValue, int maxValue, int v0, int v1,
                   bool isMaxRangeLimited = true)
      : TProperty(name)
      , m_range(minValue, maxValue)
      , m_isMaxRangeLimited(isMaxRangeLimited)
      , m_isLinearSlider(true) {
    setValue(Value(v0, v1));
  }

  TProperty *clone() const override { return new TIntPairProperty(*this); }

  Range getRange() const { return m_range; }

  bool isMaxRangeLimited() const { return m_isMaxRangeLimited; }

  void setValue(const Value &value) {
    if (value.first < m_range.first ||
        (m_isMaxRangeLimited && value.first > m_range.second) ||
        value.second < m_range.first ||
        (m_isMaxRangeLimited && value.second > m_range.second))
      throw RangeError();
    m_value = value;
  }
  Value getValue() const { return m_value; }
  std::string getValueAsString() override {
    return std::to_string(m_value.first) + "," + std::to_string(m_value.second);
  }
  void accept(Visitor &v) override { v.visit(this); };

  void setNonLinearSlider() { m_isLinearSlider = false; }
  bool isLinearSlider() { return m_isLinearSlider; }

private:
  Range m_range;
  Value m_value;
  bool m_isMaxRangeLimited;
  bool m_isLinearSlider;
};

//---------------------------------------------------------

class DVAPI TBoolProperty final : public TProperty {
public:
  TBoolProperty(std::string name, bool value)
      : TProperty(name), m_value(value) {}

  TProperty *clone() const override { return new TBoolProperty(*this); }

  void setValue(bool v) { m_value = v; }
  bool getValue() const { return m_value; }
  std::string getValueAsString() override { return std::to_string(m_value); }
  void accept(Visitor &v) override { v.visit(this); };

private:
  bool m_value;
};

//---------------------------------------------------------

class DVAPI TStringProperty final : public TProperty {
public:
  TStringProperty(std::string name, std::wstring value)
      : TProperty(name), m_value(value) {}

  TProperty *clone() const override { return new TStringProperty(*this); }

  void setValue(std::wstring v) { m_value = v; }
  std::wstring getValue() const { return m_value; }
  std::string getValueAsString() override { return ::to_string(m_value); }
  void accept(Visitor &v) override { v.visit(this); };

private:
  std::wstring m_value;
};

//---------------------------------------------------------

class DVAPI TStyleIndexProperty final : public TProperty {
  int m_styleIndex = -1;

public:
  TStyleIndexProperty(std::string name, std::wstring value)
      : TProperty(name), m_value(value) {}

  TProperty *clone() const override { return new TStyleIndexProperty(*this); }

  void setValue(std::wstring v) { m_value = v; }
  void setStyleIndex(int index) { m_styleIndex = index; }
  int getStyleIndex() { return m_styleIndex; }
  std::wstring getValue() const { return m_value; }

  std::string getValueAsString() override { return ::to_string(m_value); }

  void accept(Visitor &v) override { v.visit(this); };

private:
  std::wstring m_value;
};

//------------------------------------------------------------------

class DVAPI TPointerProperty final : public TProperty {
public:
  TPointerProperty(std::string name, void *value)
      : TProperty(name), m_value(value) {}

  TProperty *clone() const override { return new TPointerProperty(*this); }

  void setValue(void *v) { m_value = v; }
  void *getValue() const { return m_value; }

  std::string getValueAsString() override { return ::to_string(m_value); }

  void accept(Visitor &v) override { v.visit(this); };

private:
  void *m_value;
};

//---------------------------------------------------------

class DVAPI TEnumProperty final : public TProperty {
public:
  typedef std::vector<std::wstring> Range;
  // Used only for translation and styling in Qt
  struct Item {
    QString UIName;
    QString iconName;

    Item(const QString &name = QString(), const QString &icon = QString())
        : UIName(name), iconName(icon) {}
  };
  typedef std::vector<Item> Items;

  TEnumProperty(const std::string &name) : TProperty(name), m_index(-1) {}

  TEnumProperty(const std::string &name, const Range &range,
                const std::wstring &v)
      : TProperty(name), m_range(range), m_index(indexOf(v)) {
    if (m_index < 0) throw RangeError();
    m_items.resize(m_range.size());
  }

  TEnumProperty(const std::string &name, Range::const_iterator i0,
                Range::const_iterator i1, const std::wstring &v)
      : TProperty(name), m_range(i0, i1), m_index(indexOf(v)) {
    if (m_index < 0) throw RangeError();
    m_items.resize(m_range.size());
  }

  TProperty *clone() const override { return new TEnumProperty(*this); }

  int indexOf(const std::wstring &value) {
    Range::const_iterator it = std::find(m_range.begin(), m_range.end(), value);
    return (it == m_range.end()) ? -1 : it - m_range.begin();
  }

  bool isValue(const std::wstring &value) {
    bool ret =
        std::find(m_range.begin(), m_range.end(), value) != m_range.end();
    return ret;
  }

  void addValueWithUIName(std::wstring value, const QString &name, const QString &iconName = QString()) {
    if (m_index == -1) m_index = 0;
    m_range.push_back(value);
    m_items.push_back(Item(name, iconName));
  }

  void addValue(std::wstring value, const QString &iconName = QString())
    { addValueWithUIName(value, QString::fromStdWString(value), iconName); }

  void setItemUIName(std::wstring value, const QString &name) {
    int index = indexOf(value);
    if (index < 0 || index >= (int)m_items.size()) throw RangeError();
    m_items[index].UIName = name;
  }

  void deleteAllValues() {
    m_range.clear();
    m_items.clear();
    m_index = -1;
  }

  void setIndex(int index) {
    if (index < 0 || index >= (int)m_range.size()) throw RangeError();
    m_index = index;
  }

  void setValue(const std::wstring &value) {
    int idx = indexOf(value);
    // if (idx < 0) throw RangeError();
    if (idx < 0)
      idx = 0;  // Avoid exception if program's item list doesn't contain
                //  the selected item in scene file
    m_index = idx;
  }

  int getCount() const { return (int)m_items.size(); }

  const Range &getRange() const { return m_range; }
  const Items &getItems() const { return m_items; }

  std::wstring getValue() const {
    return (m_index < 0) ? L"" : m_range[m_index];
  }
  std::string getValueAsString() override {
    return ::to_string(m_range[m_index]);
  }

  int getIndex() const { return m_index; }

  void accept(Visitor &v) override { v.visit(this); }

  static void enableRangeSaving(bool on);
  static bool isRangeSavingEnabled();

  void assignUIName(TProperty *refP) override;

private:
  Range m_range;
  Items m_items;
  int m_index;
};

//---------------------------------------------------------

class DVAPI TColorChipProperty final : public TProperty {
public:
  typedef std::vector<std::wstring> Range;
  struct ColorChip {
    QString UIName;
    TPixel32 pixelColor;

    ColorChip(const QString &name   = QString(),
              const TPixel32 &color = TPixel32(0, 0, 0))
        : UIName(name), pixelColor(color) {}
  };
  typedef std::vector<ColorChip> ColorChips;

  TColorChipProperty(const std::string &name) : TProperty(name), m_index(-1) {}

  TProperty *clone() const override { return new TColorChipProperty(*this); }

  int indexOf(const std::wstring &value) {
    Range::const_iterator it = std::find(m_range.begin(), m_range.end(), value);
    return (it == m_range.end()) ? -1 : it - m_range.begin();
  }

  int indexOf(TPixel32 color) {
    ColorChips::const_iterator it;
    for (it = m_chips.begin(); it != m_chips.end(); it++) {
      ColorChip chip = *it;
      if (chip.pixelColor == color) break;
    }
    return (it == m_chips.end()) ? -1 : it - m_chips.begin();
  }

  bool isValue(const std::wstring &value) {
    bool ret =
        std::find(m_range.begin(), m_range.end(), value) != m_range.end();
    return ret;
  }

  void addValue(std::wstring value, const TPixel32 &color) {
    if (m_index == -1) m_index = 0;
    m_range.push_back(value);
    m_chips.push_back(ColorChip(QString::fromStdWString(value), color));
  }

  void setItemUIName(std::wstring value, const QString &name) {
    int index = indexOf(value);
    if (index < 0 || index >= (int)m_chips.size()) throw RangeError();
    m_chips[index].UIName = name;
  }

  void deleteAllValues() {
    m_range.clear();
    m_chips.clear();
    m_index = -1;
  }

  void setIndex(int index) {
    if (index < 0 || index >= (int)m_chips.size()) throw RangeError();
    m_index = index;
  }

  void setValue(const std::wstring &value) {
    int idx = indexOf(value);
    if (idx < 0) throw RangeError();
    m_index = idx;
  }

  void setColor(TPixel32 color) {
    int idx = indexOf(color);
    if (idx < 0) throw RangeError();
    m_index = idx;
  }

  int getCount() const { return (int)m_chips.size(); }

  const Range &getRange() const { return m_range; }
  const ColorChips &getColorChips() const { return m_chips; }

  std::wstring getValue() const {
    return (m_index < 0) ? L"" : m_range[m_index];
  }
  std::string getValueAsString() override {
    return ::to_string(m_range[m_index]);
  }
  TPixel32 getColorValue() const {
    return (m_index < 0) ? TPixel32(0, 0, 0) : m_chips[m_index].pixelColor;
  }

  int getIndex() const { return m_index; }

  void accept(Visitor &v) override { v.visit(this); }

  void assignUIName(TProperty *refP) override;

private:
  Range m_range;
  ColorChips m_chips;
  int m_index;
};

//---------------------------------------------------------

class DVAPI TPropertyGroup {
public:
  typedef std::vector<std::pair<TProperty *, bool>> PropertyVector;
  typedef std::map<TStringId, TProperty *> PropertyTable;

  // exception
  class PropertyNotFoundError {};

  TPropertyGroup();
  virtual ~TPropertyGroup();

  virtual TPropertyGroup *clone() const;

  //! get ownership
  void add(TProperty *p);

  //! don't get ownership
  void bind(TProperty &p);

  //! returns 0 if the property doesn't exist
  TProperty *getProperty(const TStringId &name);
  TProperty *getProperty(const std::string &name)
    { return getProperty(TStringId::find(name)); }
  TProperty *getProperty(int i)
    { return (i >= (int)m_properties.size()) ? 0 : m_properties[i].first; }

  void setProperties(TPropertyGroup *g);

  void accept(TProperty::Visitor &v);

  int getPropertyCount() const { return (int)m_properties.size(); }

  void loadData(TIStream &is);
  void saveData(TOStream &os) const;

  void clear();

  // for adding translation to file writers properties
  virtual void updateTranslation(){};

  void assignUINames(TPropertyGroup *refPg);

private:
  PropertyTable m_table;
  PropertyVector m_properties;

private:
  // not implemented
  TPropertyGroup(const TPropertyGroup &);
  TPropertyGroup &operator=(const TPropertyGroup &);
};

//---------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
