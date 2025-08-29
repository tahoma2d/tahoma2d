

#include "tproperty.h"
#include "tstream.h"
#include "texception.h"
#include "tcurves.h"
// #include "tconvert.h"

void TProperty::addListener(Listener *listener) {
  if (std::find(m_listeners.begin(), m_listeners.end(), listener) ==
      m_listeners.end())
    m_listeners.push_back(listener);
}

void TProperty::removeListener(Listener *listener) {
  m_listeners.erase(
      std::remove(m_listeners.begin(), m_listeners.end(), listener),
      m_listeners.end());
}

void TProperty::notifyListeners() const {
  std::vector<Listener *>::const_iterator it;
  for (it = m_listeners.begin(); it != m_listeners.end(); ++it)
    (*it)->onPropertyChanged();
}

void TProperty::assignUIName(TProperty *refP) {
  m_qstringName = refP->getQStringName();
}

//=============================================================================

TPropertyGroup::TPropertyGroup() {}

TPropertyGroup::~TPropertyGroup() {
  for (PropertyVector::iterator it = m_properties.begin();
       it != m_properties.end(); ++it)
    if (it->second) delete it->first;
}

void TPropertyGroup::clear() {
  m_properties.clear();
  m_table.clear();
}

TPropertyGroup *TPropertyGroup::clone() const {
  TPropertyGroup *g = new TPropertyGroup();
  for (PropertyVector::const_iterator i = m_properties.begin();
       i != m_properties.end(); ++i)
    g->add(i->first->clone());
  return g;
}

void TPropertyGroup::add(TProperty *p) {
  const TStringId &name = p->getNameId();
  assert(m_table.find(name) == m_table.end());
  m_properties.push_back(std::make_pair(p, true));
  m_table[name] = p;
}

void TPropertyGroup::bind(TProperty &p) {
  const TStringId &name = p.getNameId();
  assert(m_table.find(name) == m_table.end());
  m_properties.push_back(std::make_pair(&p, false));
  m_table[name] = &p;
}

TProperty *TPropertyGroup::getProperty(const TStringId &name) {
  PropertyTable::iterator i = m_table.find(name);
  if (i == m_table.end())
    return 0;
  else
    return i->second;
}

template <class Property>
void assign(Property *dst, TProperty *src) {
  Property *s = dynamic_cast<Property *>(src);
  if (!s) throw TProperty::TypeError();
  dst->setValue(s->getValue());
}

class Setter final : public TProperty::Visitor {
  TProperty *m_src;

public:
  Setter(TProperty *src) : m_src(src) {}

  void visit(TDoubleProperty *dst) override { assign(dst, m_src); }
  void visit(TIntProperty *dst) override { assign(dst, m_src); }
  void visit(TStringProperty *dst) override { assign(dst, m_src); }
  void visit(TBoolProperty *dst) override { assign(dst, m_src); }
  void visit(TEnumProperty *dst) override { assign(dst, m_src); }
  void visit(TDoublePairProperty *dst) override { assign(dst, m_src); }
  void visit(TIntPairProperty *dst) override { assign(dst, m_src); }
  void visit(TStyleIndexProperty *dst) override { assign(dst, m_src); }
  void visit(TPointerProperty *dst) override { assign(dst, m_src); }
  void visit(TColorChipProperty *dst) override { assign(dst, m_src); }
  void visit(TStylusProperty *dst) override { assign(dst, m_src); }
};

void TPropertyGroup::setProperties(TPropertyGroup *g) {
  for (PropertyVector::const_iterator i = g->m_properties.begin();
       i != g->m_properties.end(); ++i) {
    TProperty *src = i->first;
    TProperty *dst = getProperty(src->getName());
    if (dst) {
      Setter setter(src);
      TProperty::Visitor *visitor = &setter;
      dst->accept(*visitor);
    }
  }
}

void TPropertyGroup::accept(TProperty::Visitor &v) {
  for (PropertyVector::const_iterator i = m_properties.begin();
       i != m_properties.end(); ++i)
    i->first->accept(v);
}

class PropertyWriter final : public TProperty::Visitor {
  TOStream &m_os;

public:
  PropertyWriter(TOStream &os) : m_os(os) {}

  void visit(TDoubleProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]  = "double";
    attr["name"]  = p->getName();
    attr["min"]   = std::to_string(p->getRange().first);
    attr["max"]   = std::to_string(p->getRange().second);
    attr["value"] = std::to_string(p->getValue());
    m_os.openCloseChild("property", attr);
  }
  void visit(TDoublePairProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]                     = "pair";
    attr["name"]                     = p->getName();
    attr["min"]                      = std::to_string(p->getRange().first);
    attr["max"]                      = std::to_string(p->getRange().second);
    TDoublePairProperty::Value value = p->getValue();
    attr["value"] =
        std::to_string(value.first) + " " + std::to_string(value.second);
    m_os.openCloseChild("property", attr);
  }
  void visit(TIntPairProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]                  = "pair";
    attr["name"]                  = p->getName();
    attr["min"]                   = std::to_string(p->getRange().first);
    attr["max"]                   = std::to_string(p->getRange().second);
    TIntPairProperty::Value value = p->getValue();
    attr["value"] =
        std::to_string(value.first) + " " + std::to_string(value.second);
    m_os.openCloseChild("property", attr);
  }
  void visit(TIntProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]  = "int";
    attr["name"]  = p->getName();
    attr["min"]   = std::to_string(p->getRange().first);
    attr["max"]   = std::to_string(p->getRange().second);
    attr["value"] = std::to_string(p->getValue());
    m_os.openCloseChild("property", attr);
  }
  void visit(TBoolProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]  = "bool";
    attr["name"]  = p->getName();
    attr["value"] = p->getValue() ? "true" : "false";
    m_os.openCloseChild("property", attr);
  }
  void visit(TStringProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]  = "string";
    attr["name"]  = p->getName();
    attr["value"] = ::to_string(p->getValue());
    m_os.openCloseChild("property", attr);
  }

  void visit(TStyleIndexProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]  = "string";
    attr["name"]  = p->getName();
    attr["value"] = p->getValueAsString();
    m_os.openCloseChild("property", attr);
  }

  void visit(TEnumProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]  = "enum";
    attr["name"]  = p->getName();
    attr["value"] = ::to_string(p->getValue());
    if (TEnumProperty::isRangeSavingEnabled()) {
      m_os.openChild("property", attr);
      std::vector<std::wstring> range = p->getRange();
      for (int i = 0; i < (int)range.size(); i++) {
        attr.clear();
        attr["value"] = ::to_string(range[i]);
        m_os.openCloseChild("item", attr);
      }
      m_os.closeChild();
    } else
      m_os.openCloseChild("property", attr);
  }
  void visit(TPointerProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]  = "pointer";
    attr["name"]  = p->getName();
    attr["value"] = p->getValueAsString();
    m_os.openCloseChild("property", attr);
  }

  void visit(TColorChipProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]  = "string";
    attr["name"]  = p->getName();
    attr["value"] = ::to_string(p->getValue());
    m_os.openCloseChild("property", attr);
  }

  void visit(TStylusProperty *p) override {
    std::map<std::string, std::string> attr;
    attr["type"]  = "stylus";
    attr["name"]  = p->getName();
    attr["value"] = ::to_string(p->getValue());
    m_os.openChild("property", attr);
    // Pressure
    attr.clear();
    attr["enabled"]    = p->isPressureEnabled() ? "true" : "false";
    QList<TPointD> pts = p->getPressureCurve();
    if (pts.isEmpty())
      m_os.openCloseChild("pressure", attr);
    else {
      m_os.openChild("pressure", attr);
      for (int i = 0; i < pts.count(); i++) {
        attr.clear();
        attr["x"] = std::to_string(pts[i].x);
        attr["y"] = std::to_string(pts[i].y);
        m_os.openCloseChild("curvePoint", attr);
      }
      m_os.closeChild();
    }
    // Tilt
    attr.clear();
    attr["enabled"]    = p->isTiltEnabled() ? "true" : "false";
    pts = p->getTiltCurve();
    if (pts.isEmpty())
      m_os.openCloseChild("tilt", attr);
    else {
      m_os.openChild("tilt", attr);
      for (int i = 0; i < pts.count(); i++) {
        attr.clear();
        attr["x"] = std::to_string(pts[i].x);
        attr["y"] = std::to_string(pts[i].y);
        m_os.openCloseChild("curvePoint", attr);
      }
      m_os.closeChild();
    }
    m_os.closeChild();
  }
};

void TPropertyGroup::loadData(TIStream &is) {
  for (PropertyVector::iterator it = m_properties.begin();
       it != m_properties.end(); ++it)
    if (it->second) delete it->first;
  m_properties.clear();
  m_table.clear();
  std::string tagName;
  while (is.matchTag(tagName)) {
    if (tagName == "property") {
      std::string name   = is.getTagAttribute("name");
      std::string type   = is.getTagAttribute("type");
      std::string svalue = is.getTagAttribute("value");
      if (name == "") throw TException("missing property name");
      if (type == "") throw TException("missing property type");
      if (type != "string" && svalue == "")
        throw TException("missing property value");
      if (type == "double") {
        double min = std::stod(is.getTagAttribute("min"));
        double max = std::stod(is.getTagAttribute("max"));
        add(new TDoubleProperty(name, min, max, std::stod(svalue)));
      } else if (type == "pair") {
        double min = std::stod(is.getTagAttribute("min"));
        double max = std::stod(is.getTagAttribute("max"));
        TDoublePairProperty::Value v(0, 0);
        int i = svalue.find(' ');
        if (i != (int)std::string::npos) {
          v.first  = std::stod(svalue.substr(0, i));
          v.second = std::stod(svalue.substr(i + 1));
        }
        add(new TDoublePairProperty(name, min, max, v.first, v.second));
      } else if (type == "int") {
        int min = std::stoi(is.getTagAttribute("min"));
        int max = std::stoi(is.getTagAttribute("max"));
        add(new TIntProperty(name, min, max, std::stoi(svalue)));
      } else if (type == "bool") {
        if (svalue != "true" && svalue != "false")
          throw TException("bad boolean property value");
        add(new TBoolProperty(name, svalue == "true" ? true : false));
      } else if (type == "string") {
        add(new TStringProperty(name, ::to_wstring(svalue)));
      } else if (type == "enum") {
        TEnumProperty *p = new TEnumProperty(name);
        if (is.isBeginEndTag())
          p->addValue(::to_wstring(svalue));
        else {
          while (is.matchTag(tagName)) {
            if (tagName == "item") {
              std::string item = is.getTagAttribute("value");
              p->addValue(::to_wstring(item));
            } else
              throw TException("expected range property <item>");
          }
          is.closeChild();
        }
        p->setValue(::to_wstring(svalue));
        add(p);
      } else if (type == "stylus") {
        TStylusProperty *p = new TStylusProperty(name);
        while (is.matchTag(tagName)) {
          if (tagName == "pressure") {
            p->setPressureEnabled(is.getTagAttribute("enabled") == "true" ? true : false);
            if (!is.isBeginEndTag()) {
              QList<TPointD> pts;
              while (is.matchTag(tagName)) {
                if (tagName == "curvePoint") {
                  double x = std::stod(is.getTagAttribute("x"));
                  double y = std::stod(is.getTagAttribute("y"));
                  pts.push_back(TPointD(x, y));
                } else
                  throw TException("unexpected pressure curve property");
              }
              p->setPressureCurve(pts);
              is.closeChild();
            }
          } else if (tagName == "tilt") {
            p->setTiltEnabled(
                is.getTagAttribute("enabled") == "true" ? true : false);
            if (!is.isBeginEndTag()) {
              QList<TPointD> pts;
              while (is.matchTag(tagName)) {
                if (tagName == "curvePoint") {
                  double x = std::stod(is.getTagAttribute("x"));
                  double y = std::stod(is.getTagAttribute("y"));
                  pts.push_back(TPointD(x, y));
                } else
                  throw TException("unexpected tilt curve property");
              }
              p->setTiltCurve(pts);
              is.closeChild();
            }
          } else
            throw TException("expected range property <item>");
        }
        is.closeChild();
        add(p);
      } else
        throw TException("unrecognized property type : " + type);
    } else
      throw TException("expected <property>");
    // is.closeChild();
  }
}

void TPropertyGroup::saveData(TOStream &os) const {
  PropertyWriter writer(os);
  const_cast<TPropertyGroup *>(this)->accept(writer);
}

void TPropertyGroup::assignUINames(TPropertyGroup *refPg) {
  for (PropertyVector::const_iterator i = m_properties.begin();
       i != m_properties.end(); ++i) {
    TProperty *refP = refPg->getProperty(i->first->getName());
    if (refP) i->first->assignUIName(refP);
  }
}

namespace {
bool EnumRangeSavingEnabled = true;
}

void TEnumProperty::enableRangeSaving(bool on) { EnumRangeSavingEnabled = on; }

bool TEnumProperty::isRangeSavingEnabled() { return EnumRangeSavingEnabled; }

void TEnumProperty::assignUIName(TProperty *refP) {
  setQStringName(refP->getQStringName());
  TEnumProperty *enumRefP = dynamic_cast<TEnumProperty *>(refP);
  if (!enumRefP) return;
  Items refItems = enumRefP->getItems();
  for (int i = 0; i < m_range.size(); i++) {
    int refIndex = enumRefP->indexOf(m_range[i]);
    if (0 <= refIndex) m_items[i].UIName = refItems[refIndex].UIName;
  }
}

void TColorChipProperty::assignUIName(TProperty *refP) {
  setQStringName(refP->getQStringName());
  TColorChipProperty *colorChipRefP = dynamic_cast<TColorChipProperty *>(refP);
  if (!colorChipRefP) return;
  ColorChips refChips = colorChipRefP->getColorChips();
  for (int i = 0; i < m_chips.size(); i++) {
    int refIndex = colorChipRefP->indexOf(m_chips[i].UIName.toStdWString());
    if (0 <= refIndex) m_chips[i].UIName = refChips[refIndex].UIName;
  }
}

TPointD evaluateSegment(const TCubic &seg, double t) {
  double u = 1 - t;
  TPointD p;
  p.x = u * u * u * seg.getP0().x + 3 * u * u * t * seg.getP1().x +
        3 * u * t * t * seg.getP2().x + t * t * t * seg.getP3().x;
  p.y = u * u * u * seg.getP0().y + 3 * u * u * t * seg.getP1().y +
        3 * u * t * t * seg.getP2().y + t * t * t * seg.getP3().y;
  return p;
}

bool findPointInSegment(const TCubic &seg, double x_target, double t0,
                        double t1, double &root) {
  double epsilon = 1e-6;
  TPointD p0     = evaluateSegment(seg, t0);
  TPointD p1     = evaluateSegment(seg, t1);

  if ((p0.x - x_target) * (p1.x - x_target) > 0) return false;

  for (int i = 0; i < 50; ++i) {
    double tm = 0.5 * (t0 + t1);
    double xm = evaluateSegment(seg, tm).x;
    if (std::fabs(xm - x_target) < epsilon) {
      root = tm;
      return true;
    }
    if ((p0.x - x_target) * (xm - x_target) < 0)
      t1 = tm;
    else
      t0 = tm;
  }

  root = 0.5 * (t0 + t1);
  return true;
}

double findYforX(double x, bool isLinearCurve, QList<TPointD> curve) {
  double y = x;

  int step = isLinearCurve ? 1 : 3;
  for (int i = 0; (i + step) < curve.count(); i += step) {
    TCubic seg;

    seg.setP0(curve[i]);
    seg.setP1(isLinearCurve ? curve[i] : curve[i + 1]);
    seg.setP2(isLinearCurve ? curve[i + 1] : curve[i + 2]);
    seg.setP3(isLinearCurve ? curve[i + 1] : curve[i + 3]);

    TPointD x0 = evaluateSegment(seg, 0.0);
    TPointD x1 = evaluateSegment(seg, 1.0);

    // Check if x is in this segment's x range
    if ((x >= x0.x && x <= x1.x) || (x >= x1.x && x <= x0.x)) {
      double t_found;
      if (findPointInSegment(seg, x, 0.0, 1.0, t_found)) {
        y = evaluateSegment(seg, t_found).y;
        break;
      }
    }
  }

  return y;
}

double TStylusProperty::getOutputPressureForInput(double pressure) {
  if (!m_pressureEnabled ||
      (m_pressureCurve.isEmpty() && m_defaultPressureCurve.isEmpty()))
    return pressure;

  QList<TPointD> curve =
      m_pressureCurve.isEmpty() ? m_defaultPressureCurve : m_pressureCurve;

  // Assumes pressure is in range 0 - 100%, returning 0 - 100%
  return findYforX(pressure, m_useLinearCurves, curve);
}

double TStylusProperty::getOutputTiltForInput(double tilt) {
  if (!m_tiltEnabled || (m_tiltCurve.isEmpty() && m_defaultTiltCurve.isEmpty()))
    return tilt;

  QList<TPointD> curve =
      m_tiltCurve.isEmpty() ? m_defaultTiltCurve : m_tiltCurve;

  // Assumes tilt is in range 30 - 90 degrees, returning 0 - 100%
  return findYforX(std::fabs(tilt), m_useLinearCurves, curve);
}
