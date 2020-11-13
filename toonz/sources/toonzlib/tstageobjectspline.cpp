

#include "toonz/tstageobjectspline.h"
#include "tconst.h"
#include "tstroke.h"
#include "tstream.h"
#include "tconvert.h"
#include "tdoubleparam.h"
#include "tdoublekeyframe.h"

#include <QDebug>
#include <QList>

//=============================================================================

PERSIST_IDENTIFIER(TStageObjectSpline, "pegbarspline")

DEFINE_CLASS_CODE(TStageObjectSpline, 19)

namespace {
int idBaseCode = 1;
}

//=============================================================================
//
// PosPathKeyframesUpdater
//
// When the spline changes its shape, the PosPathKeyframesUpdater update the
// PosPath channel keyframes.
// If an object is close to a spline control point it must remain close to it
// If the object is between two control points, then the ratio of the distances
// along the spline to the two control points remain the same.
//-----------------------------------------------------------------------------

class PosPathKeyframesUpdater {
  std::vector<double> m_oldControlPointsLengths;
  std::vector<double> m_newControlPointsLengths;
  double m_oldSplineLength, m_newSplineLength;

public:
  PosPathKeyframesUpdater(TStroke *oldSpline, TStroke *newSpline)
      : m_oldSplineLength(0), m_newSplineLength(0) {
    assert(oldSpline);
    assert(newSpline);
    m_oldSplineLength = oldSpline->getLength();
    m_newSplineLength = newSpline->getLength();
    int m;
    m = oldSpline->getControlPointCount();
    for (int i = 0; i < m; i += 4)
      m_oldControlPointsLengths.push_back(
          oldSpline->getLengthAtControlPoint(i));
    m = newSpline->getControlPointCount();
    for (int i = 0; i < m; i += 4)
      m_newControlPointsLengths.push_back(
          newSpline->getLengthAtControlPoint(i));
  }

  void update(TDoubleParam *param) {
    assert(m_newSplineLength > 0);
    if (m_newSplineLength <= 0) return;
    for (int j = 0; j < param->getKeyframeCount(); j++) {
      TDoubleKeyframe kf = param->getKeyframe(j);
      double s           = m_oldSplineLength * kf.m_value * 0.01;
      update(s);
      double updatedValue = s * 100.0 / m_newSplineLength;
      kf.m_value          = updatedValue;
      param->setKeyframe(j, kf);
    }
  }
  void update(double &s);
};

//-----------------------------------------------------------------------------

void PosPathKeyframesUpdater::update(double &s) {
  int k    = 0;
  int oldm = (int)m_oldControlPointsLengths.size();
  int newm = (int)m_newControlPointsLengths.size();
  while (k < oldm && m_oldControlPointsLengths[k] <= s) k++;
  if (k >= oldm) {
    // all CP lengths <=s
    if (oldm - 1 < newm)
      s = m_newControlPointsLengths[oldm - 1];
    else
      s = m_newSplineLength;
  } else if (k == 0) {
    // all CP lengths >s
    s = 0;
  } else {
    if (k >= newm)
      s = m_newSplineLength;
    else {
      double sa    = m_oldControlPointsLengths[k - 1];
      double sb    = m_oldControlPointsLengths[k];
      double newSa = m_newControlPointsLengths[k - 1];
      double newSb = m_newControlPointsLengths[k];
      assert(sa <= s && s < sb);
      if (sa >= sb) {
        s = (newSa + newSb) * 0.5;
      } else {
        s = newSa + (newSb - newSa) * (s - sa) / (sb - sa);
      }
    }
  }
}

//=============================================================================
// TStageObjectSpline

TStageObjectSpline::TStageObjectSpline()
    : TSmartObject(m_classCode)
    , m_stroke(0)
    , m_dagNodePos(TConst::nowhere)
    , m_id(-1)
    , m_idBase(std::to_string(idBaseCode++))
    , m_name("")
    , m_active(false)
    , m_color(0)
    , m_steps(10)
    , m_width(1)
    , m_isOpened(false) {
  double d = 30;
  std::vector<TThickPoint> points;
  points.push_back(TPointD(0, 0));
  points.push_back(TPointD(d, 0));
  points.push_back(TPointD(2.0 * d, 0));
  m_stroke = new TStroke(points);

  d = 250.0;

  m_interpolationStroke.push_back(TPointD(-40, 0));
  m_interpolationStroke.push_back(TPointD(-20, 0));
  m_interpolationStroke.push_back(TPointD(-20, 0));
  m_interpolationStroke.push_back(TPointD(0, 0));
  m_interpolationStroke.push_back(TPointD(65, 65));

  m_interpolationStroke.push_back(TPointD(935, 935));
  m_interpolationStroke.push_back(TPointD(1000, 1000));
  m_interpolationStroke.push_back(TPointD(1020, 1000));
  m_interpolationStroke.push_back(TPointD(1020, 1000));
  m_interpolationStroke.push_back(TPointD(1040, 1000));
}

//-----------------------------------------------------------------------------

TStageObjectSpline::~TStageObjectSpline() {
  delete m_stroke;
  m_interpolationStroke.clear();
  for (int i = 0; i < (int)m_posPathParams.size(); i++)
    m_posPathParams[i]->release();
  m_posPathParams.clear();
}

//-----------------------------------------------------------------------------

TStageObjectSpline *TStageObjectSpline::clone() const {
  TStageObjectSpline *clonedSpline    = new TStageObjectSpline();
  clonedSpline->m_id                  = m_id;
  clonedSpline->m_name                = m_name;
  clonedSpline->m_stroke              = new TStroke(*m_stroke);
  clonedSpline->m_interpolationStroke = m_interpolationStroke;
  clonedSpline->m_color               = m_color;
  clonedSpline->m_active              = m_active;
  clonedSpline->m_steps               = m_steps;
  clonedSpline->m_width               = m_width;
  for (int i = 0; i < (int)m_posPathParams.size(); i++)
    clonedSpline->m_posPathParams.push_back(
        (TDoubleParam *)m_posPathParams[i]->clone());
  return clonedSpline;
}

//-----------------------------------------------------------------------------

const TStroke *TStageObjectSpline::getStroke() const { return m_stroke; }

//-----------------------------------------------------------------------------

QList<TPointD> TStageObjectSpline::getInterpolationStroke() {
  return m_interpolationStroke;
}

//-----------------------------------------------------------------------------

void TStageObjectSpline::setInterpolationStroke(QList<TPointD> stroke) {
  m_interpolationStroke = stroke;
}

//-----------------------------------------------------------------------------

void TStageObjectSpline::updatePosPathKeyframes(TStroke *oldSpline,
                                                TStroke *newSpline) {
  if (m_posPathParams.empty()) return;
  PosPathKeyframesUpdater updater(oldSpline, newSpline);
  for (int i = 0; i < (int)m_posPathParams.size(); i++) {
    updater.update(m_posPathParams[i]);
  }
}

//-----------------------------------------------------------------------------

void TStageObjectSpline::setStroke(TStroke *stroke) {
  if (stroke != m_stroke) {
    if (!m_posPathParams.empty() && stroke != 0 && m_stroke != 0)
      updatePosPathKeyframes(m_stroke, stroke);
    delete m_stroke;
    m_stroke = stroke;
  }
}

//-----------------------------------------------------------------------------

void TStageObjectSpline::loadData(TIStream &is) {
  std::vector<TThickPoint> points;
  m_interpolationStroke.clear();
  VersionNumber tnzVersion = is.getVersion();

    std::string tagName;
    while (is.matchTag(tagName)) {
      if (tagName == "splineId")
        is >> m_id;
      else if (tagName == "name")
        is >> m_name;
      else if (tagName == "pos")
        is >> m_dagNodePos.x >> m_dagNodePos.y;
      else if (tagName == "color")
        is >> m_color;
      else if (tagName == "width")
        is >> m_width;
      else if (tagName == "steps")
        is >> m_steps;
      else if (tagName == "isOpened") {
        int v = 0;
        is >> v;
        m_isOpened = (bool)v;
      } else if (tagName == "active") {
        int v = 0;
        is >> v;
        m_active = (bool)v;
      } else if (tagName == "stroke") {
        int i, n = 0;
        is >> n;
        for (i = 0; i < n; i++) {
          TThickPoint p;
          is >> p.x >> p.y >> p.thick;
          points.push_back(p);
        }
      } else if (tagName == "interpolationStroke") {
        int i, n = 0;
        is >> n;
        for (i = 0; i < n; i++) {
          TPointD p;
          is >> p.x >> p.y;
          m_interpolationStroke.push_back(p);
        }
      }

      is.matchEndTag();
    }
    
    // try loading the old type
    if (points.size() == 0) {
        while (!is.eos()) {
            TThickPoint p;
            is >> p.x >> p.y >> p.thick;
            points.push_back(p);
        }
        m_interpolationStroke.clear();
        m_interpolationStroke.push_back(TPointD(-40, 0));
        m_interpolationStroke.push_back(TPointD(-20, 0));
        m_interpolationStroke.push_back(TPointD(-20, 0));
        m_interpolationStroke.push_back(TPointD(0, 0));
        m_interpolationStroke.push_back(TPointD(65, 65));

        m_interpolationStroke.push_back(TPointD(935, 935));
        m_interpolationStroke.push_back(TPointD(1000, 1000));
        m_interpolationStroke.push_back(TPointD(1020, 1000));
        m_interpolationStroke.push_back(TPointD(1020, 1000));
        m_interpolationStroke.push_back(TPointD(1040, 1000));
        m_active = false;
        m_steps = 10;
        m_color = 0;
        m_width = 1;
    }

  delete m_stroke;
  m_stroke = new TStroke(points);
}

//-----------------------------------------------------------------------------

void TStageObjectSpline::saveData(TOStream &os) {
  const TStroke *stroke       = getStroke();
  QList<TPointD> interpStroke = getInterpolationStroke();
  os.child("splineId") << (int)m_id;
  if (!m_name.empty()) os.child("name") << m_name;
  os.child("isOpened") << (int)m_isOpened;
  os.child("pos") << m_dagNodePos.x << m_dagNodePos.y;
  os.child("color") << (int)m_color;
  os.child("active") << (int)m_active;
  os.child("steps") << (int)m_steps;
  os.child("width") << (int)m_width;
  os.openChild("stroke");
  int n = stroke->getControlPointCount();
  os << n;
  for (int i = 0; i < n; i++) {
    TThickPoint p = stroke->getControlPoint(i);
    os << p.x << p.y << p.thick;
  }
  os.closeChild();
  os.openChild("interpolationStroke");
  n = interpStroke.size();
  os << n;
  for (auto p : interpStroke) {
    os << p.x << p.y;
  }
  os.closeChild();
}

//-----------------------------------------------------------------------------

void TStageObjectSpline::setId(int id) { m_id = id; }

//-----------------------------------------------------------------------------

int TStageObjectSpline::getId() const { return m_id; }

//-----------------------------------------------------------------------------

std::string TStageObjectSpline::getName() const {
  if (m_name == "") return "Path" + std::to_string(m_id + 1);
  return m_name;
}

//-----------------------------------------------------------------------------

std::string TStageObjectSpline::getIconId() { return "spline" + m_idBase; }

//-----------------------------------------------------------------------------

void TStageObjectSpline::addParam(TDoubleParam *param) {
  for (int i = 0; i < (int)m_posPathParams.size(); i++)
    if (param == m_posPathParams[i]) return;
  m_posPathParams.push_back(param);
  param->addRef();
}

//-----------------------------------------------------------------------------

void TStageObjectSpline::removeParam(TDoubleParam *param) {
  std::vector<TDoubleParam *>::iterator it =
      std::find(m_posPathParams.begin(), m_posPathParams.end(), param);
  if (it == m_posPathParams.end()) return;
  (*it)->release();
  m_posPathParams.erase(it);
}
