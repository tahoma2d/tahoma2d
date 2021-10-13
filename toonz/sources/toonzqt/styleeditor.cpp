

#include "toonzqt/styleeditor.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/filefield.h"
#include "historytypes.h"
#include "toonzqt/lutcalibrator.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/checkbox.h"

// TnzLib includes
#include "toonz/txshlevel.h"
#include "toonz/stylemanager.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/toonzfolders.h"
#include "toonz/cleanupcolorstyles.h"
#include "toonz/palettecontroller.h"
#include "toonz/imagestyles.h"
#include "toonz/txshsimplelevel.h"  //iwsw
#include "toonz/levelproperties.h"  //iwsw
#include "toonz/mypaintbrushstyle.h"
#include "toonz/preferences.h"
#include "toonz/palettecmd.h"

// TnzCore includes
#include "tconvert.h"
#include "tfiletype.h"
#include "tsystem.h"
#include "tundo.h"
#include "tcolorstyles.h"
#include "tpalette.h"
#include "tpixel.h"
#include "tvectorimage.h"
#include "trasterimage.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tropcm.h"
#include "tvectorrenderdata.h"
#include "tsimplecolorstyles.h"
#include "tvectorbrushstyle.h"

// Qt includes
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <QPainterPath>
#include <QButtonGroup>
#include <QMouseEvent>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QComboBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyleOptionSlider>
#include <QToolTip>
#include <QSplitter>
#include <QMenu>
#include <QOpenGLFramebufferObject>
#include <QWidgetAction>
#include <QScrollBar>

#include <sstream>

using namespace StyleEditorGUI;
using namespace DVGui;

//*****************************************************************************
//    UndoPaletteChange  definition
//*****************************************************************************

QString color2Hex(TPixel32 color) {
  int r = color.r;
  int g = color.g;
  int b = color.b;
  int a = color.m;
  std::stringstream ss;
  QString text = "#";

  if (r != 0 && r < 16) {
    text += "0";
  } else if (r == 0) {
    text += "00";
    if (g == 0) {
      text += "00";
      if (b == 0) {
        text += "00";
        if (a == 0) {
          text += "00";
          return text;
        }
      }
    }
  }
  if (text != "#000000") {
    ss << std::hex << (r << 16 | g << 8 | b);
    text += QString::fromStdString(ss.str());
  }
  if (a < 16) {
    if (a == 0) {
      text += "00";
    } else
      text += "0";
  }
  std::stringstream as;
  as << std::hex << a;
  if (as.str() != "ff") {
    text += QString::fromStdString(as.str());
  }
  return text;
}

bool isHex(QString color) {
  if (color[0] == "#") {
    color.remove(0, 1);
  }
  bool ok;
  const unsigned int parsedValue = color.toUInt(&ok, 16);
  if (ok && (color.length() == 6 || color.length() == 8)) {
    return true;
  } else
    return false;
}

TPixel32 hex2Color(QString hex) {
  if (hex[0] == "#") {
    hex.remove(0, 1);
  }
  bool hasAlpha = hex.length() == 8;
  int length    = hex.length();
  if (hex.length() != 8 && hex.length() != 6) return TPixel32();

  QStringList values;
  while (hex.length() >= 2) {
    values.append(hex.left(2));
    hex.remove(0, 2);
  }
  assert(values.length() == 3 || values.length() == 4);

  TPixel32 color;
  bool dummy;
  int r                 = values.at(0).toInt(&dummy, 16);
  int g                 = values.at(1).toInt(&dummy, 16);
  int b                 = values.at(2).toInt(&dummy, 16);
  color.r               = values.at(0).toInt(&dummy, 16);
  color.g               = values.at(1).toInt(&dummy, 16);
  color.b               = values.at(2).toInt(&dummy, 16);
  if (hasAlpha) color.m = values.at(3).toInt(&dummy, 16);
  return color;
}
void HexLineEdit::focusInEvent(QFocusEvent *event) {
  // QLineEdit::focusInEvent(event);
  selectAll();
}

void HexLineEdit::showEvent(QShowEvent *event) { selectAll(); }

namespace {

class UndoPaletteChange final : public TUndo {
  TPaletteHandle *m_paletteHandle;
  TPaletteP m_palette;

  int m_styleId;
  const TColorStyleP m_oldColor, m_newColor;

  std::wstring m_oldName, m_newName;

  bool m_oldEditedFlag, m_newEditedFlag;

  int m_frame;

public:
  UndoPaletteChange(TPaletteHandle *paletteHandle, int styleId,
                    const TColorStyle &oldColor, const TColorStyle &newColor)
      : m_paletteHandle(paletteHandle)
      , m_palette(paletteHandle->getPalette())
      , m_styleId(styleId)
      , m_oldColor(oldColor.clone())
      , m_newColor(newColor.clone())
      , m_oldName(oldColor.getName())
      , m_newName(newColor.getName())
      , m_frame(m_palette->getFrame())
      , m_oldEditedFlag(oldColor.getIsEditedFlag())
      , m_newEditedFlag(newColor.getIsEditedFlag()) {}

  void undo() const override {
    m_palette->setStyle(m_styleId, m_oldColor->clone());
    m_palette->getStyle(m_styleId)->setIsEditedFlag(m_oldEditedFlag);
    m_palette->getStyle(m_styleId)->setName(m_oldName);

    if (m_palette->isKeyframe(m_styleId, m_frame))
      m_palette->setKeyframe(m_styleId, m_frame);

    // don't change the dirty flag. because m_palette may not the current
    // palette when undo executed
    m_paletteHandle->notifyColorStyleChanged(false, false);
  }

  void redo() const override {
    m_palette->setStyle(m_styleId, m_newColor->clone());
    m_palette->getStyle(m_styleId)->setIsEditedFlag(m_newEditedFlag);
    m_palette->getStyle(m_styleId)->setName(m_newName);

    if (m_palette->isKeyframe(m_styleId, m_frame))
      m_palette->setKeyframe(m_styleId, m_frame);

    // don't change the dirty flag. because m_palette may not the current
    // palette when undo executed
    m_paletteHandle->notifyColorStyleChanged(false, false);
  }

  // imprecise - depends on the style
  int getSize() const override {
    return sizeof(*this) + 2 * sizeof(TColorStyle *);
  }

  QString getHistoryString() override {
    return QObject::tr(
               "Change Style   Palette : %1  Style#%2  [R%3 G%4 B%5] -> [R%6 "
               "G%7 B%8]")
        .arg(QString::fromStdWString(m_palette->getPaletteName()))
        .arg(QString::number(m_styleId))
        .arg(m_oldColor->getMainColor().r)
        .arg(m_oldColor->getMainColor().g)
        .arg(m_oldColor->getMainColor().b)
        .arg(m_newColor->getMainColor().r)
        .arg(m_newColor->getMainColor().g)
        .arg(m_newColor->getMainColor().b);
  }

  int getHistoryType() override { return HistoryType::Palette; }
};

}  // namespace

//*****************************************************************************
//    ColorModel  implementation
//*****************************************************************************

const int ChannelMaxValues[]        = {255, 255, 255, 255, 359, 100, 100};
const int ChannelPairMaxValues[][2] = {{255, 255}, {255, 255}, {255, 255},
                                       {255, 255}, {100, 100}, {359, 100},
                                       {359, 100}};

ColorModel::ColorModel() { memset(m_channels, 0, sizeof m_channels); }

//-----------------------------------------------------------------------------

void ColorModel::rgb2hsv() {
  QColor converter(m_channels[0], m_channels[1], m_channels[2]);
  m_channels[4] =
      std::max(converter.hue(), 0);  // hue() ritorna -1 per colori acromatici
  m_channels[5] = converter.saturation() * 100 / 255;
  m_channels[6] = converter.value() * 100 / 255;
}

//-----------------------------------------------------------------------------

void ColorModel::hsv2rgb() {
  QColor converter = QColor::fromHsv(m_channels[4], m_channels[5] * 255 / 100,
                                     m_channels[6] * 255 / 100);

  m_channels[0] = converter.red();
  m_channels[1] = converter.green();
  m_channels[2] = converter.blue();
}

//-----------------------------------------------------------------------------

void ColorModel::setTPixel(const TPixel32 &pix) {
  QColor color(pix.r, pix.g, pix.b, pix.m);
  m_channels[0] = color.red();
  m_channels[1] = color.green();
  m_channels[2] = color.blue();
  m_channels[3] = color.alpha();
  m_channels[4] =
      std::max(color.hue(), 0);  // hue() ritorna -1 per colori acromatici
  m_channels[5] = color.saturation() * 100 / 255;
  m_channels[6] = color.value() * 100 / 255;
}

//-----------------------------------------------------------------------------

TPixel32 ColorModel::getTPixel() const {
  return TPixel32(m_channels[0], m_channels[1], m_channels[2], m_channels[3]);
}

//-----------------------------------------------------------------------------

void ColorModel::setValue(ColorChannel channel, int value) {
  assert(0 <= (int)channel && (int)channel < 7);
  assert(0 <= value && value <= ChannelMaxValues[channel]);
  m_channels[(int)channel] = value;
  if (channel >= eHue)
    hsv2rgb();
  else if (channel <= eBlue)
    rgb2hsv();
}

//-----------------------------------------------------------------------------

void ColorModel::setValues(ColorChannel channel, int v, int u) {
  assert(0 <= (int)channel && (int)channel < 7);
  switch (channel) {
  case eRed:
    setValue(eGreen, v);
    setValue(eBlue, u);
    break;
  case eGreen:
    setValue(eRed, v);
    setValue(eBlue, u);
    break;
  case eBlue:
    setValue(eRed, v);
    setValue(eGreen, u);
    break;
  case eHue:
    setValue(eSaturation, v);
    setValue(eValue, u);
    break;
  case eSaturation:
    setValue(eHue, v);
    setValue(eValue, u);
    break;
  case eValue:
    setValue(eHue, v);
    setValue(eSaturation, u);
    break;
  default:
    break;
  }
}

//-----------------------------------------------------------------------------

int ColorModel::getValue(ColorChannel channel) const {
  assert(0 <= (int)channel && (int)channel < 7);
  return m_channels[(int)channel];
}

//-----------------------------------------------------------------------------

void ColorModel::getValues(ColorChannel channel, int &u, int &v) {
  switch (channel) {
  case eRed:
    u = getValue(eGreen);
    v = getValue(eBlue);
    break;
  case eGreen:
    u = getValue(eRed);
    v = getValue(eBlue);
    break;
  case eBlue:
    u = getValue(eRed);
    v = getValue(eGreen);
    break;
  case eHue:
    u = getValue(eSaturation);
    v = getValue(eValue);
    break;
  case eSaturation:
    u = getValue(eHue);
    v = getValue(eValue);
    break;
  case eValue:
    u = getValue(eHue);
    v = getValue(eSaturation);
    break;
  default:
    break;
  }
}

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

class RedShadeMaker {
  const ColorModel &m_color;

public:
  RedShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor(value, m_color.g(), m_color.b()).rgba();
  }
};

//-----------------------------------------------------------------------------

class GreenShadeMaker {
  const ColorModel &m_color;

public:
  GreenShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor(m_color.r(), value, m_color.b()).rgba();
  }
};

//-----------------------------------------------------------------------------

class BlueShadeMaker {
  const ColorModel &m_color;

public:
  BlueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor(m_color.r(), m_color.g(), value).rgba();
  }
};

//-----------------------------------------------------------------------------

class AlphaShadeMaker {
  const ColorModel &m_color;

public:
  AlphaShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor(m_color.r(), m_color.g(), m_color.b(), value).rgba();
  }
};

//-----------------------------------------------------------------------------

class HueShadeMaker {
  const ColorModel &m_color;

public:
  HueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor::fromHsv(359 * value / 255, m_color.s() * 255 / 100,
                           m_color.v() * 255 / 100)
        .rgba();
  }
};

//-----------------------------------------------------------------------------

class SaturationShadeMaker {
  const ColorModel &m_color;

public:
  SaturationShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor::fromHsv(m_color.h(), value, m_color.v() * 255 / 100).rgba();
  }
};

//-----------------------------------------------------------------------------

class ValueShadeMaker {
  const ColorModel &m_color;

public:
  ValueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int value) const {
    return QColor::fromHsv(m_color.h(), m_color.s() * 255 / 100, value).rgba();
  }
};

//-----------------------------------------------------------------------------

class RedGreenShadeMaker {
  const ColorModel &m_color;

public:
  RedGreenShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor(u, v, m_color.b()).rgba();
  }
};

//-----------------------------------------------------------------------------

class RedBlueShadeMaker {
  const ColorModel &m_color;

public:
  RedBlueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor(u, m_color.g(), v).rgba();
  }
};

//-----------------------------------------------------------------------------

class GreenBlueShadeMaker {
  const ColorModel &m_color;

public:
  GreenBlueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor(m_color.r(), u, v).rgba();
  }
};

//-----------------------------------------------------------------------------

class SaturationValueShadeMaker {
  const ColorModel &m_color;

public:
  SaturationValueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor::fromHsv(m_color.h(), u, v).rgba();
  }
};

//-----------------------------------------------------------------------------

class HueValueShadeMaker {
  const ColorModel &m_color;

public:
  HueValueShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor::fromHsv(359 * u / 255, m_color.s() * 255 / 100, v).rgba();
  }
};

//-----------------------------------------------------------------------------

class HueSaturationShadeMaker {
  const ColorModel &m_color;

public:
  HueSaturationShadeMaker(const ColorModel &color) : m_color(color) {}
  inline QRgb shade(int u, int v) const {
    return QColor::fromHsv(359 * u / 255, v, m_color.v() * 255 / 100).rgba();
  }
};

//-----------------------------------------------------------------------------

template <class ShadeMaker>
QPixmap makeLinearShading(const ShadeMaker &shadeMaker, int size,
                          bool isVertical) {
  assert(size > 0);
  QPixmap bgPixmap;
  int i, dx, dy, w = 1, h = 1;
  int x = 0, y = 0;
  if (isVertical) {
    dx = 0;
    dy = -1;
    h  = size;
    y  = size - 1;
  } else {
    dx = 1;
    dy = 0;
    w  = size;
  }
  QImage image(w, h, QImage::Format_ARGB32);
  for (i = 0; i < size; i++) {
    int v = 255 * i / (size - 1);
    image.setPixel(x, y, shadeMaker.shade(v));
    x += dx;
    y += dy;
  }
  return QPixmap::fromImage(image);
}

//-----------------------------------------------------------------------------

QPixmap makeLinearShading(const ColorModel &color, ColorChannel channel,
                          int size, bool isVertical) {
  switch (channel) {
  case eRed:
    if (isVertical)
      return makeLinearShading(RedShadeMaker(color), size, isVertical);
    else
      return QPixmap(":Resources/grad_r.png").scaled(size, 1);
  case eGreen:
    if (isVertical)
      return makeLinearShading(GreenShadeMaker(color), size, isVertical);
    else
      return QPixmap(":Resources/grad_g.png").scaled(size, 1);
  case eBlue:
    if (isVertical)
      return makeLinearShading(BlueShadeMaker(color), size, isVertical);
    else
      return QPixmap(":Resources/grad_b.png").scaled(size, 1);
  case eAlpha:
    if (isVertical)
      return makeLinearShading(AlphaShadeMaker(color), size, isVertical);
    else
      return QPixmap(":Resources/grad_m.png").scaled(size, 1);
  case eHue:
    return makeLinearShading(HueShadeMaker(color), size, isVertical);
  case eSaturation:
    return makeLinearShading(SaturationShadeMaker(color), size, isVertical);
  case eValue:
    return makeLinearShading(ValueShadeMaker(color), size, isVertical);
  default:
    assert(0);
  }
  return QPixmap();
}

//-----------------------------------------------------------------------------

template <class ShadeMaker>
QPixmap makeSquareShading(const ShadeMaker &shadeMaker, int size) {
  assert(size > 0);
  QPixmap bgPixmap;
  QImage image(size, size, QImage::Format_RGB32);
  int i, j;
  for (j = 0; j < size; j++) {
    int u = 255 - (255 * j / (size - 1));
    for (i = 0; i < size; i++) {
      int v = 255 * i / (size - 1);
      image.setPixel(i, j, shadeMaker.shade(v, u));
    }
  }
  return QPixmap::fromImage(image);
}

//-----------------------------------------------------------------------------

QPixmap makeSquareShading(const ColorModel &color, ColorChannel channel,
                          int size) {
  switch (channel) {
  case eRed:
    return makeSquareShading(GreenBlueShadeMaker(color), size);
  case eGreen:
    return makeSquareShading(RedBlueShadeMaker(color), size);
  case eBlue:
    return makeSquareShading(RedGreenShadeMaker(color), size);
  case eHue:
    return makeSquareShading(SaturationValueShadeMaker(color), size);
  case eSaturation:
    return makeSquareShading(HueValueShadeMaker(color), size);
  case eValue:
    return makeSquareShading(HueSaturationShadeMaker(color), size);
  default:
    assert(0);
  }
  return QPixmap();
}

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//*****************************************************************************
//    HexagonalColorWheel  implementation
//*****************************************************************************

HexagonalColorWheel::HexagonalColorWheel(QWidget *parent)
    : GLWidgetForHighDpi(parent)
    , m_bgColor(128, 128, 128)  // default value in case this value does not set
                                // in the style sheet
{
  setObjectName("HexagonalColorWheel");
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  setFocusPolicy(Qt::NoFocus);
  m_currentWheel = none;
  if (Preferences::instance()->isColorCalibrationEnabled())
    m_lutCalibrator = new LutCalibrator();
}

//-----------------------------------------------------------------------------

HexagonalColorWheel::~HexagonalColorWheel() {
  if (m_fbo) delete m_fbo;
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::updateColorCalibration() {
  if (Preferences::instance()->isColorCalibrationEnabled()) {
    makeCurrent();
    if (!m_lutCalibrator)
      m_lutCalibrator = new LutCalibrator();
    else
      m_lutCalibrator->cleanup();
    m_lutCalibrator->initialize();
    connect(context(), SIGNAL(aboutToBeDestroyed()), this,
            SLOT(onContextAboutToBeDestroyed()));
    if (m_lutCalibrator->isValid() && !m_fbo)
      m_fbo = new QOpenGLFramebufferObject(width(), height());
    doneCurrent();
  }
  update();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::showEvent(QShowEvent *) {
  if (m_cuedCalibrationUpdate) {
    updateColorCalibration();
    m_cuedCalibrationUpdate = false;
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::initializeGL() {
  initializeOpenGLFunctions();

  // to be computed once through the software
  if (m_lutCalibrator && !m_lutCalibrator->isInitialized()) {
    m_lutCalibrator->initialize();
    connect(context(), SIGNAL(aboutToBeDestroyed()), this,
            SLOT(onContextAboutToBeDestroyed()));
  }

  QColor const color = getBGColor();
  glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());

  // Without the following lines, the wheel in a floating style editor
  // disappears on switching the room due to context switching.
  if (m_firstInitialized)
    m_firstInitialized = false;
  else {
    resizeGL(width(), height());
    update();
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::resizeGL(int w, int h) {
  w *= getDevPixRatio();
  h *= getDevPixRatio();
  float d                 = (w - 5.0f) / 2.5f;
  bool isHorizontallyLong = ((d * 1.732f) < h) ? false : true;

  if (isHorizontallyLong) {
    m_triEdgeLen = (float)h / 1.732f;
    m_triHeight  = (float)h / 2.0f;
    m_wheelPosition.setX(((float)w - (m_triEdgeLen * 2.5f + 5.0f)) / 2.0f);
    m_wheelPosition.setY(0.0f);
  } else {
    m_triEdgeLen = d;
    m_triHeight  = m_triEdgeLen * 0.866f;
    m_wheelPosition.setX(0.0f);
    m_wheelPosition.setY(((float)h - (m_triHeight * 2.0f)) / 2.0f);
  }

  // set all vertices positions
  m_wp[0].setX(m_triEdgeLen);
  m_wp[0].setY(m_triHeight);
  m_wp[1].setX(m_triEdgeLen * 0.5f);
  m_wp[1].setY(0.0f);
  m_wp[2].setX(0.0f);
  m_wp[2].setY(m_triHeight);
  m_wp[3].setX(m_triEdgeLen * 0.5f);
  m_wp[3].setY(m_triHeight * 2.0f);
  m_wp[4].setX(m_triEdgeLen * 1.5f);
  m_wp[4].setY(m_triHeight * 2.0f);
  m_wp[5].setX(m_triEdgeLen * 2.0f);
  m_wp[5].setY(m_triHeight);
  m_wp[6].setX(m_triEdgeLen * 1.5f);
  m_wp[6].setY(0.0f);

  m_leftp[0].setX(m_wp[6].x() + 5.0f);
  m_leftp[0].setY(0.0f);
  m_leftp[1].setX(m_leftp[0].x() + m_triEdgeLen);
  m_leftp[1].setY(m_triHeight * 2.0f);
  m_leftp[2].setX(m_leftp[1].x());
  m_leftp[2].setY(0.0f);

  // GL settings
  glViewport(0, 0, w, h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, (GLdouble)w, (GLdouble)h, 0.0, 1.0, -1.0);

  // remake fbo with new size
  if (m_lutCalibrator && m_lutCalibrator->isValid()) {
    if (m_fbo) delete m_fbo;
    m_fbo = new QOpenGLFramebufferObject(w, h);
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::paintGL() {
  // call ClearColor() here in order to update bg color when the stylesheet is
  // switched
  QColor const color = getBGColor();
  glClearColor(color.redF(), color.greenF(), color.blueF(), color.alphaF());

  glMatrixMode(GL_MODELVIEW);

  if (m_lutCalibrator && m_lutCalibrator->isValid()) m_fbo->bind();

  glClear(GL_COLOR_BUFFER_BIT);

  float v = (float)m_color.getValue(eValue) / 100.0f;

  glPushMatrix();

  // draw hexagonal color wheel
  glTranslatef(m_wheelPosition.rx(), m_wheelPosition.ry(), 0.0f);
  glBegin(GL_TRIANGLE_FAN);
  glColor3f(v, v, v);
  glVertex2f(m_wp[0].x(), m_wp[0].y());

  glColor3f(0.0f, v, 0.0f);
  glVertex2f(m_wp[1].x(), m_wp[1].y());
  glColor3f(0.0f, v, v);
  glVertex2f(m_wp[2].x(), m_wp[2].y());
  glColor3f(0.0f, 0.0f, v);
  glVertex2f(m_wp[3].x(), m_wp[3].y());
  glColor3f(v, 0.0f, v);
  glVertex2f(m_wp[4].x(), m_wp[4].y());
  glColor3f(v, 0.0f, 0.0f);
  glVertex2f(m_wp[5].x(), m_wp[5].y());
  glColor3f(v, v, 0.0f);
  glVertex2f(m_wp[6].x(), m_wp[6].y());
  glColor3f(0.0f, v, 0.0f);
  glVertex2f(m_wp[1].x(), m_wp[1].y());
  glEnd();

  QColor leftCol = QColor().fromHsv(m_color.getValue(eHue), 255, 255);

  // draw triangle color picker
  glBegin(GL_TRIANGLES);
  glColor3f(leftCol.redF(), leftCol.greenF(), leftCol.blueF());
  glVertex2f(m_leftp[0].x(), m_leftp[0].y());
  glColor3f(0.0f, 0.0f, 0.0f);
  glVertex2f(m_leftp[1].x(), m_leftp[1].y());
  glColor3f(1.0f, 1.0f, 1.0f);
  glVertex2f(m_leftp[2].x(), m_leftp[2].y());
  glEnd();

  // draw small quad at current color position
  drawCurrentColorMark();

  glPopMatrix();

  if (m_lutCalibrator && m_lutCalibrator->isValid())
    m_lutCalibrator->onEndDraw(m_fbo);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::drawCurrentColorMark() {
  int h;
  float s, v;

  // show hue in a counterclockwise fashion
  h = 360 - m_color.getValue(eHue);

  s = (float)m_color.getValue(eSaturation) / 100.0f;
  v = (float)m_color.getValue(eValue) / 100.0f;

  // d is a distance from a center of the wheel
  float d, phi;
  phi = (float)(h % 60 - 30) / 180.0f * 3.1415f;
  d   = s * m_triHeight / cosf(phi);

  // set marker color
  if (v > 0.4f)
    glColor3f(0.0f, 0.0f, 0.0f);
  else
    glColor3f(1.0f, 1.0f, 1.0f);

  // draw marker (in the wheel)
  glPushMatrix();
  glTranslatef(m_wp[0].x(), m_wp[0].y(), 0.1f);
  glRotatef(h, 0.0, 0.0, 1.0);
  glTranslatef(d, 0.0f, 0.0f);
  glRotatef(-h, 0.0, 0.0, 1.0);
  glBegin(GL_LINE_LOOP);
  glVertex2f(-3, -3);
  glVertex2f(3, -3);
  glVertex2f(3, 3);
  glVertex2f(-3, 3);
  glEnd();
  glPopMatrix();

  // draw marker (in the triangle)
  glPushMatrix();
  glTranslatef(m_leftp[1].x(), m_leftp[1].y(), 0.1f);
  glTranslatef(-m_triEdgeLen * v * s, -m_triHeight * v * 2.0f, 0.0f);
  glBegin(GL_LINE_LOOP);
  glVertex2f(-3, -3);
  glVertex2f(3, -3);
  glVertex2f(3, 3);
  glVertex2f(-3, 3);
  glEnd();
  glPopMatrix();
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::mousePressEvent(QMouseEvent *event) {
  if (~event->buttons() & Qt::LeftButton) return;

  // check whether the mouse cursor is in the wheel or in the triangle (or
  // nothing).
  QPoint curPos = event->pos() * getDevPixRatio();

  QPolygonF wheelPolygon;
  // in the case of the wheel
  wheelPolygon << m_wp[1] << m_wp[2] << m_wp[3] << m_wp[4] << m_wp[5]
               << m_wp[6];
  wheelPolygon.translate(m_wheelPosition);
  if (wheelPolygon.toPolygon().containsPoint(curPos, Qt::OddEvenFill)) {
    m_currentWheel = leftWheel;
    clickLeftWheel(curPos);
    return;
  }

  wheelPolygon.clear();
  // in the case of the triangle
  wheelPolygon << m_leftp[0] << m_leftp[1] << m_leftp[2];
  wheelPolygon.translate(m_wheelPosition);
  if (wheelPolygon.toPolygon().containsPoint(curPos, Qt::OddEvenFill)) {
    m_currentWheel = rightTriangle;
    clickRightTriangle(curPos);
    return;
  }

  //... or, in the case of nothing
  m_currentWheel = none;
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::mouseMoveEvent(QMouseEvent *event) {
  // change the behavior according to the current touching wheel
  switch (m_currentWheel) {
  case none:
    break;
  case leftWheel:
    clickLeftWheel(event->pos() * getDevPixRatio());
    break;
  case rightTriangle:
    clickRightTriangle(event->pos() * getDevPixRatio());
    break;
  }
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::mouseReleaseEvent(QMouseEvent *event) {
  m_currentWheel = none;
  emit colorChanged(m_color, false);
}

//-----------------------------------------------------------------------------
/*! compute hue and saturation position. saturation value must be clamped
 */
void HexagonalColorWheel::clickLeftWheel(const QPoint &pos) {
  QLineF p(m_wp[0] + m_wheelPosition, QPointF(pos));
  QLineF horizontal(0, 0, 1, 0);
  float theta = (p.dy() < 0) ? p.angle(horizontal) : 360 - p.angle(horizontal);
  float phi   = theta;
  while (phi >= 60.0f) phi -= 60.0f;
  phi -= 30.0f;
  // d is a length from center to edge of the wheel when saturation = 100
  float d = m_triHeight / cosf(phi / 180.0f * 3.1415f);

  int h          = (int)theta;
  if (h > 359) h = 359;
  // clamping
  int s = (int)(std::min(p.length() / d, 1.0) * 100.0f);

  m_color.setValues(eValue, h, s);

  emit colorChanged(m_color, true);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::clickRightTriangle(const QPoint &pos) {
  int s, v;
  QPointF p = m_leftp[1] + m_wheelPosition - QPointF(pos);
  if (p.ry() <= 0.0f) {
    s = 0;
    v = 0;
  } else {
    float v_ratio = std::min((float)(p.ry() / (m_triHeight * 2.0f)), 1.0f);
    float s_f     = p.rx() / (m_triEdgeLen * v_ratio);
    v             = (int)(v_ratio * 100.0f);
    s             = (int)(std::min(std::max(s_f, 0.0f), 1.0f) * 100.0f);
  }
  m_color.setValues(eHue, s, v);
  emit colorChanged(m_color, true);
}

//-----------------------------------------------------------------------------

void HexagonalColorWheel::onContextAboutToBeDestroyed() {
  if (!m_lutCalibrator) return;
  makeCurrent();
  m_lutCalibrator->cleanup();
  doneCurrent();
  disconnect(context(), SIGNAL(aboutToBeDestroyed()), this,
             SLOT(onContextAboutToBeDestroyed()));
}

//*****************************************************************************
//    SquaredColorWheel  implementation
//*****************************************************************************

SquaredColorWheel::SquaredColorWheel(QWidget *parent)
    : QWidget(parent), m_channel(eRed), m_color() {}

//-----------------------------------------------------------------------------

void SquaredColorWheel::paintEvent(QPaintEvent *) {
  QPainter p(this);
  // calcolo lo sfondo
  int size = width();

  QPixmap bgPixmap = makeSquareShading(m_color, m_channel, size);

  if (!bgPixmap.isNull()) p.drawTiledPixmap(0, 0, size, size, bgPixmap);

  int u = 0, v = 0;
  m_color.getValues(m_channel, u, v);
  int x = u * width() / ChannelPairMaxValues[m_channel][0];
  int y = (ChannelPairMaxValues[m_channel][1] - v) * height() /
          ChannelPairMaxValues[m_channel][1];

  if (m_color.v() > 127)
    p.setPen(Qt::black);
  else
    p.setPen(Qt::white);
  p.drawRect(x - 1, y - 1, 3, 3);
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::click(const QPoint &pos) {
  int u = ChannelPairMaxValues[m_channel][0] * pos.x() / width();
  int v = ChannelPairMaxValues[m_channel][1] * (height() - pos.y()) / height();
  u     = tcrop(u, 0, ChannelPairMaxValues[m_channel][0]);
  v     = tcrop(v, 0, ChannelPairMaxValues[m_channel][1]);
  m_color.setValues(m_channel, u, v);
  update();
  emit colorChanged(m_color, true);
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::mousePressEvent(QMouseEvent *event) {
  if (event->buttons() & Qt::LeftButton) click(event->pos());
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::mouseMoveEvent(QMouseEvent *event) {
  if (event->buttons() & Qt::LeftButton) click(event->pos());
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::mouseReleaseEvent(QMouseEvent *event) {
  emit colorChanged(m_color, false);
}

//-----------------------------------------------------------------------------

void SquaredColorWheel::setColor(const ColorModel &color) { m_color = color; }

//-----------------------------------------------------------------------------

void SquaredColorWheel::setChannel(int channel) {
  assert(0 <= channel && channel < 7);
  m_channel = (ColorChannel)channel;
  update();
}

//*****************************************************************************
//    ColorSlider  implementation
//*****************************************************************************

ColorSlider::ColorSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent), m_channel(eRed), m_color() {
  setFocusPolicy(Qt::NoFocus);

  setOrientation(orientation);
  setMinimum(0);
  setMaximum(ChannelMaxValues[m_channel]);

  setMinimumHeight(7);
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  // Attenzione: necessario per poter individuare l'oggetto nel file di
  // definizione dello stile
  setObjectName("colorSlider");
}

//-----------------------------------------------------------------------------

void ColorSlider::setChannel(ColorChannel channel) {
  if (m_channel == channel) return;
  m_channel = channel;
  setMaximum(ChannelMaxValues[m_channel]);
}

//-----------------------------------------------------------------------------

void ColorSlider::setColor(const ColorModel &color) { m_color = color; }

//-----------------------------------------------------------------------------

void ColorSlider::paintEvent(QPaintEvent *event) {
  QPainter p(this);

  int x          = rect().x();
  int y          = rect().y();
  int w          = width();
  int h          = height();
  int handleSize = svgToPixmap(":Resources/h_chandle_center.svg").width();

  bool isVertical = orientation() == Qt::Vertical;

  if (!isVertical) h -= 5;

  QPixmap bgPixmap =
      makeLinearShading(m_color, m_channel, isVertical ? h : w, isVertical);

  if (m_channel == eAlpha) {
    static QPixmap checkboard(":Resources/backg.png");
    p.drawTiledPixmap(x + (handleSize / 2), y + 1, w - handleSize, h,
                      checkboard);
  }

  if (!bgPixmap.isNull()) {
    p.drawTiledPixmap(x + (handleSize / 2), y + 1, w - handleSize, h, bgPixmap);
    p.setPen(Qt::black);
    p.drawRect(x + (handleSize / 2), y + 1, x + w - handleSize, y + h);
  }

  /*!
     Bug in Qt 4.3: The vertical Slider cannot be styled due to a bug.
     In this case we draw "manually" the slider handle at correct position
  */
  if (isVertical) {
    int pos = QStyle::sliderPositionFromValue(minimum(), maximum(), value(),
                                              h - 9, true);
    static QPixmap vHandlePixmap(":Resources/v_chandle.png");
    p.drawPixmap(0, pos, vHandlePixmap);
  } else {
    static QPixmap hHandleUpPm(":Resources/h_chandle_up.svg");
    static QPixmap hHandleDownPm(":Resources/h_chandle_down.svg");
    static QPixmap hHandleCenterPm(":Resources/h_chandle_center.svg");
    int pos = QStyle::sliderPositionFromValue(
        0, maximum(), value(), width() - hHandleCenterPm.width(), false);
    p.drawPixmap(pos, 0, hHandleUpPm);
    p.drawPixmap(pos, height() - hHandleDownPm.height(), hHandleDownPm);
    p.drawPixmap(pos, hHandleUpPm.height(), hHandleCenterPm.width(),
                 height() - hHandleUpPm.height() - hHandleDownPm.height(),
                 hHandleCenterPm);
  }
};

//-----------------------------------------------------------------------------

void ColorSlider::mousePressEvent(QMouseEvent *event) {
  // vogliamo che facendo click sullo slider, lontano dall'handle
  // l'handle salti subito nella posizione giusta invece di far partire
  // l'autorepeat.
  //
  // cfr. qslider.cpp:429: sembra che questo comportamento si possa ottenere
  // anche con SH_Slider_AbsoluteSetButtons. Ma non capisco come si possa fare
  // per definire quest hint
  QStyleOptionSlider opt;
  initStyleOption(&opt);
  const QRect handleRect = style()->subControlRect(
      QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
  if (!handleRect.contains(event->pos())) {
    const QPoint handleCenter = handleRect.center();
    const QRect grooveRect    = style()->subControlRect(
        QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
    int pos, span;
    bool upsideDown = false;
    if (opt.orientation == Qt::Vertical) {
      upsideDown     = true;
      int handleSize = handleRect.height();
      pos            = event->pos().y();
      span           = grooveRect.height();
    } else {
      int handleSize = QPixmap(":Resources/h_chandle_center.svg").width();
      pos            = event->pos().x();
      span           = grooveRect.width();
    }
    int value = QStyle::sliderValueFromPosition(minimum(), maximum(), pos, span,
                                                upsideDown);
    setValue(value);
  }
  QSlider::mousePressEvent(event);
}

//-----------------------------------------------------------------------------

void ColorSlider::mouseReleaseEvent(QMouseEvent *event) {
  emit sliderReleased();
}

//*****************************************************************************
//    ArrowButton  implementation
//*****************************************************************************

ArrowButton::ArrowButton(QWidget *parent, Qt::Orientation orientation,
                         bool isFirstArrow)
    : QToolButton(parent)
    , m_orientaion(orientation)
    , m_isFirstArrow(isFirstArrow)
    , m_timerId(0)
    , m_firstTimerId(0) {
  setFixedSize(10, 10);
  setObjectName("StyleEditorArrowButton");
  bool isVertical = orientation == Qt::Vertical;
  if (m_isFirstArrow) {
    if (isVertical)
      setIcon(createQIconPNG("arrow_up"));
    else
      setIcon(createQIconPNG("arrow_left"));
  } else {
    if (isVertical)
      setIcon(createQIconPNG("arrow_down"));
    else
      setIcon(createQIconPNG("arrow_right"));
  }
  connect(this, SIGNAL(pressed()), this, SLOT(onPressed()));
  connect(this, SIGNAL(released()), this, SLOT(onRelease()));
}

//-----------------------------------------------------------------------------

void ArrowButton::timerEvent(QTimerEvent *event) {
  if (m_firstTimerId != 0) {
    killTimer(m_firstTimerId);
    m_firstTimerId = 0;
    m_timerId      = startTimer(10);
  }
  notifyChanged();
}

//-----------------------------------------------------------------------------

void ArrowButton::notifyChanged() {
  bool isVertical = m_orientaion == Qt::Vertical;
  if ((m_isFirstArrow && !isVertical) || (!m_isFirstArrow && isVertical))
    emit remove();
  else
    emit add();
}

//-----------------------------------------------------------------------------

void ArrowButton::onPressed() {
  notifyChanged();
  assert(m_timerId == 0 && m_firstTimerId == 0);
  m_firstTimerId = startTimer(500);
}

//-----------------------------------------------------------------------------

void ArrowButton::onRelease() {
  if (m_firstTimerId != 0) {
    killTimer(m_firstTimerId);
    m_firstTimerId = 0;
  } else if (m_timerId != 0) {
    killTimer(m_timerId);
    m_timerId = 0;
  }
}

//*****************************************************************************
//    ColorSliderBar  implementation
//*****************************************************************************

ColorSliderBar::ColorSliderBar(QWidget *parent, Qt::Orientation orientation)
    : QWidget(parent) {
  bool isVertical = orientation == Qt::Vertical;

  ArrowButton *first = new ArrowButton(this, orientation, true);
  connect(first, SIGNAL(remove()), this, SLOT(onRemove()));
  connect(first, SIGNAL(add()), this, SLOT(onAdd()));

  m_colorSlider = new ColorSlider(orientation, this);
  if (isVertical) m_colorSlider->setMaximumWidth(22);

  ArrowButton *last = new ArrowButton(this, orientation, false);
  connect(last, SIGNAL(add()), this, SLOT(onAdd()));
  connect(last, SIGNAL(remove()), this, SLOT(onRemove()));

  connect(m_colorSlider, SIGNAL(valueChanged(int)), this,
          SIGNAL(valueChanged(int)));
  connect(m_colorSlider, SIGNAL(sliderReleased()), this,
          SIGNAL(valueChanged()));

  QBoxLayout *layout;
  if (!isVertical)
    layout = new QHBoxLayout(this);
  else
    layout = new QVBoxLayout(this);

  layout->setSpacing(0);
  layout->setMargin(0);
  layout->addWidget(first, 0, Qt::AlignCenter);
  layout->addWidget(m_colorSlider, 1);
  layout->addWidget(last, 0, Qt::AlignCenter);
  setLayout(layout);
}

//-----------------------------------------------------------------------------

void ColorSliderBar::onRemove() {
  int value = m_colorSlider->value();
  if (value <= m_colorSlider->minimum()) return;
  m_colorSlider->setValue(value - 1);
}

//-----------------------------------------------------------------------------

void ColorSliderBar::onAdd() {
  int value = m_colorSlider->value();
  if (value >= m_colorSlider->maximum()) return;
  m_colorSlider->setValue(value + 1);
}

//*****************************************************************************
//    ChannelLineEdit  implementation
//*****************************************************************************

void ChannelLineEdit::mousePressEvent(QMouseEvent *e) {
  IntLineEdit::mousePressEvent(e);

  if (!m_isEditing) {
    selectAll();
    m_isEditing = true;
  }
}

//-----------------------------------------------------------------------------

void ChannelLineEdit::focusOutEvent(QFocusEvent *e) {
  IntLineEdit::focusOutEvent(e);

  m_isEditing = false;
}

//-----------------------------------------------------------------------------

void ChannelLineEdit::paintEvent(QPaintEvent *e) {
  IntLineEdit::paintEvent(e);

  /* Now that stylesheets added lineEdit focus this is likely redundant,
   * commenting out in-case it is still required.
  if (m_isEditing) {
    QPainter p(this);
    p.setPen(Qt::yellow);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
  }
  */
}

//*****************************************************************************
//    ColorChannelControl  implementation
//*****************************************************************************

ColorChannelControl::ColorChannelControl(ColorChannel channel, QWidget *parent)
    : QWidget(parent), m_channel(channel), m_value(0), m_signalEnabled(true) {
  setFocusPolicy(Qt::NoFocus);

  QStringList channelList;
  channelList << tr("R") << tr("G") << tr("B") << tr("A") << tr("H") << tr("S")
              << tr("V");
  assert(0 <= (int)m_channel && (int)m_channel < 7);
  QString text = channelList.at(m_channel);
  m_label      = new QLabel(text, this);

  int minValue = 0;
  int maxValue = 0;
  if (m_channel < 4)  // RGBA
    maxValue = 255;
  else if (m_channel == 4)  // H
    maxValue = 359;
  else  // SV
    maxValue = 100;

  m_field = new ChannelLineEdit(this, 0, minValue, maxValue);
  if (text == "A") {
    m_label->setToolTip(
        tr("Alpha controls the transparency. \nZero is fully transparent."));
    m_field->setToolTip(
        tr("Alpha controls the transparency. \nZero is fully transparent."));
  }
  m_slider = new ColorSlider(Qt::Horizontal, this);

  // buttons to increment/decrement the values by 1
  QPushButton *addButton = new QPushButton(this);
  QPushButton *subButton = new QPushButton(this);

  m_slider->setValue(0);
  m_slider->setChannel(m_channel);

  m_label->setObjectName("colorSliderLabel");
  m_label->setFixedWidth(11);
  m_label->setMinimumHeight(7);
  m_label->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

  m_field->setObjectName("colorSliderField");
  m_field->setFixedWidth(fontMetrics().width('0') * 4);
  m_field->setMinimumHeight(7);

  addButton->setObjectName("colorSliderAddButton");
  subButton->setObjectName("colorSliderSubButton");
  addButton->setFixedWidth(18);
  subButton->setFixedWidth(18);
  addButton->setMinimumHeight(7);
  subButton->setMinimumHeight(7);
  addButton->setFlat(true);
  subButton->setFlat(true);
  addButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  subButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  addButton->setAutoRepeat(true);
  subButton->setAutoRepeat(true);
  addButton->setAutoRepeatInterval(25);
  subButton->setAutoRepeatInterval(25);
  addButton->setFocusPolicy(Qt::NoFocus);
  subButton->setFocusPolicy(Qt::NoFocus);

  QHBoxLayout *mainLayout = new QHBoxLayout(this);
  mainLayout->setMargin(0);
  mainLayout->setSpacing(1);
  {
    mainLayout->addWidget(m_label, 0);
    mainLayout->addWidget(subButton, 0);
    mainLayout->addSpacing(1);
    mainLayout->addWidget(m_field, 0);
    mainLayout->addSpacing(1);
    mainLayout->addWidget(addButton, 0);
    mainLayout->addWidget(m_slider, 1);
  }
  setLayout(mainLayout);

  bool ret =
      connect(m_field, SIGNAL(editingFinished()), this, SLOT(onFieldChanged()));
  ret = ret && connect(m_slider, SIGNAL(valueChanged(int)), this,
                       SLOT(onSliderChanged(int)));
  ret = ret && connect(m_slider, SIGNAL(sliderReleased()), this,
                       SLOT(onSliderReleased()));
  ret = ret &&
        connect(addButton, SIGNAL(clicked()), this, SLOT(onAddButtonClicked()));
  ret = ret &&
        connect(subButton, SIGNAL(clicked()), this, SLOT(onSubButtonClicked()));
  assert(ret);
  setMaximumHeight(30);
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onAddButtonClicked() {
  m_slider->setValue(m_slider->value() + 1);
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onSubButtonClicked() {
  m_slider->setValue(m_slider->value() - 1);
}

//-----------------------------------------------------------------------------

void ColorChannelControl::setColor(const ColorModel &color) {
  m_color = color;
  m_slider->setColor(color);
  int value = color.getValue(m_channel);
  if (m_value != value) {
    bool signalEnabled = m_signalEnabled;
    m_signalEnabled    = false;
    m_value            = value;
    m_field->setText(QString::number(value));
    m_slider->setValue(value);
    m_signalEnabled = signalEnabled;
  }
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onFieldChanged() {
  int value = m_field->text().toInt();
  if (m_value == value) return;
  m_value = value;
  m_slider->setValue(value);
  if (m_signalEnabled) {
    m_color.setValue(m_channel, value);
    emit colorChanged(m_color, false);
  }
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onSliderChanged(int value) {
  if (m_value == value) return;
  m_value = value;
  m_field->setText(QString::number(value));
  if (m_signalEnabled) {
    m_color.setValue(m_channel, value);
    emit colorChanged(m_color, true);
  }
}

//-----------------------------------------------------------------------------

void ColorChannelControl::onSliderReleased() {
  emit colorChanged(m_color, false);
}

//*****************************************************************************
//    StyleEditorPage  implementation
//*****************************************************************************

StyleEditorPage::StyleEditorPage(QWidget *parent) : QFrame(parent) {
  setFocusPolicy(Qt::NoFocus);

  // It is necessary for the style sheets
  setObjectName("styleEditorPage");
  setFrameStyle(QFrame::StyledPanel);

  m_editor = dynamic_cast<StyleEditor *>(parent);
}

//*****************************************************************************
//    ColorParameterSelector  implementation
//*****************************************************************************

ColorParameterSelector::ColorParameterSelector(QWidget *parent)
    : QWidget(parent)
    , m_index(-1)
    , m_chipSize(21, 21)
    , m_chipOrigin(0, 1)
    , m_chipDelta(21, 0) {
  setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
}

//-----------------------------------------------------------------------------

void ColorParameterSelector::paintEvent(QPaintEvent *event) {
  if (m_colors.empty()) return;
  QPainter p(this);
  int i;
  QRect currentChipRect = QRect();
  for (i = 0; i < (int)m_colors.size(); i++) {
    QRect chipRect(m_chipOrigin + i * m_chipDelta, m_chipSize);
    p.fillRect(chipRect, m_colors[i]);
    if (i == m_index) currentChipRect = chipRect;
  }
  // Current index border
  if (!currentChipRect.isEmpty()) {
    p.setPen(QColor(199, 202, 50));
    p.drawRect(currentChipRect.adjusted(0, 0, -1, -1));
    p.setPen(Qt::white);
    p.drawRect(currentChipRect.adjusted(1, 1, -2, -2));
    p.setPen(Qt::black);
    p.drawRect(currentChipRect.adjusted(2, 2, -3, -3));
  }
}

//-----------------------------------------------------------------------------

void ColorParameterSelector::setStyle(const TColorStyle &style) {
  int count = style.getColorParamCount();
  if (count <= 1) {
    clear();
    return;
  }
  if (m_colors.size() != count) {
    m_index = 0;
    m_colors.resize(count);
  }
  int i;
  for (i = 0; i < count; i++) {
    TPixel32 color = style.getColorParamValue(i);
    m_colors[i]    = QColor(color.r, color.g, color.b, color.m);
  }
  update();
}

//-----------------------------------------------------------------------------

void ColorParameterSelector::clear() {
  if (m_colors.size() != 0) m_colors.clear();
  m_index = -1;
  update();
}

//-----------------------------------------------------------------------------

void ColorParameterSelector::mousePressEvent(QMouseEvent *event) {
  QPoint pos = event->pos() - m_chipOrigin;
  int index  = pos.x() / m_chipDelta.x();
  QRect chipRect(index * m_chipDelta, m_chipSize);
  if (chipRect.contains(pos)) {
    m_index = index;
    emit colorParamChanged();
    update();
  }
}

//-----------------------------------------------------------------------------

QSize ColorParameterSelector::sizeHint() const {
  return QSize(m_chipOrigin.x() + (m_colors.size() - 1) * m_chipDelta.x() +
                   m_chipSize.width(),
               m_chipOrigin.y() + m_chipSize.height());
}

//*****************************************************************************
//    PlainColorPage  implementation
//*****************************************************************************

PlainColorPage::PlainColorPage(QWidget *parent)
    : StyleEditorPage(parent), m_color(), m_signalEnabled(true) {
  setFocusPolicy(Qt::NoFocus);

  // m_squaredColorWheel = new SquaredColorWheel(this);

  // m_verticalSlider = new ColorSliderBar(this, Qt::Vertical);

  m_hexagonalColorWheel = new HexagonalColorWheel(this);

  /*
  QButtonGroup *channelButtonGroup = new QButtonGroup();
  int i;
  for (i = 0; i<7; i++)
  {
          if (i != (int)eAlpha)
          {
                  QRadioButton *button = new QRadioButton(this);
                  m_modeButtons[i] = button;
                  if (i == 0) button->setChecked(true);
                  channelButtonGroup->addButton(button, i);
                  //slidersLayout->addWidget(button,i,0);
                  //とりあえず隠す
                  m_modeButtons[i]->hide();
          }
          else
                  m_modeButtons[i] = 0;

          m_channelControls[i] = new ColorChannelControl((ColorChannel)i, this);
          m_channelControls[i]->setColor(m_color);
          bool ret = connect(m_channelControls[i], SIGNAL(colorChanged(const
  ColorModel &, bool)),
                  this, SLOT(onControlChanged(const ColorModel &, bool)));
  }
  */
  for (int i = 0; i < 7; i++) {
    m_channelControls[i] = new ColorChannelControl((ColorChannel)i, this);
    m_channelControls[i]->setColor(m_color);
    bool ret = connect(m_channelControls[i],
                       SIGNAL(colorChanged(const ColorModel &, bool)), this,
                       SLOT(onControlChanged(const ColorModel &, bool)));
  }

  m_wheelFrame = new QFrame(this);
  m_hsvFrame   = new QFrame(this);
  m_alphaFrame = new QFrame(this);
  m_rgbFrame   = new QFrame(this);

  m_slidersContainer = new QFrame(this);
  m_vSplitter        = new QSplitter(this);

  //プロパティの設定
  // channelButtonGroup->setExclusive(true);

  m_wheelFrame->setObjectName("PlainColorPageParts");
  m_hsvFrame->setObjectName("PlainColorPageParts");
  m_alphaFrame->setObjectName("PlainColorPageParts");
  m_rgbFrame->setObjectName("PlainColorPageParts");

  m_vSplitter->setOrientation(Qt::Vertical);
  m_vSplitter->setFocusPolicy(Qt::NoFocus);

  // m_verticalSlider->hide();
  // m_squaredColorWheel->hide();
  // m_ghibliColorWheel->hide();

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setSpacing(0);
  mainLayout->setMargin(0);
  {
    QHBoxLayout *wheelLayout = new QHBoxLayout();
    wheelLayout->setMargin(5);
    wheelLayout->setSpacing(0);
    { wheelLayout->addWidget(m_hexagonalColorWheel); }
    m_wheelFrame->setLayout(wheelLayout);
    m_vSplitter->addWidget(m_wheelFrame);

    QVBoxLayout *slidersLayout = new QVBoxLayout();
    slidersLayout->setMargin(0);
    slidersLayout->setSpacing(0);
    {
      QVBoxLayout *hsvLayout = new QVBoxLayout();
      hsvLayout->setMargin(4);
      hsvLayout->setSpacing(4);
      {
        hsvLayout->addWidget(m_channelControls[eHue]);
        hsvLayout->addWidget(m_channelControls[eSaturation]);
        hsvLayout->addWidget(m_channelControls[eValue]);
      }
      m_hsvFrame->setLayout(hsvLayout);
      slidersLayout->addWidget(m_hsvFrame, 3);

      QVBoxLayout *alphaLayout = new QVBoxLayout();
      alphaLayout->setMargin(4);
      alphaLayout->setSpacing(4);
      { alphaLayout->addWidget(m_channelControls[eAlpha]); }
      m_alphaFrame->setLayout(alphaLayout);
      slidersLayout->addWidget(m_alphaFrame, 1);

      QVBoxLayout *rgbLayout = new QVBoxLayout();
      rgbLayout->setMargin(4);
      rgbLayout->setSpacing(4);
      {
        rgbLayout->addWidget(m_channelControls[eRed]);
        rgbLayout->addWidget(m_channelControls[eGreen]);
        rgbLayout->addWidget(m_channelControls[eBlue]);
      }
      m_rgbFrame->setLayout(rgbLayout);
      slidersLayout->addWidget(m_rgbFrame, 3);
    }
    m_slidersContainer->setLayout(slidersLayout);
    m_vSplitter->addWidget(m_slidersContainer);

    mainLayout->addWidget(m_vSplitter, 1);
  }
  setLayout(mainLayout);

  // QList<int> list;
  // list << rect().height() / 2 << rect().height() / 2;
  // m_vSplitter->setSizes(list);
  m_vSplitter->setStretchFactor(0, 50);
  m_vSplitter->setStretchFactor(1, 0);

  // connect(m_squaredColorWheel, SIGNAL(colorChanged(const ColorModel &,
  // bool)),
  //	this, SLOT(onWheelChanged(const ColorModel &, bool)));
  connect(m_hexagonalColorWheel, SIGNAL(colorChanged(const ColorModel &, bool)),
          this, SLOT(onWheelChanged(const ColorModel &, bool)));
  // m_verticalSlider->setMaximumSize(20,150);
  // connect(m_verticalSlider, SIGNAL(valueChanged(int)), this,
  // SLOT(onWheelSliderChanged(int)));
  // connect(m_verticalSlider, SIGNAL(valueChanged()), this,
  // SLOT(onWheelSliderReleased()));
  // connect( m_verticalSlider,		SIGNAL(sliderReleased()),	this,
  // SLOT(onWheelSliderReleased()));
  // connect(channelButtonGroup, SIGNAL(buttonClicked(int)), this,
  // SLOT(setWheelChannel(int)));
}

//-----------------------------------------------------------------------------

void PlainColorPage::resizeEvent(QResizeEvent *) {
  int w = width();
  int h = height();

  int parentW = parentWidget()->width();
}

//-----------------------------------------------------------------------------

void PlainColorPage::updateControls() {
  int i;
  for (i = 0; i < 7; i++) {
    m_channelControls[i]->setColor(m_color);
    m_channelControls[i]->update();
  }
  /*
  m_squaredColorWheel->setColor(m_color);
  m_squaredColorWheel->update();
  */

  m_hexagonalColorWheel->setColor(m_color);
  m_hexagonalColorWheel->update();

  /*
bool signalsBlocked = m_verticalSlider->blockSignals(true);
  m_verticalSlider->setColor(m_color);
int value = m_color.getValue(m_verticalSlider->getChannel());
m_verticalSlider->setValue(value);
  m_verticalSlider->update();
m_verticalSlider->blockSignals(signalsBlocked);
*/
}

//-----------------------------------------------------------------------------

void PlainColorPage::setColor(const TColorStyle &style,
                              int colorParameterIndex) {
  TPixel32 newPixel = style.getColorParamValue(colorParameterIndex);
  if (m_color.getTPixel() == newPixel) return;
  bool oldSignalEnabled = m_signalEnabled;
  m_signalEnabled       = false;
  m_color.setTPixel(newPixel);
  updateControls();
  m_signalEnabled = oldSignalEnabled;
}

//-----------------------------------------------------------------------------

void PlainColorPage::setIsVertical(bool isVertical) {
  // if (m_isVertical == isVertical) return;
  // not returning even if it already is the same orientation
  // to take advantage of the resizing here
  // this is useful for the first time the splitter is set
  // afterwards, it will be overridden by the saved state
  // from settings.
  m_isVertical = isVertical;
  if (isVertical) {
    m_vSplitter->setOrientation(Qt::Vertical);
    QList<int> sectionSizes;
    // maximize color wheel space
    sectionSizes << height() - 1 << 1;
    m_vSplitter->setSizes(sectionSizes);
  } else {
    m_vSplitter->setOrientation(Qt::Horizontal);
    QList<int> sectionSizes;
    sectionSizes << width() / 2 << width() / 2;
    m_vSplitter->setSizes(sectionSizes);
  }
}

//-----------------------------------------------------------------------------

void PlainColorPage::toggleOrientation() { setIsVertical(!m_isVertical); }

//-----------------------------------------------------------------------------

QByteArray PlainColorPage::getSplitterState() {
  return m_vSplitter->saveState();
}

//-----------------------------------------------------------------------------

void PlainColorPage::setSplitterState(QByteArray state) {
  m_vSplitter->restoreState(state);
}

//-----------------------------------------------------------------------------

void PlainColorPage::updateColorCalibration() {
  if (m_hexagonalColorWheel->isVisible())
    m_hexagonalColorWheel->updateColorCalibration();
  else
    m_hexagonalColorWheel->cueCalibrationUpdate();
}

//-----------------------------------------------------------------------------
/*
void PlainColorPage::setWheelChannel(int channel)
{
        assert(0<=channel && channel<7);
        m_squaredColorWheel->setChannel(channel);
  bool signalsBlocked = m_verticalSlider->signalsBlocked();
        m_verticalSlider->blockSignals(true);
        m_verticalSlider->setChannel((ColorChannel)channel);
  m_verticalSlider->setRange(0,ChannelMaxValues[channel]);
  m_verticalSlider->setValue(m_color.getValue((ColorChannel)channel));
  m_verticalSlider->update();
  m_verticalSlider->blockSignals(signalsBlocked);
}
*/
//-----------------------------------------------------------------------------

void PlainColorPage::onControlChanged(const ColorModel &color,
                                      bool isDragging) {
  if (!(m_color == color)) {
    m_color = color;
    updateControls();
  }

  if (m_signalEnabled) emit colorChanged(m_color, isDragging);
}

//-----------------------------------------------------------------------------

void PlainColorPage::onWheelChanged(const ColorModel &color, bool isDragging) {
  if (!(m_color == color)) {
    m_color = color;
    updateControls();
  }
  if (m_signalEnabled) emit colorChanged(m_color, isDragging);
}

//-----------------------------------------------------------------------------
/*
void PlainColorPage::onWheelSliderChanged(int value)
{
        if(m_color.getValue(m_verticalSlider->getChannel()) == value) return;
        m_color.setValue(m_verticalSlider->getChannel(), value);
  updateControls();
  if(m_signalEnabled)
    emit colorChanged(m_color, true);
}
*/
//-----------------------------------------------------------------------------
/*
void PlainColorPage::onWheelSliderReleased()
{
  emit colorChanged(m_color, false);
}
*/

//*****************************************************************************
//    StyleChooserPage  implementation
//*****************************************************************************

StyleChooserPage::StyleChooserPage(TFilePath styleFolder, QWidget *parent)
    : StyleEditorPage(parent)
    , m_chipOrigin(5, 3)
    , m_chipSize(25, 25)
    , m_chipPerRow(0)
    , m_currentIndex(-1)
    , m_stylesFolder(styleFolder)
    , m_styleSetName("Unknown Style Set")
    , m_allowPageDelete(true)
    , m_favorite(false)
    , m_allowFavorite(false)
    , m_external(false) {}

//-----------------------------------------------------------------------------

void StyleChooserPage::paintEvent(QPaintEvent *) {
  if (loadIfNeeded()) computeSize();

  QPainter p(this);
  // p.setRenderHint(QPainter::SmoothPixmapTransform);
  bool origAA = p.testRenderHint(QPainter::HighQualityAntialiasing);
  bool origS  = p.testRenderHint(QPainter::SmoothPixmapTransform);
  if (m_chipPerRow == 0 || getChipCount() == 0) return;

  int w      = parentWidget()->width();
  int chipLx = m_chipSize.width(), chipLy = m_chipSize.height();
  int nX = m_chipPerRow;
  int nY = (getChipCount() + m_chipPerRow - 1) / m_chipPerRow;
  int x0 = m_chipOrigin.x();
  int y0 = m_chipOrigin.y();
  int i, j;
  QRect currentIndexRect = QRect();
  int count              = 0;
  for (i = 0; i < nY; i++)
    for (j = 0; j < nX; j++) {
      QRect rect(x0 + chipLx * j + 2, y0 + chipLy * i + 2, chipLx, chipLy);

      drawChip(p, rect, count);
      p.setPen(Qt::black);
      p.drawRect(rect);

      // Draw selection check boxes
      if (std::find(m_selection.begin(), m_selection.end(), count) !=
          m_selection.end()) {
        int x = rect.topLeft().x();
        int y = rect.topRight().y();
        QRect selectRect(x, y, 15, 15);

        // Background
        p.fillRect(selectRect, QBrush(Qt::white));
        p.setPen(Qt::black);
        p.drawRect(selectRect);

        // Actual checkmark
        QPainterPath checkmark(QPointF(x + 3, y + 8));
        checkmark.lineTo(QPointF(x + 6, y + 12));
        checkmark.lineTo(QPointF(x + 13, y + 5));

        QPen checkPen(Qt::red);
        checkPen.setWidthF(1.5);
        p.setPen(checkPen);
        if (!origAA) p.setRenderHint(QPainter::HighQualityAntialiasing, true);
        if (!origS) p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPath(checkmark);
        p.setRenderHint(QPainter::HighQualityAntialiasing, origAA);
        p.setRenderHint(QPainter::SmoothPixmapTransform, origS);
      }

      if (m_currentIndex == count) currentIndexRect = rect;

      count++;
      if (count >= getChipCount()) break;
    }

  if (!currentIndexRect.isEmpty()) {
    // Draw the curentIndex border
    p.setPen(Qt::white);
    p.drawRect(currentIndexRect);
    p.setPen(QColor(199, 202, 50));
    p.drawRect(currentIndexRect.adjusted(1, 1, -1, -1));
    p.setPen(Qt::white);
    p.drawRect(currentIndexRect.adjusted(2, 2, -2, -2));
    p.setPen(Qt::black);
    p.drawRect(currentIndexRect.adjusted(3, 3, -3, -3));
  }
}

//-----------------------------------------------------------------------------

void StyleChooserPage::computeSize() {
  int w        = width();
  m_chipPerRow = (w - 15) / m_chipSize.width();
  int rowCount = 0;
  if (m_chipPerRow != 0)
    rowCount = (getChipCount() + m_chipPerRow - 1) / m_chipPerRow;
  setMinimumSize(3 * m_chipSize.width(), rowCount * m_chipSize.height() + 10);
  update();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::onTogglePage(bool toggled) {
  if (toggled) computeSize();
  setVisible(toggled);
}

//-----------------------------------------------------------------------------

void StyleChooserPage::onRemoveStyleFromSet() {
  if (m_currentIndex <= 0) return;

  if (!isMyFavoriteSet()) {
    int ret = DVGui::MsgBox(
        QObject::tr("Removing a Style will permanently delete the style file. "
                    "This cannot be undone!\nAre you sure?")
            .arg(getStyleSetName()),
        QObject::tr("Ok"), QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) return;
  }

  m_selection.clear();
  m_selection.push_back(m_currentIndex);
  removeSelectedStylesFromSet(m_selection);
  m_selection.clear();
};

//-----------------------------------------------------------------------------

void StyleChooserPage::onEmptySet() {
  if (!isMyFavoriteSet()) {
    int ret = DVGui::MsgBox(
        QObject::tr(
            "Emptying Set \"%1\" will permanently delete all style files "
            "for this set. This cannot be undone!\nAre you sure?")
            .arg(getStyleSetName()),
        QObject::tr("Ok"), QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) return;
  }

  m_selection.clear();
  for (int index = 1; index < getChipCount(); index++)
    m_selection.push_back(index);
  removeSelectedStylesFromSet(m_selection);
  m_selection.clear();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::onAddStyleToFavorite() {
  if ((isFavorite() && !allowFavorite()) || m_currentIndex <= 0) return;

  TFilePath setPath = ToonzFolder::getMyFavoritesFolder() + "library";

  m_selection.clear();
  m_selection.push_back(m_currentIndex);
  switch (m_pageType) {
  case StylePageType::Texture:
    setPath += TFilePath("textures");
    break;
  case StylePageType::VectorBrush:
  case StylePageType::VectorCustom:
  case StylePageType::VectorGenerated:
    setPath += TFilePath("vector styles");
    break;
  case StylePageType::Raster:
    setPath += TFilePath("raster styles");
    break;
  }
  addSelectedStylesToSet(m_selection, setPath);
  m_selection.clear();
};

//-----------------------------------------------------------------------------

void StyleChooserPage::onAddStyleToPalette() {
  if (m_currentIndex <= 0) return;

  m_selection.clear();
  m_selection.push_back(m_currentIndex);
  addSelectedStylesToPalette(m_selection);
  m_selection.clear();
};

//-----------------------------------------------------------------------------

void StyleChooserPage::onCopyStyleToSet() {
  if (m_currentIndex <= 0) return;

  QAction *action = dynamic_cast<QAction *>(sender());
  QString setName = action->data().toString();

  m_selection.clear();
  m_selection.push_back(m_currentIndex);
  addSelectedStylesToSet(m_selection, m_editor->getSetStyleFolder(setName));
  m_selection.clear();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::onMoveStyleToSet() {
  if (m_currentIndex <= 0) return;

  QAction *action = dynamic_cast<QAction *>(sender());
  QString setName = action->data().toString();

  m_selection.clear();
  m_selection.push_back(m_currentIndex);
  addSelectedStylesToSet(m_selection, m_editor->getSetStyleFolder(setName));
  removeSelectedStylesFromSet(m_selection);
  m_selection.clear();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::onAddSetToPalette() {
  m_selection.clear();
  for (int index = 1; index < getChipCount(); index++)
    m_selection.push_back(index);
  addSelectedStylesToPalette(m_selection);
  m_selection.clear();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::onUpdateFavorite() { updateFavorite(); };

//-----------------------------------------------------------------------------

void StyleChooserPage::onReloadStyleSet() { loadItems(); }

//-----------------------------------------------------------------------------

void StyleChooserPage::onRenameStyleSet() { m_editor->editStyleSetName(this); }

//-----------------------------------------------------------------------------

void StyleChooserPage::onRemoveStyleSet() {
  if (!isMyFavoriteSet()) {
    int ret = DVGui::MsgBox(
        QObject::tr(
            "Removing Style Set \"%1\" will permanently delete all style "
            "files for this set. This cannot be undone!\nAre you sure?")
            .arg(getStyleSetName()),
        QObject::tr("Ok"), QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) return;
  }

  try {
    TSystem::rmDirTree(m_stylesFolder);
  } catch (TSystemException se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  }

  m_editor->removeStyleSet(this);
}

//-----------------------------------------------------------------------------

void StyleChooserPage::onLabelContextMenu(const QPoint &pos) {
  QContextMenuEvent *event =
      new QContextMenuEvent(QContextMenuEvent::Reason::Mouse, pos);
  processContextMenuEvent(event);
}

//-----------------------------------------------------------------------------

int StyleChooserPage::posToIndex(const QPoint &pos) const {
  if (m_chipPerRow == 0) return -1;

  int x = (pos.x() - m_chipOrigin.x() - 2) / m_chipSize.width();
  if (x >= m_chipPerRow) return -1;

  int y = (pos.y() - m_chipOrigin.y() - 2) / m_chipSize.height();

  int index = x + m_chipPerRow * y;
  if (index < 0 || index >= getChipCount()) return -1;

  return index;
}

//-----------------------------------------------------------------------------

void StyleChooserPage::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::RightButton) return;

  QPoint pos       = event->pos();
  int currentIndex = posToIndex(pos);
  if (currentIndex < 0) {
    if (!m_editor->isCtrlPressed()) m_editor->clearSelection();
    return;
  }

  if (!m_editor->isCtrlPressed()) {
    // If in select mode, just cancel select mode and don't change style
    if (m_editor->isSelecting()) {
      m_editor->clearSelection();
      return;
    }

    m_currentIndex = currentIndex;
  }

  if (m_editor->isAltPressed()) {
    onAddStyleToPalette();
  } else if (m_editor->isCtrlPressed()) {
    if (currentIndex > 0 && currentIndex < getChipCount() &&
        (m_pageType != StylePageType::Texture ||
         currentIndex < getChipCount() - 1)) {
      std::vector<int>::iterator it =
          std::find(m_selection.begin(), m_selection.end(), currentIndex);
      if (it == m_selection.end())
        m_selection.push_back(currentIndex);
      else
        m_selection.erase(it);
    }
  } else {
    onSelect(currentIndex);
  }

  update();
}

//-----------------------------------------------------------------------------

void StyleChooserPage::enterEvent(QEvent *event) {
  TApplication *app = m_editor->getApplication();
  if (app)
    app->showMessage(tr("Style Set Manager:              %1+click - Add Style "
                        "to Palette              %2+click - Multi-Style Select")
                         .arg(trModKey("Alt"))
                         .arg(trModKey("Ctrl")),
                     0);
}

//-----------------------------------------------------------------------------

void StyleChooserPage::mouseReleaseEvent(QMouseEvent *event) {}

//-----------------------------------------------------------------------------

bool canCopyTo(StylePageType fromPageType, StylePageType toPageType,
               bool toFavorite) {
  // Can always copy from like pages or to a favorites page
  if ((fromPageType != StylePageType::VectorGenerated &&
       fromPageType == toPageType) ||
      toFavorite)
    return true;

  // Vectors have 3 types to check
  // Generated can go to Custom but not Brush
  // Brush can go to Custom but not Generated
  // Custom cannot go to Generated or Brush
  if ((fromPageType == StylePageType::VectorBrush ||
       fromPageType == StylePageType::VectorGenerated) &&
      toPageType == StylePageType::VectorCustom)
    return true;

  return false;
}

void StyleChooserPage::contextMenuEvent(QContextMenuEvent *event) {
  QPoint pos       = event->pos();
  int currentIndex = posToIndex(pos);

  QMenu *menu = new QMenu(this);
  QAction *action;

  std::vector<StyleChooserPage *> styleSets =
      m_editor->getStyleSetList(m_pageType);

  if (m_editor->isSelecting()) {
    action = new QAction(this);
    action->setText(tr("Add Selected to Palette"));
    connect(action, SIGNAL(triggered()), m_editor,
            SLOT(onAddSelectedStylesToPalette()));
    menu->addAction(action);

    action = new QAction(this);
    if (m_editor->isSelectingFavoritesOnly()) {
      action->setText(tr("Remove Selected from Favorites"));
      connect(action, SIGNAL(triggered()), m_editor,
              SLOT(onRemoveSelectedStylesFromFavorites()));
    } else {
      action->setText(tr("Add Selected to Favorites"));
      connect(action, SIGNAL(triggered()), m_editor,
              SLOT(onAddSelectedStylesToFavorites()));
      // Disable if selecting favorites also
      if (m_editor->isSelectingFavorites()) action->setDisabled(true);
    }
    menu->addAction(action);

    QMenu *styleSetMenu = new QMenu(tr("Copy Selected to Style Set..."), this);
    for (int i = 1; i < styleSets.size(); i++) {
      StyleChooserPage *page = styleSets[i];
      QAction *subAction     = new QAction(page->getStyleSetName());
      subAction->setData(page->getStyleSetName());
      connect(subAction, SIGNAL(triggered()), m_editor,
              SLOT(onCopySelectedStylesToSet()));
      if (page == this || page->isExternal() || page->getSelection().size() ||
          !canCopyTo(m_pageType, page->getPageType(), page->isFavorite()))
        subAction->setDisabled(true);
      styleSetMenu->addAction(subAction);
    }
    if (styleSets.size() < 2) styleSetMenu->setDisabled(true);
    menu->addMenu(styleSetMenu);

    styleSetMenu = new QMenu(tr("Move Selected to Style Set..."), this);
    for (int i = 1; i < styleSets.size(); i++) {
      StyleChooserPage *page = styleSets[i];
      QAction *subAction     = new QAction(page->getStyleSetName());
      subAction->setData(page->getStyleSetName());
      connect(subAction, SIGNAL(triggered()), m_editor,
              SLOT(onMoveSelectedStylesToSet()));
      if (page == this || page->isExternal() || page->getSelection().size() ||
          !canCopyTo(m_pageType, page->getPageType(), page->isFavorite()))
        subAction->setDisabled(true);
      styleSetMenu->addAction(subAction);
    }
    if (styleSets.size() < 2 || m_pageType == StylePageType::VectorGenerated)
      styleSetMenu->setDisabled(true);
    menu->addMenu(styleSetMenu);

    action = new QAction(this);
    action->setText(tr("Remove Selected from Sets"));
    connect(action, SIGNAL(triggered()), m_editor,
            SLOT(onRemoveSelectedStyleFromSet()));
    for (int i = 1; i < styleSets.size(); i++) {
      StyleChooserPage *page = styleSets[i];
      if (page->getSelection().size() &&
          (page->isExternal() ||
           page->getPageType() == StylePageType::VectorGenerated)) {
        action->setDisabled(true);
        break;
      }
    }
    menu->addAction(action);

    menu->exec(event->globalPos());
    return;
  }

  if (currentIndex >= 0) {
    m_currentIndex = currentIndex;

    action = new QAction(this);
    action->setText(tr("Add to Palette"));
    connect(action, SIGNAL(triggered()), this, SLOT(onAddStyleToPalette()));
    if (m_currentIndex == 0 || (m_pageType == StylePageType::Texture &&
                                m_currentIndex == getChipCount() - 1))
      action->setDisabled(true);
    menu->addAction(action);

    action = new QAction(this);
    action->setText(tr("Add to Favorites"));
    connect(action, SIGNAL(triggered()), this, SLOT(onAddStyleToFavorite()));
    if (m_currentIndex == 0 || (m_pageType == StylePageType::Texture &&
                                m_currentIndex == getChipCount() - 1) ||
        !allowFavorite())
      action->setDisabled(true);
    menu->addAction(action);

    QMenu *styleSetMenu = new QMenu(tr("Copy to Style Set..."), this);
    for (int i = 1; i < styleSets.size(); i++) {
      StyleChooserPage *page = styleSets[i];
      QAction *subAction     = new QAction(page->getStyleSetName());
      subAction->setData(page->getStyleSetName());
      connect(subAction, SIGNAL(triggered()), this, SLOT(onCopyStyleToSet()));
      if (page == this || page->isExternal() ||
          !canCopyTo(m_pageType, page->getPageType(), page->isFavorite()))
        subAction->setDisabled(true);
      styleSetMenu->addAction(subAction);
    }
    if (m_currentIndex == 0 || (m_pageType == StylePageType::Texture &&
                                m_currentIndex == getChipCount() - 1) ||
        styleSets.size() <= 2)
      styleSetMenu->setDisabled(true);
    menu->addMenu(styleSetMenu);

    styleSetMenu = new QMenu(tr("Move to Style Set..."), this);
    for (int i = 1; i < styleSets.size(); i++) {
      StyleChooserPage *page = styleSets[i];
      QAction *subAction     = new QAction(page->getStyleSetName());
      subAction->setData(page->getStyleSetName());
      connect(subAction, SIGNAL(triggered()), this, SLOT(onMoveStyleToSet()));
      if (page == this || page->isExternal() ||
          !canCopyTo(m_pageType, page->getPageType(), page->isFavorite()))
        subAction->setDisabled(true);
      styleSetMenu->addAction(subAction);
    }
    if (m_currentIndex == 0 || (m_pageType == StylePageType::Texture &&
                                m_currentIndex == getChipCount() - 1) ||
        m_pageType == StylePageType::VectorGenerated || styleSets.size() <= 2)
      styleSetMenu->setDisabled(true);
    menu->addMenu(styleSetMenu);

    action = new QAction(this);
    action->setText(tr("Remove from Set"));
    connect(action, SIGNAL(triggered()), this, SLOT(onRemoveStyleFromSet()));
    if (m_currentIndex == 0 || (m_pageType == StylePageType::Texture &&
                                m_currentIndex == getChipCount() - 1) ||
        m_pageType == StylePageType::VectorGenerated || isExternal())
      action->setDisabled(true);
    menu->addAction(action);

    menu->addSeparator();

    action = new QAction(this);
    action->setText(tr("Add Set to Palette"));
    connect(action, SIGNAL(triggered()), this, SLOT(onAddSetToPalette()));
    if (getChipCount() <= 1) action->setDisabled(true);
    menu->addAction(action);

    action = new QAction(this);
    action->setText(tr("Empty Set"));
    connect(action, SIGNAL(triggered()), this, SLOT(onEmptySet()));
    if (getChipCount() <= 1 ||
        (m_pageType == StylePageType::Texture && getChipCount() <= 2) ||
        isExternal())
      action->setDisabled(true);
    menu->addAction(action);

    menu->addSeparator();
  }

  action = new QAction(this);
  action->setText(tr("New Style Set..."));
  connect(action, SIGNAL(triggered()), m_editor, SLOT(onAddNewStyleSet()));
  menu->addAction(action);

  action = new QAction(this);
  action->setText(tr("Rename Style Set..."));
  connect(action, SIGNAL(triggered()), this, SLOT(onRenameStyleSet()));
  if (isMyFavoriteSet() || isRootFolder() || isExternal())
    action->setDisabled(true);
  menu->addAction(action);

  action = new QAction(this);
  action->setText(tr("Reload Style Set"));
  connect(action, SIGNAL(triggered()), this, SLOT(onReloadStyleSet()));
  if (m_pageType == StylePageType::VectorGenerated ||
      m_styleSetName == "Unknown Style Set")
    action->setDisabled(true);
  menu->addAction(action);

  action = new QAction(this);
  action->setText(tr("Scan for Style Set Changes"));
  connect(action, SIGNAL(triggered()), m_editor, SLOT(onScanStyleSetChanges()));
  menu->addAction(action);

  menu->addSeparator();

  if (m_styleSetName != "Unknown Style Set") {
    action = new QAction(this);
    action->setText(tr("Remove '%1' Style Set").arg(m_styleSetName));
    if (m_stylesFolder == TFilePath() || (isFavorite() && !allowFavorite()) ||
        !canDeletePage())
      action->setDisabled(true);
    connect(action, SIGNAL(triggered()), this, SLOT(onRemoveStyleSet()));
    menu->addAction(action);
  }

  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

bool StyleChooserPage::copyFilesToStyleFolder(TFilePathSet srcFiles,
                                              TFilePath destDir) {
  if (srcFiles.empty()) return false;

  if (!TFileStatus(destDir).doesExist()) try {
      TSystem::mkDir(destDir);
    } catch (TSystemException se) {
      DVGui::warning(QString::fromStdWString(se.getMessage()));
      return false;
    }

  TFilePathSet::iterator it;
  for (it = srcFiles.begin(); it != srcFiles.end(); it++) try {
      TSystem::copyFile((destDir + it->withoutParentDir()), *it, true);
    } catch (...) {
    }

  return true;
}

//-----------------------------------------------------------------------------

bool StyleChooserPage::deleteFilesFromStyleFolder(TFilePathSet targetFiles) {
  if (targetFiles.empty()) return false;

  TFilePathSet::iterator it;
  for (it = targetFiles.begin(); it != targetFiles.end(); it++) try {
      if (!TSystem::doesExistFileOrLevel(*it)) return false;
      TSystem::deleteFile(*it);
    } catch (...) {
    }
}

//*****************************************************************************
//    CustomStyleChooser  definition
//*****************************************************************************

class CustomStyleChooserPage final : public StyleChooserPage {
  QString m_filters;
  CustomStyleManager *m_styleManager;

public:
  CustomStyleChooserPage(TFilePath stylesFolder = TFilePath(),
                         QString filters = QString(), QWidget *parent = 0)
      : StyleChooserPage(stylesFolder, parent) {
    setPageType(StylePageType::VectorCustom);
    m_filters      = filters;
    m_styleManager = TStyleManager::instance()->getCustomStyleManager(
        m_stylesFolder, m_filters);
  }

  ~CustomStyleChooserPage() {
    TStyleManager::instance()->removeCustomStyleFolder(m_stylesFolder);
  }

  void setFavorite(bool favorite) override {
    m_favorite = favorite;
    if (favorite && m_styleManager)
      connect(m_styleManager, SIGNAL(itemsUpdated()), this,
              SLOT(onUpdateFavorite()));
  }

  bool event(QEvent *e) override;

  void showEvent(QShowEvent *) override {
    connect(m_styleManager, SIGNAL(itemsUpdated()), this, SLOT(computeSize()));
    m_styleManager->loadItems();
  }
  void hideEvent(QHideEvent *) override {
    disconnect(m_styleManager, SIGNAL(itemsUpdated()), this,
               SLOT(computeSize()));
  }
  bool loadIfNeeded() override { return false; }  // serve?
  /*
if(!m_loaded) {loadItems(); m_loaded=true;return true;}
else return false;
}
  */
  void loadItems() override { m_styleManager->loadItems(); }

  int getChipCount() const override {
    return m_styleManager->getPatternCount() + 1;
  }

  void drawChip(QPainter &p, QRect rect, int index) override {
    if (index == 0) {
      static QImage noSpecialStyleImage(":Resources/no_vectorbrush.png");
      p.drawImage(rect, noSpecialStyleImage);
    } else {
      index -= 1;
      assert(0 <= index && index <= getChipCount());
      CustomStyleManager::PatternData pattern =
          m_styleManager->getPattern(index);
      if (pattern.m_image && !pattern.m_image->isNull())
        p.drawImage(rect, *pattern.m_image);
    }
  }
  void onSelect(int index) override;

  void removeSelectedStylesFromSet(std::vector<int> selection) override;
  void addSelectedStylesToSet(std::vector<int> selection,
                              TFilePath setPath) override;
  void updateFavorite() override { emit refreshFavorites(); };
  void addSelectedStylesToPalette(std::vector<int> selection) override;
  void changeStyleSetFolder(TFilePath newPath) override {
    TStyleManager::instance()->changeStyleSetFolder(m_styleManager, newPath);
    m_stylesFolder = newPath;
  };

  bool isLoading() override { return m_styleManager->isLoading(); }
};

//-----------------------------------------------------------------------------

bool CustomStyleChooserPage::event(QEvent *e) {
  // Intercept tooltip events
  if (e->type() != QEvent::ToolTip) return StyleChooserPage::event(e);

  // see StyleChooserPage::paintEvent
  QHelpEvent *he = static_cast<QHelpEvent *>(e);

  int chipIdx   = posToIndex(he->pos());
  int chipCount = m_styleManager->getPatternCount();
  if (chipIdx == 0) {
    QToolTip::showText(he->globalPos(),
                       QObject::tr("Plain color", "CustomStyleChooserPage"));
  } else {
    chipIdx--;
    if (chipIdx < 0 || chipIdx >= chipCount) return false;

    CustomStyleManager::PatternData pattern =
        m_styleManager->getPattern(chipIdx);
    QToolTip::showText(he->globalPos(),
                       QString::fromStdString(pattern.m_patternName));
  }
  return true;
}

//-----------------------------------------------------------------------------

void CustomStyleChooserPage::onSelect(int index) {
  if (index < 0 || index >= getChipCount()) return;

  if (index == 0) {
    TSolidColorStyle cs(TPixel32::Black);
    emit styleSelected(cs);
  } else {
    index--;
    CustomStyleManager::PatternData pattern = m_styleManager->getPattern(index);

    if (m_currentIndex < 0) return;

    std::string name = pattern.m_patternName;
    if (pattern.m_isVector) {
      TVectorImagePatternStrokeStyle cs(m_stylesFolder, name);
      emit styleSelected(cs);
    } else if (pattern.m_isGenerated) {
      QString name          = QString::fromStdString(pattern.m_path.getName());
      QStringList nameParts = name.split("-");
      int tagId             = std::stoi(nameParts[1].toStdString());
      TColorStyle *cs       = TColorStyle::create(tagId);
      emit styleSelected(*cs);
    } else {
      TRasterImagePatternStrokeStyle cs(m_stylesFolder, name);
      emit styleSelected(cs);
    }
  }
}

//-----------------------------------------------------------------------------

void CustomStyleChooserPage::addSelectedStylesToPalette(
    std::vector<int> selection) {
  if (!selection.size()) return;

  for (int i = 0; i < selection.size(); i++) {
    CustomStyleManager::PatternData pattern =
        m_styleManager->getPattern(selection[i] - 1);

    std::string name = pattern.m_patternName;
    if (pattern.m_isVector) {
      TVectorImagePatternStrokeStyle cs(m_stylesFolder, name);
      m_editor->addToPalette(cs);
    } else if (pattern.m_isGenerated) {
      QString name          = QString::fromStdString(pattern.m_path.getName());
      QStringList nameParts = name.split("-");
      int tagId             = std::stoi(nameParts[1].toStdString());
      TColorStyle *cs       = TColorStyle::create(tagId);
      m_editor->addToPalette(*cs);
    } else {
      TRasterImagePatternStrokeStyle cs(m_stylesFolder, name);
      m_editor->addToPalette(cs);
    }
  }
}

//-----------------------------------------------------------------------------

void CustomStyleChooserPage::removeSelectedStylesFromSet(
    std::vector<int> selection) {
  if (!selection.size()) return;

  bool deleted = false;
  for (int i = 0; i < selection.size(); i++) {
    CustomStyleManager::PatternData pattern =
        m_styleManager->getPattern(selection[i] - 1);

    if (pattern.m_isGenerated) {
      try {
        TSystem::deleteFile(pattern.m_path);
        deleted = true;
      } catch (...) {
        continue;
      }
    } else {
      std::string name = pattern.m_patternName;

      TFilePathSet fileList;

      QDir patternDir(m_stylesFolder.getQString());
      patternDir.setNameFilters(
          QStringList(QString::fromStdString(name) + ".*" +
                      QString::fromStdString(pattern.m_path.getType())));
      TSystem::readDirectory(fileList, patternDir, false);

      deleted |= deleteFilesFromStyleFolder(fileList);
    }
  }

  if (deleted) loadItems();
}

//-----------------------------------------------------------------------------

void CustomStyleChooserPage::addSelectedStylesToSet(std::vector<int> selection,
                                                    TFilePath setPath) {
  if (!selection.size() || setPath.isEmpty()) return;

  bool added = false;
  for (int i = 0; i < selection.size(); i++) {
    CustomStyleManager::PatternData pattern =
        m_styleManager->getPattern(selection[i] - 1);
    std::string name = pattern.m_patternName;

    TFilePathSet fileList;

    QDir patternDir(m_stylesFolder.getQString());
    patternDir.setNameFilters(
        QStringList(QString::fromStdString(pattern.m_path.getName()) + ".*" +
                    QString::fromStdString(pattern.m_path.getType())));
    TSystem::readDirectory(fileList, patternDir, false);

    added |= copyFilesToStyleFolder(fileList, setPath);
  }

  if (added) m_editor->setUpdated(setPath);
}

//*****************************************************************************
//    VectorBrushStyleChooser  definition
//*****************************************************************************

class VectorBrushStyleChooserPage final : public StyleChooserPage {
  QString m_filters;
  CustomStyleManager *m_styleManager;

public:
  VectorBrushStyleChooserPage(TFilePath stylesFolder = TFilePath(),
                              QString filters = QString(), QWidget *parent = 0)
      : StyleChooserPage(stylesFolder, parent) {
    setPageType(StylePageType::VectorBrush);
    m_filters      = filters;
    m_chipSize     = QSize(60, 25);
    m_styleManager = TStyleManager::instance()->getCustomStyleManager(
        m_stylesFolder, m_filters, m_chipSize);
  }

  ~VectorBrushStyleChooserPage() {
    TStyleManager::instance()->removeCustomStyleFolder(m_stylesFolder);
  }

  void setFavorite(bool favorite) override {
    m_favorite = favorite;
    if (favorite && m_styleManager)
      connect(m_styleManager, SIGNAL(itemsUpdated()), this,
              SLOT(onUpdateFavorite()));
  }

  bool event(QEvent *e) override;

  void showEvent(QShowEvent *) override {
    connect(m_styleManager, SIGNAL(itemsUpdated()), this, SLOT(computeSize()));
    m_styleManager->loadItems();
  }
  void hideEvent(QHideEvent *) override {
    disconnect(m_styleManager, SIGNAL(itemsUpdated()), this,
               SLOT(computeSize()));
  }
  bool loadIfNeeded() override { return false; }
  void loadItems() override { m_styleManager->loadItems(); }

  int getChipCount() const override {
    return m_styleManager->getPatternCount() + 1;
  }

  void drawChip(QPainter &p, QRect rect, int index) override;
  void onSelect(int index) override;

  void removeSelectedStylesFromSet(std::vector<int> selection) override;
  void addSelectedStylesToSet(std::vector<int> selection,
                              TFilePath setPath) override;
  void addSelectedStylesToPalette(std::vector<int> selection) override;
  void changeStyleSetFolder(TFilePath newPath) override {
    TStyleManager::instance()->changeStyleSetFolder(m_styleManager, newPath);
    m_stylesFolder = newPath;
  };

  bool isLoading() override { return m_styleManager->isLoading(); }
};

//-----------------------------------------------------------------------------

bool VectorBrushStyleChooserPage::event(QEvent *e) {
  // Intercept tooltip events
  if (e->type() != QEvent::ToolTip) return StyleChooserPage::event(e);

  // see StyleChooserPage::paintEvent
  QHelpEvent *he = static_cast<QHelpEvent *>(e);

  int chipIdx = posToIndex(he->pos()), chipCount = getChipCount();
  if (chipIdx < 0 || chipIdx >= chipCount) return false;

  if (chipIdx > 0) {
    CustomStyleManager::PatternData pattern =
        m_styleManager->getPattern(chipIdx - 1);
    QToolTip::showText(he->globalPos(),
                       QString::fromStdString(pattern.m_patternName));
  } else
    QToolTip::showText(
        he->globalPos(),
        QObject::tr("Plain color", "VectorBrushStyleChooserPage"));

  return true;
}

//-----------------------------------------------------------------------------

void VectorBrushStyleChooserPage::drawChip(QPainter &p, QRect rect, int index) {
  if (index == 0) {
    static QImage noSpecialStyleImage(":Resources/no_vectorbrush.png");
    p.drawImage(rect, noSpecialStyleImage);
  } else {
    assert(0 <= index && index < getChipCount());
    CustomStyleManager::PatternData pattern =
        m_styleManager->getPattern(index - 1);
    p.drawImage(rect, *pattern.m_image);
  }
}

//-----------------------------------------------------------------------------

void VectorBrushStyleChooserPage::onSelect(int index) {
  if (index < 0 || index >= getChipCount()) return;

  if (index > 0) {
    --index;

    CustomStyleManager::PatternData pattern = m_styleManager->getPattern(index);

    if (m_currentIndex < 0) return;

    std::string name = pattern.m_patternName;
    assert(pattern.m_isVector);
    if (!pattern.m_isVector) return;

    TVectorBrushStyle cs(m_stylesFolder, name);
    emit styleSelected(cs);
  } else {
    TSolidColorStyle cs(TPixel32::Black);
    emit styleSelected(cs);
  }
}

//-----------------------------------------------------------------------------

void VectorBrushStyleChooserPage::addSelectedStylesToPalette(
    std::vector<int> selection) {
  if (!selection.size()) return;

  for (int i = 0; i < selection.size(); i++) {
    CustomStyleManager::PatternData pattern =
        m_styleManager->getPattern(selection[i] - 1);

    std::string name = pattern.m_patternName;
    assert(pattern.m_isVector);
    if (!pattern.m_isVector) return;

    TVectorBrushStyle cs(m_stylesFolder, name);
    m_editor->addToPalette(cs);
  }
}

//-----------------------------------------------------------------------------

void VectorBrushStyleChooserPage::removeSelectedStylesFromSet(
    std::vector<int> selection) {
  if (!selection.size()) return;

  bool deleted = false;
  for (int i = 0; i < selection.size(); i++) {
    CustomStyleManager::PatternData pattern =
        m_styleManager->getPattern(selection[i] - 1);
    std::string name = pattern.m_patternName;

    TFilePathSet fileList;

    QDir patternDir(m_stylesFolder.getQString());
    patternDir.setNameFilters(
        QStringList(QString::fromStdString(name) + ".*" +
                    QString::fromStdString(pattern.m_path.getType())));
    TSystem::readDirectory(fileList, patternDir, false);

    deleted |= deleteFilesFromStyleFolder(fileList);
  }

  if (deleted) loadItems();
}

//-----------------------------------------------------------------------------

void VectorBrushStyleChooserPage::addSelectedStylesToSet(
    std::vector<int> selection, TFilePath setPath) {
  if (!selection.size() || setPath.isEmpty()) return;

  bool added = false;
  for (int i = 0; i < selection.size(); i++) {
    CustomStyleManager::PatternData pattern =
        m_styleManager->getPattern(selection[i] - 1);
    std::string name = pattern.m_patternName;

    TFilePathSet fileList;

    QDir patternDir(m_stylesFolder.getQString());
    patternDir.setNameFilters(
        QStringList(QString::fromStdString(name) + ".*" +
                    QString::fromStdString(pattern.m_path.getType())));
    TSystem::readDirectory(fileList, patternDir, false);

    added |= copyFilesToStyleFolder(fileList, setPath);
  }

  if (added) m_editor->setUpdated(setPath);
}

//*****************************************************************************
//    TextureStyleChooser  definition
//*****************************************************************************

class TextureStyleChooserPage final : public StyleChooserPage {
  QString m_filters;
  TextureStyleManager *m_styleManager;

public:
  TextureStyleChooserPage(TFilePath stylesFolder = TFilePath(),
                          QString filters = QString(), QWidget *parent = 0)
      : StyleChooserPage(stylesFolder, parent) {
    setPageType(StylePageType::Texture);
    m_filters      = filters;
    m_styleManager = TStyleManager::instance()->getTextureStyleManager(
        m_stylesFolder, m_filters);
  }

  ~TextureStyleChooserPage() {
    TStyleManager::instance()->removeTextureStyleFolder(m_stylesFolder);
  }

  void setFavorite(bool favorite) override {
    m_favorite = favorite;
    if (favorite && m_styleManager)
      connect(m_styleManager, SIGNAL(itemsUpdated()), this,
              SLOT(onUpdateFavorite()));
  }

  bool event(QEvent *e) override;

  void showEvent(QShowEvent *) override {
    connect(m_styleManager, SIGNAL(itemsUpdated()), this, SLOT(computeSize()));
    m_styleManager->loadItems();
  }
  void hideEvent(QHideEvent *) override {
    disconnect(m_styleManager, SIGNAL(itemsUpdated()), this,
               SLOT(computeSize()));
  }
  bool loadIfNeeded() override { return false; }
  void loadItems() override { m_styleManager->loadItems(); }

  int getChipCount() const override {
    return m_styleManager->getTextureCount() + 1;
  }

  void drawChip(QPainter &p, QRect rect, int index) override {
    if (index == 0) {
      static QImage noSpecialStyleImage(":Resources/no_vectorbrush.png");
      p.drawImage(rect, noSpecialStyleImage);
    } else {
      index -= 1;
      assert(0 <= index && index < getChipCount());
      TextureStyleManager::TextureData texture =
          m_styleManager->getTexture(index);
      if (texture.m_raster && !texture.m_raster->isEmpty())
        p.drawImage(rect, rasterToQImage(texture.m_raster));
    }
  }

  void onSelect(int index) override;

  void removeSelectedStylesFromSet(std::vector<int> selection) override;
  void addSelectedStylesToSet(std::vector<int> selection,
                              TFilePath setPath) override;
  void updateFavorite() override { emit refreshFavorites(); };
  void addSelectedStylesToPalette(std::vector<int> selection) override;
  void changeStyleSetFolder(TFilePath newPath) override {
    TStyleManager::instance()->changeStyleSetFolder(m_styleManager, newPath);
    m_stylesFolder = newPath;
  };
};

//-----------------------------------------------------------------------------

void TextureStyleChooserPage::onSelect(int index) {
  if (index < 0 || index >= getChipCount()) return;

  if (index == 0) {
    TSolidColorStyle cs(TPixel32::Black);
    emit styleSelected(cs);
  } else {
    index--;

    TextureStyleManager::TextureData texture =
        m_styleManager->getTexture(index);

    if (m_currentIndex < 0) return;

    TTextureStyle style(
        texture.m_raster,
        m_stylesFolder + TFilePath(texture.m_path.getLevelName()));

    if (texture.m_path == TFilePath()) style.setIsCustom(true);

    emit styleSelected(style);

    // If selecting Custom Style, switch to Settings page automatically
    if (style.isCustom()) emit customStyleSelected();
  }
}

//-----------------------------------------------------------------------------

void TextureStyleChooserPage::addSelectedStylesToPalette(
    std::vector<int> selection) {
  if (!selection.size()) return;

  for (int i = 0; i < selection.size(); i++) {
    TextureStyleManager::TextureData texture =
        m_styleManager->getTexture(selection[i] - 1);

    TTextureStyle style(
        texture.m_raster,
        m_stylesFolder + TFilePath(texture.m_path.getLevelName()));

    if (texture.m_path == TFilePath()) style.setIsCustom(true);

    m_editor->addToPalette(style);

    // If selecting Custom Style, only, switch to Settings page automatically
    if (style.isCustom())
      emit customStyleSelected();
  }
}

//-----------------------------------------------------------------------------

bool TextureStyleChooserPage::event(QEvent *e) {
  if (e->type() != QEvent::ToolTip) return StyleChooserPage::event(e);

  QHelpEvent *helpEvent = dynamic_cast<QHelpEvent *>(e);
  QString toolTip;
  QPoint pos = helpEvent->pos();

  int chipIdx   = posToIndex(pos);
  int chipCount = m_styleManager->getTextureCount();
  if (chipIdx == 0) {
    QToolTip::showText(helpEvent->globalPos(),
                       QObject::tr("Plain color", "TextureStyleChooserPage"));
  } else {
    chipIdx--;
    if (chipIdx < 0 || chipIdx >= chipCount) return false;

    TextureStyleManager::TextureData texture =
        m_styleManager->getTexture(chipIdx);
    toolTip = QString::fromStdString(texture.m_textureName);
    QToolTip::showText(
        helpEvent->globalPos(),
        toolTip != QString()
            ? toolTip
            : QObject::tr("Custom Texture", "TextureStyleChooserPage"));
  }
  return true;
}

//-----------------------------------------------------------------------------

void TextureStyleChooserPage::removeSelectedStylesFromSet(
    std::vector<int> selection) {
  if (!selection.size()) return;

  bool deleted = false;
  for (int i = 0; i < selection.size(); i++) {
    TFilePathSet fileList;
    TextureStyleManager::TextureData texture =
        m_styleManager->getTexture(selection[i] - 1);
    if (texture.m_path == TFilePath()) continue;
    fileList.push_back(texture.m_path);

    deleted |= deleteFilesFromStyleFolder(fileList);
  }

  if (deleted) loadItems();
}

//-----------------------------------------------------------------------------

void TextureStyleChooserPage::addSelectedStylesToSet(std::vector<int> selection,
                                                     TFilePath setPath) {
  if (!selection.size() || setPath.isEmpty()) return;

  bool added = false;
  for (int i = 0; i < selection.size(); i++) {
    TFilePathSet fileList;
    TextureStyleManager::TextureData texture =
        m_styleManager->getTexture(selection[i] - 1);
    if (texture.m_path == TFilePath()) continue;
    fileList.push_back(texture.m_path);

    added |= copyFilesToStyleFolder(fileList, setPath);
  }

  if (added) m_editor->setUpdated(setPath);
}

//*****************************************************************************
//    MyPaintBrushStyleChooserPage definition
//*****************************************************************************

class MyPaintBrushStyleChooserPage final : public StyleChooserPage {
  QString m_filters;
  BrushStyleManager *m_styleManager;

public:
  MyPaintBrushStyleChooserPage(TFilePath stylesFolder = TFilePath(),
                               QString filters = QString(), QWidget *parent = 0)
      : StyleChooserPage(stylesFolder, parent) {
    setPageType(StylePageType::Raster);
    m_filters      = filters;
    m_chipSize     = QSize(64, 64);
    m_styleManager = TStyleManager::instance()->getBrushStyleManager(
        m_stylesFolder, m_filters, m_chipSize);
  }

  ~MyPaintBrushStyleChooserPage() {
    TStyleManager::instance()->removeBrushStyleFolder(m_stylesFolder);
  }

  void setFavorite(bool favorite) override {
    m_favorite = favorite;
    if (favorite && m_styleManager)
      connect(m_styleManager, SIGNAL(itemsUpdated()), this,
              SLOT(onUpdateFavorite()));
  }

  bool event(QEvent *e) override;

  void showEvent(QShowEvent *) override {
    connect(m_styleManager, SIGNAL(itemsUpdated()), this, SLOT(computeSize()));
    m_styleManager->loadItems();
  }
  void hideEvent(QHideEvent *) override {
    disconnect(m_styleManager, SIGNAL(itemsUpdated()), this,
               SLOT(computeSize()));
  }
  bool loadIfNeeded() override { return false; }
  void loadItems() override { m_styleManager->loadItems(); }

  int getChipCount() const override {
    return m_styleManager->getBrushCount() + 1;
  }

  void drawChip(QPainter &p, QRect rect, int index) override {
    if (index == 0) {
      static QImage noStyleImage(":Resources/no_mypaintbrush.png");
      p.drawImage(rect, noStyleImage);
    } else {
      index -= 1;
      assert(0 <= index && index < getChipCount());
      BrushStyleManager::BrushData brush = m_styleManager->getBrush(index);
      p.drawImage(rect, rasterToQImage(brush.m_brush.getPreview()));
    }
  }

  void onSelect(int index) override;

  void removeSelectedStylesFromSet(std::vector<int> selection) override;
  void addSelectedStylesToSet(std::vector<int> selection,
                              TFilePath setPath) override;
  void updateFavorite() override { emit refreshFavorites(); };
  void addSelectedStylesToPalette(std::vector<int> selection) override;
  void changeStyleSetFolder(TFilePath newPath) override {
    TStyleManager::instance()->changeStyleSetFolder(m_styleManager, newPath);
    m_stylesFolder = newPath;
  };
};

//-----------------------------------------------------------------------------

void MyPaintBrushStyleChooserPage::onSelect(int index) {
  if (index < 0 || index >= getChipCount()) return;

  if (index == 0) {
    TSolidColorStyle noStyle(TPixel32::Black);
    emit styleSelected(noStyle);
  } else {
    index--;

    BrushStyleManager::BrushData brush = m_styleManager->getBrush(index);

    if (m_currentIndex < 0) return;

    emit styleSelected(brush.m_brush);
  }
}

//-----------------------------------------------------------------------------

void MyPaintBrushStyleChooserPage::addSelectedStylesToPalette(
    std::vector<int> selection) {
  if (!selection.size()) return;

  for (int i = 0; i < selection.size(); i++) {
    BrushStyleManager::BrushData brush =
        m_styleManager->getBrush(selection[i] - 1);

    m_editor->addToPalette(brush.m_brush);
  }
}

//-----------------------------------------------------------------------------

bool MyPaintBrushStyleChooserPage::event(QEvent *e) {
  if (e->type() != QEvent::ToolTip) return StyleChooserPage::event(e);

  static TSolidColorStyle noStyle(TPixel32::Black);

  QHelpEvent *helpEvent = dynamic_cast<QHelpEvent *>(e);
  QString toolTip;
  QPoint pos = helpEvent->pos();

  int chipIdx   = posToIndex(pos);
  int chipCount = m_styleManager->getBrushCount();
  if (chipIdx == 0) {
    QToolTip::showText(
        helpEvent->globalPos(),
        QObject::tr("Plain color", "MyPaintBrushStyleChooserPage"));
  } else {
    chipIdx--;
    if (chipIdx < 0 || chipIdx >= chipCount) return false;

    BrushStyleManager::BrushData brush = m_styleManager->getBrush(chipIdx);
    toolTip = QString::fromStdString(brush.m_brushName);
    QToolTip::showText(helpEvent->globalPos(), toolTip);
  }
  return true;
}

//-----------------------------------------------------------------------------

void MyPaintBrushStyleChooserPage::removeSelectedStylesFromSet(
    std::vector<int> selection) {
  if (!selection.size()) return;

  bool deleted = false;
  for (int i = 0; i < selection.size(); i++) {
    TFilePathSet fileList;
    BrushStyleManager::BrushData brush =
        m_styleManager->getBrush(selection[i] - 1);
    std::string name = brush.m_path.getName();
    fileList.push_back(brush.m_path);
    fileList.push_back(brush.m_path.withName(name + "_prev").withType("png"));

    deleted |= deleteFilesFromStyleFolder(fileList);
  }

  if (deleted) loadItems();
}

//-----------------------------------------------------------------------------

void MyPaintBrushStyleChooserPage::addSelectedStylesToSet(
    std::vector<int> selection, TFilePath setPath) {
  if (!selection.size() || setPath.isEmpty()) return;

  bool added = false;
  for (int i = 0; i < selection.size(); i++) {
    TFilePathSet fileList;
    BrushStyleManager::BrushData brush =
        m_styleManager->getBrush(selection[i] - 1);
    std::string name = brush.m_path.getName();
    fileList.push_back(brush.m_path);
    fileList.push_back(brush.m_path.withName(name + "_prev").withType("png"));

    added |= copyFilesToStyleFolder(fileList, setPath);
  }

  if (added) m_editor->setUpdated(setPath);
}

//*****************************************************************************
//    SpecialStyleChooser  definition
//*****************************************************************************

class SpecialStyleChooserPage final : public StyleChooserPage {
  static std::vector<std::pair<int, QImage *>> m_customStyles;
  static bool m_loaded;
  static QString m_filters;

public:
  SpecialStyleChooserPage(TFilePath stylesFolder = TFilePath(),
                          QString filters = QString(), QWidget *parent = 0)
      : StyleChooserPage(stylesFolder, parent) {
    setPageType(StylePageType::VectorGenerated);
    m_filters = filters;
  }

  ~SpecialStyleChooserPage() {
    TStyleManager::instance()->removeCustomStyleFolder(m_stylesFolder);
  }

  bool loadIfNeeded() override {
    if (!m_loaded) {
      loadItems();
      m_loaded = true;
      return true;
    } else
      return false;
  }
  int getChipCount() const override { return m_customStyles.size(); }

  void loadItems() override;

  void drawChip(QPainter &p, QRect rect, int index) override;
  void onSelect(int index) override;
  bool event(QEvent *e) override;

  void addSelectedStylesToSet(std::vector<int> selection,
                              TFilePath setPath) override;
  void addSelectedStylesToPalette(std::vector<int> selection) override;
};

//-----------------------------------------------------------------------------

QString SpecialStyleChooserPage::m_filters;

//-----------------------------------------------------------------------------

std::vector<std::pair<int, QImage *>> SpecialStyleChooserPage::m_customStyles;
bool SpecialStyleChooserPage::m_loaded(false);

//-----------------------------------------------------------------------------

void SpecialStyleChooserPage::loadItems() {
  std::vector<int> tags;
  TColorStyle::getAllTags(tags);

  int chipCount = 0;

  for (int j = 0; j < (int)tags.size(); j++) {
    int tagId = tags[j];
    if (tagId == 3 ||     // solid color
        tagId == 4 ||     // texture
        tagId == 100 ||   // obsolete imagepattern id
        tagId == 2000 ||  // imagepattern
        tagId == 2800 ||  // imagepattern
        tagId == 2001 ||  // cleanup
        tagId == 2002 ||  // black cleanup
        tagId == 3000 ||  // vector brush
        tagId == 4001     // mypaint brush
        )
      continue;

    TColorStyle *style = TColorStyle::create(tagId);
    if (style->isRasterStyle()) {
      delete style;
      continue;
    }
    TDimension chipSize(getChipSize().width(), getChipSize().height());
    QImage *image = new QImage(rasterToQImage(style->getIcon(chipSize), false));
    m_customStyles.push_back(std::make_pair(tagId, image));
    delete style;
  }
}

//-----------------------------------------------------------------------------

void SpecialStyleChooserPage::drawChip(QPainter &p, QRect rect, int index) {
  if (index == 0) {
    static QImage noSpecialStyleImage(":Resources/no_specialstyle.png");
    p.drawImage(rect, noSpecialStyleImage);
  } else {
    int j = index - 1;
    if (0 <= j && j < (int)m_customStyles.size())
      p.drawImage(rect, *m_customStyles[j].second);
    else
      p.fillRect(rect, QBrush(QColor(255, 0, 0)));
  }
}

//-----------------------------------------------------------------------------

void SpecialStyleChooserPage::onSelect(int index) {
  assert(0 <= index && index < (int)m_customStyles.size());
  TColorStyle *cs = 0;
  if (m_currentIndex < 0) return;
  if (index == 0)
    cs = new TSolidColorStyle(TPixel32::Black);
  else {
    int j = index - 1;
    assert(0 <= j && j < (int)m_customStyles.size());
    int tagId = m_customStyles[j].first;
    cs        = TColorStyle::create(tagId);
  }
  emit styleSelected(*cs);
}

//-----------------------------------------------------------------------------

void SpecialStyleChooserPage::addSelectedStylesToPalette(
    std::vector<int> selection) {
  if (!selection.size()) return;

  for (int i = 0; i < selection.size(); i++) {
    int j           = selection[i] - 1;
    int tagId       = m_customStyles[j].first;
    TColorStyle *cs = TColorStyle::create(tagId);
    m_editor->addToPalette(*cs);
  }
}

//-----------------------------------------------------------------------------

bool SpecialStyleChooserPage::event(QEvent *e) {
  if (e->type() == QEvent::ToolTip) {
    QHelpEvent *helpEvent = dynamic_cast<QHelpEvent *>(e);
    QString toolTip;
    QPoint pos = helpEvent->pos();
    int index  = posToIndex(pos);
    if (index == 0)
      toolTip = QObject::tr("Plain color", "SpecialStyleChooserPage");
    else {
      int j = index - 1;
      if (0 <= j && j < (int)m_customStyles.size()) {
        int tagId       = m_customStyles[j].first;
        TColorStyle *cs = TColorStyle::create(tagId);
        if (cs) {
          toolTip = cs->getDescription();
          delete cs;
        }
      }
    }
    if (toolTip != "")
      QToolTip::showText(helpEvent->globalPos(), toolTip);
    else
      QToolTip::hideText();
    e->accept();
  }
  return StyleChooserPage::event(e);
}

//-----------------------------------------------------------------------------

void SpecialStyleChooserPage::addSelectedStylesToSet(std::vector<int> selection,
                                                     TFilePath setPath) {
  if (!selection.size() || setPath.isEmpty()) return;

  bool added = false;
  for (int i = 0; i < selection.size(); i++) {
    int j           = selection[i] - 1;
    int tagId       = m_customStyles[j].first;
    TColorStyle *cs = TColorStyle::create(tagId);

    std::string name = std::to_string(tagId);

    TFilePath genFile =
        setPath +
        TFilePath(cs->getDescription() + "-" + QString::fromStdString(name))
            .withType("gen");

    try {
      if (!TFileStatus(setPath).doesExist()) TSystem::mkDir(setPath);
    } catch (TSystemException se) {
      DVGui::warning(QString::fromStdWString(se.getMessage()));
      return;
    }

    try {
      TSystem::touchFile(genFile);
      added = true;
    } catch (...) {
      continue;
    }
  }

  if (added) m_editor->setUpdated(setPath);
}

//=============================================================================
// SettingBox
//-----------------------------------------------------------------------------
/*
SettingBox::SettingBox(QWidget *parent, int index)
: QWidget(parent)
, m_index(index)
, m_style(0)
{
        QHBoxLayout* hLayout = new QHBoxLayout(this);
        hLayout->setSpacing(5);
        hLayout->setMargin(0);
        hLayout->addSpacing(10);
        m_name = new QLabel(this);
        m_name->setFixedSize(82,20);
        m_name->setStyleSheet("border: 0px;");
        hLayout->addWidget(m_name,0);
        m_doubleField = new DoubleField(this, true);
        hLayout->addWidget(m_doubleField,1);
        hLayout->addSpacing(10);
        bool ret = connect(m_doubleField, SIGNAL(valueChanged(bool)), this,
SLOT(onValueChanged(bool)));
  assert(ret);
        setLayout(hLayout);
        setFixedHeight(22);
}

//-----------------------------------------------------------------------------

void SettingBox::setParameters(TColorStyle* cs)
{
        if(!cs)
        {
                m_style = cs;
                return;
        }
        if(cs->getParamCount() == 0 || m_index<0 ||
cs->getParamCount()<=m_index)
                return;
        QString paramName = cs->getParamNames(m_index);
        m_name->setText(paramName);
   double newValue = cs->getParamValue(TColorStyle::double_tag(), m_index);
        double value = m_doubleField->getValue();
        m_style = cs;
        if(value != newValue)
        {
                double min=0, max=1;
                cs->getParamRange(m_index,min,max);
                m_doubleField->setValues(newValue, min, max);
        }
        TCleanupStyle* cleanupStyle = dynamic_cast<TCleanupStyle*>(cs);
        if(paramName == "Contrast" && cleanupStyle)
        {
                if(!cleanupStyle->isContrastEnabled())
                        m_doubleField->setEnabled(false);
                else
                        m_doubleField->setEnabled(true);
        }
        update();
}

//-----------------------------------------------------------------------------

void SettingBox::setColorStyleParam(double value, bool isDragging)
{
  TColorStyle* style = m_style;
  assert(style && m_index < style->getParamCount());

  double min = 0.0, max = 1.0;
  style->getParamRange(m_index, min, max);

  style->setParamValue(m_index, tcrop(value, min, max));

  emit valueChanged(*style, isDragging);
}

//-----------------------------------------------------------------------------

void SettingBox::onValueChanged(bool isDragging)
{
        if(!m_style || m_style->getParamCount() == 0)
                return;

        double value = m_doubleField->getValue();
        if(isDragging && m_style->getParamValue(TColorStyle::double_tag(),
m_index) == value)
                return;

        setColorStyleParam(value, isDragging);
}
*/

//*****************************************************************************
//    SettingsPage  implementation
//*****************************************************************************

SettingsPage::SettingsPage(QWidget *parent)
    : QScrollArea(parent), m_updating(false) {
  bool ret = true;

  setObjectName("styleEditorPage");  // It is necessary for the styleSheet
  setFrameStyle(QFrame::StyledPanel);

  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setWidgetResizable(true);

  // Build the scrolled widget
  QFrame *paramsContainer = new QFrame(this);
  setWidget(paramsContainer);

  QVBoxLayout *paramsContainerLayout = new QVBoxLayout(this);
  paramsContainerLayout->setMargin(10);
  paramsContainerLayout->setSpacing(10);
  paramsContainer->setLayout(paramsContainerLayout);

  // Add a vertical layout to store the "autofill" checkbox widgets
  m_autoFillCheckBox = new QCheckBox(tr("Autopaint for Lines"), this);
  paramsContainerLayout->addWidget(m_autoFillCheckBox, 0,
                                   Qt::AlignLeft | Qt::AlignVCenter);

  ret = connect(m_autoFillCheckBox, SIGNAL(stateChanged(int)), this,
                SLOT(onAutofillChanged()));
  assert(ret);

  // Prepare the style parameters layout
  m_paramsLayout = new QGridLayout;
  m_paramsLayout->setMargin(0);
  m_paramsLayout->setVerticalSpacing(8);
  m_paramsLayout->setHorizontalSpacing(5);
  paramsContainerLayout->addLayout(m_paramsLayout);

  paramsContainerLayout->addStretch(1);
}

//-----------------------------------------------------------------------------

void SettingsPage::enableAutopaintToggle(bool enabled) {
  m_autoFillCheckBox->setVisible(enabled);
}

//-----------------------------------------------------------------------------

void SettingsPage::setStyle(const TColorStyleP &editedStyle) {
  struct locals {
    inline static void clearLayout(QLayout *layout) {
      QLayoutItem *item;
      while ((item = layout->takeAt(0)) != 0) {
        delete item->layout();
        delete item->spacerItem();
        delete item->widget();
        delete item;
      }
    }
  };  // locals

  // NOTE: Layout reubilds must be avoided whenever possible. In particular, be
  // warned that this
  // function may be invoked when signals emitted from this function are still
  // "flying"...

  bool clearLayout =
      m_editedStyle &&
      !(editedStyle && typeid(*m_editedStyle) == typeid(*editedStyle));
  bool buildLayout =
      editedStyle &&
      !(m_editedStyle && typeid(*m_editedStyle) == typeid(*editedStyle));

  m_editedStyle = editedStyle;

  if (clearLayout) locals::clearLayout(m_paramsLayout);

  if (buildLayout) {
    assert(m_paramsLayout->count() == 0);

    // Assign new settings widgets - one label/editor for each parameter
    bool ret = true;

    int p, pCount = editedStyle->getParamCount();
    for (p = 0; p != pCount; ++p) {
      // Assign label
      QLabel *label = new QLabel(editedStyle->getParamNames(p));
      m_paramsLayout->addWidget(label, p, 0);

      // Assign parameter
      switch (editedStyle->getParamType(p)) {
      case TColorStyle::BOOL: {
        QCheckBox *checkBox = new QCheckBox;
        m_paramsLayout->addWidget(checkBox, p, 1);

        ret = QObject::connect(checkBox, SIGNAL(toggled(bool)), this,
                               SLOT(onValueChanged())) &&
              ret;

        break;
      }

      case TColorStyle::INT: {
        DVGui::IntField *intField = new DVGui::IntField;
        m_paramsLayout->addWidget(intField, p, 1);

        int min, max;
        m_editedStyle->getParamRange(p, min, max);

        intField->setRange(min, max);

        ret = QObject::connect(intField, SIGNAL(valueChanged(bool)), this,
                               SLOT(onValueChanged(bool))) &&
              ret;

        break;
      }

      case TColorStyle::ENUM: {
        QComboBox *comboBox = new QComboBox;
        m_paramsLayout->addWidget(comboBox, p, 1);

        QStringList items;
        m_editedStyle->getParamRange(p, items);

        comboBox->addItems(items);

        ret = QObject::connect(comboBox, SIGNAL(currentIndexChanged(int)), this,
                               SLOT(onValueChanged())) &&
              ret;

        break;
      }

      case TColorStyle::DOUBLE: {
        DVGui::DoubleField *doubleField = new DVGui::DoubleField;
        m_paramsLayout->addWidget(doubleField, p, 1);

        double min, max;
        m_editedStyle->getParamRange(p, min, max);

        doubleField->setRange(min, max);

        ret = QObject::connect(doubleField, SIGNAL(valueChanged(bool)), this,
                               SLOT(onValueChanged(bool))) &&
              ret;

        break;
      }

      case TColorStyle::FILEPATH: {
        DVGui::FileField *fileField = new DVGui::FileField;
        m_paramsLayout->addWidget(fileField, p, 1);

        QStringList extensions;
        m_editedStyle->getParamRange(p, extensions);

        fileField->setFileMode(QFileDialog::AnyFile);
        fileField->setFilters(extensions);

        fileField->setPath(QString::fromStdWString(
            editedStyle->getParamValue(TColorStyle::TFilePath_tag(), p)
                .getWideString()));

        if (!m_editedStyle->isCustom()) {
          label->hide();
          fileField->hide();
        }

        ret = QObject::connect(fileField, SIGNAL(pathChanged()), this,
                               SLOT(onValueChanged())) &&
              ret;

        break;
      }
      }

      // "reset to default" button
      if (m_editedStyle->hasParamDefault(p)) {
        QPushButton *pushButton = new QPushButton;
        pushButton->setToolTip(tr("Reset to default"));
        pushButton->setIcon(createQIcon("delete"));
        pushButton->setFixedSize(24, 24);
        m_paramsLayout->addWidget(pushButton, p, 2);
        ret = QObject::connect(pushButton, SIGNAL(clicked(bool)), this,
                               SLOT(onValueReset())) &&
              ret;
      }

      assert(ret);
    }
  }

  updateValues();
}

//-----------------------------------------------------------------------------

void SettingsPage::updateValues() {
  if (!m_editedStyle) return;

  struct Updating {
    SettingsPage *m_this;  // Prevent 'param changed' signals from being
    ~Updating() {
      m_this->m_updating = false;
    }  // sent when updating editor widgets - this is
  } updating = {(m_updating = true, this)};  // just a view REFRESH function.

  // Deal with the autofill
  m_autoFillCheckBox->setChecked(m_editedStyle->getFlags() & 1);

  int p, pCount = m_editedStyle->getParamCount();
  for (p = 0; p != pCount; ++p) {
    // Update state of "reset to default" button
    if (m_editedStyle->hasParamDefault(p)) {
      QPushButton *pushButton = static_cast<QPushButton *>(
          m_paramsLayout->itemAtPosition(p, 2)->widget());
      pushButton->setEnabled(m_editedStyle->isParamDefault(p));
    }

    // Update editor values
    switch (m_editedStyle->getParamType(p)) {
    case TColorStyle::BOOL: {
      QCheckBox *checkBox = static_cast<QCheckBox *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      checkBox->setChecked(
          m_editedStyle->getParamValue(TColorStyle::bool_tag(), p));

      break;
    }

    case TColorStyle::INT: {
      DVGui::IntField *intField = static_cast<DVGui::IntField *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      intField->setValue(
          m_editedStyle->getParamValue(TColorStyle::int_tag(), p));

      break;
    }

    case TColorStyle::ENUM: {
      QComboBox *comboBox = static_cast<QComboBox *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      comboBox->setCurrentIndex(
          m_editedStyle->getParamValue(TColorStyle::int_tag(), p));

      break;
    }

    case TColorStyle::DOUBLE: {
      DVGui::DoubleField *doubleField = static_cast<DVGui::DoubleField *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      doubleField->setValue(
          m_editedStyle->getParamValue(TColorStyle::double_tag(), p));

      break;
    }

    case TColorStyle::FILEPATH: {
      DVGui::FileField *fileField = static_cast<DVGui::FileField *>(
          m_paramsLayout->itemAtPosition(p, 1)->widget());

      fileField->setPath(QString::fromStdWString(
          m_editedStyle->getParamValue(TColorStyle::TFilePath_tag(), p)
              .getWideString()));

      if (!m_editedStyle->isCustom()) {
        m_paramsLayout->itemAtPosition(p, 0)->widget()->hide();
        fileField->hide();
      } else {
        m_paramsLayout->itemAtPosition(p, 0)->widget()->show();
        fileField->show();
      }

      break;
    }
    }
  }
}

//-----------------------------------------------------------------------------

void SettingsPage::onAutofillChanged() {
  m_editedStyle->setFlags((unsigned int)(m_autoFillCheckBox->isChecked()));

  if (!m_updating)
    emit paramStyleChanged(false);  // Forward the signal to the style editor
}

//-----------------------------------------------------------------------------

int SettingsPage::getParamIndex(const QWidget *widget) {
  int p, pCount = m_paramsLayout->rowCount();
  for (p = 0; p != pCount; ++p)
    for (int c = 0; c < 3; ++c)
      if (QLayoutItem *item = m_paramsLayout->itemAtPosition(p, c))
        if (item->widget() == widget) return p;
  return -1;
}

//-----------------------------------------------------------------------------

void SettingsPage::onValueReset() {
  assert(m_editedStyle);

  // Extract the parameter index
  QWidget *senderWidget = static_cast<QWidget *>(sender());
  int p                 = getParamIndex(senderWidget);

  assert(0 <= p && p < m_editedStyle->getParamCount());
  m_editedStyle->setParamDefault(p);

  // Forward the signal to the style editor
  if (!m_updating) emit paramStyleChanged(false);
}

//-----------------------------------------------------------------------------

void SettingsPage::onValueChanged(bool isDragging) {
  assert(m_editedStyle);

  // Extract the parameter index
  QWidget *senderWidget = static_cast<QWidget *>(sender());
  int p                 = getParamIndex(senderWidget);

  assert(0 <= p && p < m_editedStyle->getParamCount());

  // Update the style's parameter value
  switch (m_editedStyle->getParamType(p)) {
  case TColorStyle::BOOL:
    m_editedStyle->setParamValue(
        p, static_cast<QCheckBox *>(senderWidget)->isChecked());
    break;
  case TColorStyle::INT:
    m_editedStyle->setParamValue(
        p, static_cast<DVGui::IntField *>(senderWidget)->getValue());
    break;
  case TColorStyle::ENUM:
    m_editedStyle->setParamValue(
        p, static_cast<QComboBox *>(senderWidget)->currentIndex());
    break;
  case TColorStyle::DOUBLE:
    m_editedStyle->setParamValue(
        p, static_cast<DVGui::DoubleField *>(senderWidget)->getValue());
    break;
  case TColorStyle::FILEPATH: {
    const QString &string =
        static_cast<DVGui::FileField *>(senderWidget)->getPath();
    m_editedStyle->setParamValue(p, TFilePath(string.toStdWString()));
    break;
  }
  }

  // Forward the signal to the style editor
  if (!m_updating) emit paramStyleChanged(isDragging);
}

//=============================================================================

namespace {

QScrollArea *makeChooserPage(QWidget *chooser) {
  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(chooser);
  return scrollArea;
}

//-----------------------------------------------------------------------------

QScrollArea *makeChooserPageWithoutScrollBar(QWidget *chooser) {
  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(chooser);
  return scrollArea;
}

}  // namespace

//********************************************************************************
//    TStyleEditorHandler declaration
//********************************************************************************

// singleton
class TStyleEditorHandler {
  std::vector<StyleEditor *> m_styleEditors;

  TStyleEditorHandler() {}

public:
  static TStyleEditorHandler *instance() {
    static TStyleEditorHandler theInstance;
    return &theInstance;
  }

  ~TStyleEditorHandler() {}

  void addEditor(StyleEditor *styleEditor) {
    m_styleEditors.push_back(styleEditor);
  }
  void removeEditor(StyleEditor *styleEditor) {
    std::vector<StyleEditor *>::iterator it =
        std::find(m_styleEditors.begin(), m_styleEditors.end(), styleEditor);
    if (it != m_styleEditors.end()) m_styleEditors.erase(it);
  }
  void updateEditorPage(int pageIndex, StyleEditor *emitter) {
    for (int i = 0; i < m_styleEditors.size(); i++)
      if (!emitter || m_styleEditors[i] != emitter)
        m_styleEditors[i]->updatePage(pageIndex);
  }
};

//*****************************************************************************
//    StyleEditor  implementation
//*****************************************************************************

StyleEditor::StyleEditor(PaletteController *paletteController, QWidget *parent)
    : QWidget(parent)
    , m_paletteController(paletteController)
    , m_paletteHandle(paletteController->getCurrentPalette())
    , m_cleanupPaletteHandle(paletteController->getCurrentCleanupPalette())
    , m_toolBar(0)
    , m_enabled(false)
    , m_enabledOnlyFirstTab(false)
    , m_enabledFirstAndLastTab(false)
    , m_oldStyle(0)
    , m_parent(parent)
    , m_editedStyle(0) {
  // TOGLIERE
  TFilePath libraryPath     = ToonzFolder::getLibraryFolder();
  TFilePath myFavoritesPath = ToonzFolder::getMyFavoritesFolder() + "library";

  m_renameStyleSet = new RenameStyleSet(this);
  m_renameStyleSet->hide();

  m_styleBar = new DVGui::TabBar(this);
  m_styleBar->setDrawBase(false);
  m_styleBar->setObjectName("StyleEditorTabBar");

  // This widget is used to set the background color of the tabBar
  // using the styleSheet.
  // It is also used to take 6px on the left before the tabBar
  // and to draw the two lines on the bottom size
  m_tabBarContainer        = new TabBarContainter(this);
  m_colorParameterSelector = new ColorParameterSelector(this);

  m_plainColorPage = new PlainColorPage(0);
  m_settingsPage   = new SettingsPage(0);

  QWidget *emptyPage = new StyleEditorPage(0);

  initializeStyleMenus();

  // Create Style Pages
  // Load Favorites first so they appear first in all lists
  createStylePage(StylePageType::Texture,
                  myFavoritesPath + TFilePath("textures"),
                  getStylePageFilter(StylePageType::Texture), true);
  createStylePage(StylePageType::VectorCustom,
                  myFavoritesPath + TFilePath("vector styles"),
                  getStylePageFilter(StylePageType::VectorCustom), true);
  createStylePage(StylePageType::Raster,
                  myFavoritesPath + TFilePath("raster styles"),
                  getStylePageFilter(StylePageType::Raster), true);
  // Load library pages
  createStylePage(StylePageType::Texture, libraryPath + TFilePath("textures"),
                  getStylePageFilter(StylePageType::Texture));
  createStylePage(StylePageType::VectorGenerated, TFilePath());
  createStylePage(StylePageType::VectorCustom,
                  libraryPath + TFilePath("custom styles"),
                  getStylePageFilter(StylePageType::VectorCustom));
  createStylePage(StylePageType::VectorBrush,
                  libraryPath + TFilePath("vector brushes"),
                  getStylePageFilter(StylePageType::VectorBrush));

  TFilePathSet dirs = TMyPaintBrushStyle::getBrushesDirs();
  for (TFilePathSet::iterator i = dirs.begin(); i != dirs.end(); ++i) {
    TFileStatus fs(*i);
    if (fs.doesExist() && fs.isDirectory())
      createStylePage(StylePageType::Raster, *i,
                      getStylePageFilter(StylePageType::Raster));
  }

  // For the plainColorPage and the settingsPage
  // I create a "fake" QScrollArea (without ScrollingBar
  // in order to use the styleSheet to stylish its background
  QScrollArea *plainArea    = makeChooserPageWithoutScrollBar(m_plainColorPage);
  QScrollArea *settingsArea = makeChooserPageWithoutScrollBar(m_settingsPage);
  m_textureOutsideArea = makeChooserPageWithoutScrollBar(createTexturePage());
  m_rasterOutsideArea  = makeChooserPageWithoutScrollBar(createRasterPage());
  m_vectorOutsideArea  = makeChooserPageWithoutScrollBar(createVectorPage());
  m_vectorOutsideArea->setMinimumWidth(50);

  m_styleChooser = new QStackedWidget(this);
  m_styleChooser->addWidget(plainArea);
  m_styleChooser->addWidget(m_textureOutsideArea);
  m_styleChooser->addWidget(m_vectorOutsideArea);
  m_styleChooser->addWidget(m_rasterOutsideArea);
  m_styleChooser->addWidget(settingsArea);
  m_styleChooser->addWidget(makeChooserPageWithoutScrollBar(emptyPage));
  m_styleChooser->setFocusPolicy(Qt::NoFocus);

  QFrame *bottomWidget = createBottomWidget();

  m_toolBar = new QToolBar(this);
  m_toolBar->setMovable(false);
  m_toolBar->setMaximumHeight(22);
  m_toolBar->addWidget(m_colorParameterSelector);

  QMenu *menu   = new QMenu();
  m_wheelAction = new QAction(tr("Wheel"), this);
  m_hsvAction   = new QAction(tr("HSV"), this);
  m_alphaAction = new QAction(tr("Alpha"), this);
  m_rgbAction   = new QAction(tr("RGB"), this);

  m_wheelAction->setCheckable(true);
  m_hsvAction->setCheckable(true);
  m_alphaAction->setCheckable(true);
  m_rgbAction->setCheckable(true);
  m_wheelAction->setChecked(true);
  m_hsvAction->setChecked(false);
  m_alphaAction->setChecked(true);
  m_rgbAction->setChecked(false);
  menu->addAction(m_wheelAction);
  menu->addAction(m_alphaAction);
  menu->addAction(m_hsvAction);
  menu->addAction(m_rgbAction);

  m_hexAction   = new QWidgetAction(this);
  m_hexLineEdit = new HexLineEdit("", this);
  m_hexLineEdit->setObjectName("HexLineEdit");
  m_hexAction->setDefaultWidget(m_hexLineEdit);
  m_hexAction->setText(" ");
  menu->addAction(m_hexAction);
  QFontMetrics fm(QApplication::font());
  m_hexLineEdit->setFixedWidth(75);

  m_plainColorPage->m_hsvFrame->setVisible(false);
  m_plainColorPage->m_rgbFrame->setVisible(false);

  QToolButton *toolButton = new QToolButton(this);
  toolButton->setIcon(createQIcon("menu"));
  toolButton->setFixedSize(22, 22);
  toolButton->setMenu(menu);
  toolButton->setPopupMode(QToolButton::InstantPopup);
  toolButton->setToolTip(tr("Show or hide parts of the Color Page."));

  m_styleSetsButton = new QToolButton(this);
  m_styleSetsButton->setIcon(createQIcon("stylesets"));
  m_styleSetsButton->setFixedSize(22, 22);
  m_styleSetsButton->setPopupMode(QToolButton::InstantPopup);
  m_styleSetsButton->setToolTip(tr("Show or hide style sets."));

  QToolBar *displayToolbar = new QToolBar(this);
  displayToolbar->addWidget(m_styleSetsButton);
  m_toggleOrientationAction =
      displayToolbar->addAction(createQIcon("orientation_h"), "");
  m_toggleOrientationAction->setToolTip(
      tr("Toggle orientation of the Color Page."));
  QWidget *toggleOrientationButton =
      displayToolbar->widgetForAction(m_toggleOrientationAction);
  toggleOrientationButton->setFixedSize(22, 22);
  toggleOrientationButton->setFocusPolicy(Qt::NoFocus);
  displayToolbar->addWidget(toolButton);
  displayToolbar->setMaximumHeight(22);
  displayToolbar->setIconSize(QSize(16, 16));

  /* ------- layout ------- */
  QGridLayout *mainLayout = new QGridLayout;
  mainLayout->setMargin(0);
  mainLayout->setSpacing(0);
  {
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setMargin(0);
    {
      hLayout->addSpacing(0);
      hLayout->addWidget(m_styleBar);
      hLayout->addStretch();
    }
    m_tabBarContainer->setLayout(hLayout);

    mainLayout->addWidget(m_tabBarContainer, 0, 0, 1, 2);
    mainLayout->addWidget(m_styleChooser, 1, 0, 1, 2);
    mainLayout->addWidget(bottomWidget, 2, 0, 1, 2);
    mainLayout->addWidget(m_toolBar, 3, 0);
    mainLayout->addWidget(displayToolbar, 3, 1);
  }
  mainLayout->setColumnStretch(0, 1);
  mainLayout->setRowStretch(1, 1);
  setLayout(mainLayout);

  /* ------- signal-slot connections ------- */

  bool ret = true;
  ret      = ret && connect(m_styleBar, SIGNAL(currentChanged(int)), this,
                       SLOT(setPage(int)));
  ret = ret && connect(m_colorParameterSelector, SIGNAL(colorParamChanged()),
                       this, SLOT(onColorParamChanged()));
  ret = ret && connect(m_settingsPage, SIGNAL(paramStyleChanged(bool)), this,
                       SLOT(onParamStyleChanged(bool)));
  ret = ret && connect(m_plainColorPage,
                       SIGNAL(colorChanged(const ColorModel &, bool)), this,
                       SLOT(onColorChanged(const ColorModel &, bool)));

  ret = ret && connect(m_wheelAction, SIGNAL(toggled(bool)),
                       m_plainColorPage->m_wheelFrame, SLOT(setVisible(bool)));
  ret = ret && connect(m_hsvAction, SIGNAL(toggled(bool)),
                       m_plainColorPage->m_hsvFrame, SLOT(setVisible(bool)));
  ret = ret && connect(m_alphaAction, SIGNAL(toggled(bool)),
                       m_plainColorPage->m_alphaFrame, SLOT(setVisible(bool)));
  ret = ret && connect(m_rgbAction, SIGNAL(toggled(bool)),
                       m_plainColorPage->m_rgbFrame, SLOT(setVisible(bool)));
  ret = ret && connect(m_toggleOrientationAction, SIGNAL(triggered()),
                       m_plainColorPage, SLOT(toggleOrientation()));
  ret = ret && connect(m_toggleOrientationAction, SIGNAL(triggered()), this,
                       SLOT(updateOrientationButton()));
  ret = ret && connect(m_hexLineEdit, SIGNAL(returnPressed()), this,
                       SLOT(onHexChanged()));
  ret = ret && connect(m_hexLineEdit, SIGNAL(textEdited(const QString &)), this,
                       SLOT(onHexEdited(const QString &)));
  ret = ret && connect(menu, SIGNAL(aboutToHide()), this, SLOT(onHideMenu()));
  ret = ret && connect(m_styleChooser, SIGNAL(currentChanged(int)), this,
                       SLOT(onPageChanged(int)));
  assert(ret);
  /* ------- initial conditions ------- */
  enable(false, false, false);
  // set to the empty page
  m_styleChooser->setCurrentIndex(m_styleChooser->count() - 1);

  TStyleEditorHandler::instance()->addEditor(this);
}

//-----------------------------------------------------------------------------

StyleEditor::~StyleEditor() {
  TStyleEditorHandler::instance()->removeEditor(this);
}

//-----------------------------------------------------------------------------
/*
void StyleEditor::setPaletteHandle(TPaletteHandle* paletteHandle)
{
        if(m_paletteHandle != paletteHandle)
                m_paletteHandle = paletteHandle;
        onStyleSwitched();
}
*/
//-----------------------------------------------------------------------------

QFrame *StyleEditor::createBottomWidget() {
  QFrame *bottomWidget = new QFrame(this);
  m_autoButton         = new QPushButton(tr("Auto"));
  m_oldColor           = new DVGui::StyleSample(this, 42, 30);
  m_newColor           = new DVGui::StyleSample(this, 32, 32);
  m_applyButton        = new QPushButton(tr("Apply"));
  m_fillColorWidget    = new QFrame(this);

  bottomWidget->setFrameStyle(QFrame::StyledPanel);
  bottomWidget->setObjectName("bottomWidget");
  bottomWidget->setContentsMargins(0, 0, 0, 0);
  m_applyButton->setToolTip(tr("Apply changes to current style"));
  m_applyButton->setDisabled(m_paletteController->isColorAutoApplyEnabled());
  m_applyButton->setFocusPolicy(Qt::NoFocus);

  m_autoButton->setCheckable(true);
  m_autoButton->setToolTip(tr("Automatically update style changes"));
  m_autoButton->setChecked(m_paletteController->isColorAutoApplyEnabled());
  m_autoButton->setFocusPolicy(Qt::NoFocus);

  m_oldColor->setToolTip(tr("Return To Previous Style"));
  m_oldColor->enableClick(true);
  m_oldColor->setEnable(false);
  m_newColor->setToolTip(tr("Current Style"));
  m_newColor->setEnable(false);
  m_newColor->setFixedWidth(32);

  m_fillColorWidget->setFixedHeight(32);
  m_fillColorWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QHBoxLayout *fillColorLayout = new QHBoxLayout(this);
  fillColorLayout->addWidget(new QLabel(" ", this));
  m_fillColorWidget->setLayout(fillColorLayout);

  /* ------ layout ------ */
  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setMargin(2);
  mainLayout->setSpacing(1);
  {
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setMargin(0);
    hLayout->setSpacing(0);
    {
      hLayout->addWidget(m_autoButton);
      hLayout->addWidget(m_applyButton);
      // hLayout->addSpacing(2);
      hLayout->addWidget(m_newColor, 1);
      hLayout->addWidget(m_oldColor, 0);
      hLayout->addWidget(m_fillColorWidget, 0);
      m_oldColor->hide();
    }
    mainLayout->addLayout(hLayout);
    m_autoButton->hide();
    m_applyButton->hide();

    // QHBoxLayout *buttonsLayout = new QHBoxLayout;
    // buttonsLayout->setMargin(0);
    // buttonsLayout->setSpacing(5);
    //{ buttonsLayout->addWidget(m_applyButton); }
    // mainLayout->addLayout(buttonsLayout);
  }
  bottomWidget->setLayout(mainLayout);

  /* ------ signal-slot connections ------ */
  bool ret = true;
  // ret      = ret && connect(m_applyButton, SIGNAL(clicked()), this,
  //                     SLOT(applyButtonClicked()));
  ret = ret && connect(m_autoButton, SIGNAL(toggled(bool)), this,
                       SLOT(autoCheckChanged(bool)));
  // ret = ret && connect(m_oldColor, SIGNAL(clicked(const TColorStyle &)),
  // this,
  //                     SLOT(onOldStyleClicked(const TColorStyle &)));
  assert(ret);

  return bottomWidget;
}

//-----------------------------------------------------------------------------

QFrame *StyleEditor::createTexturePage() {
  QFrame *textureOutsideFrame = new QFrame(this);
  textureOutsideFrame->setMinimumWidth(50);

  /* ------ layout ------ */
  QVBoxLayout *textureOutsideLayout = new QVBoxLayout();
  textureOutsideLayout->setMargin(0);
  textureOutsideLayout->setSpacing(0);
  textureOutsideLayout->setSizeConstraint(QLayout::SetNoConstraint);
  {
    QVBoxLayout *textureLayout = new QVBoxLayout();
    textureLayout->setMargin(0);
    textureLayout->setSpacing(0);
    textureLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      std::vector<StyleChooserPage *>::iterator itP = m_texturePages.begin();
      std::vector<ClickableLabel *>::iterator itL   = m_textureLabels.begin();
      std::vector<QPushButton *>::iterator itB      = m_textureButtons.begin();
      for (; itP != m_texturePages.end(); itP++, itL++, itB++) {
        QHBoxLayout *setLabelLay = new QHBoxLayout();
        setLabelLay->setMargin(0);
        setLabelLay->setSpacing(3);
        {
          setLabelLay->addWidget(*itB, 0);
          setLabelLay->addWidget(*itL, 0);
          setLabelLay->addStretch(1);
        }
        textureLayout->addLayout(setLabelLay);
        textureLayout->addWidget(*itP);
      }
      textureLayout->addStretch();
    }
    QFrame *textureFrame = new QFrame(this);
    textureFrame->setMinimumWidth(50);
    textureFrame->setLayout(textureLayout);
    m_textureArea = makeChooserPage(textureFrame);
    m_textureArea->setMinimumWidth(50);
    textureOutsideLayout->addWidget(m_textureArea);
  }
  textureOutsideFrame->setLayout(textureOutsideLayout);

  return textureOutsideFrame;
}

//-----------------------------------------------------------------------------

QFrame *StyleEditor::createVectorPage() {
  QFrame *vectorOutsideFrame = new QFrame(this);
  vectorOutsideFrame->setMinimumWidth(50);

  /* ------ layout ------ */
  QVBoxLayout *vectorOutsideLayout = new QVBoxLayout();
  vectorOutsideLayout->setMargin(0);
  vectorOutsideLayout->setSpacing(0);
  vectorOutsideLayout->setSizeConstraint(QLayout::SetNoConstraint);
  {
    QVBoxLayout *vectorLayout = new QVBoxLayout();
    vectorLayout->setMargin(0);
    vectorLayout->setSpacing(0);
    vectorLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      std::vector<StyleChooserPage *>::iterator itP = m_vectorPages.begin();
      std::vector<ClickableLabel *>::iterator itL   = m_vectorLabels.begin();
      std::vector<QPushButton *>::iterator itB      = m_vectorButtons.begin();
      for (; itP != m_vectorPages.end(); itP++, itL++, itB++) {
        QHBoxLayout *setLabelLay = new QHBoxLayout();
        setLabelLay->setMargin(0);
        setLabelLay->setSpacing(3);
        {
          setLabelLay->addWidget(*itB, 0);
          setLabelLay->addWidget(*itL, 0);
          setLabelLay->addStretch(1);
        }
        vectorLayout->addLayout(setLabelLay);
        vectorLayout->addWidget(*itP);
      }
      vectorLayout->addStretch();
    }
    QFrame *vectorFrame = new QFrame(this);
    vectorFrame->setMinimumWidth(50);
    vectorFrame->setLayout(vectorLayout);
    m_vectorArea = makeChooserPage(vectorFrame);
    m_vectorArea->setMinimumWidth(50);
    vectorOutsideLayout->addWidget(m_vectorArea);
  }
  vectorOutsideFrame->setLayout(vectorOutsideLayout);

  return vectorOutsideFrame;
}

//-----------------------------------------------------------------------------

QFrame *StyleEditor::createRasterPage() {
  QFrame *rasterOutsideFrame = new QFrame(this);
  rasterOutsideFrame->setMinimumWidth(50);

  /* ------ layout ------ */
  QVBoxLayout *rasterOutsideLayout = new QVBoxLayout();
  rasterOutsideLayout->setMargin(0);
  rasterOutsideLayout->setSpacing(0);
  rasterOutsideLayout->setSizeConstraint(QLayout::SetNoConstraint);
  {
    QVBoxLayout *rasterLayout = new QVBoxLayout();
    rasterLayout->setMargin(0);
    rasterLayout->setSpacing(0);
    rasterLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      std::vector<StyleChooserPage *>::iterator itP = m_rasterPages.begin();
      std::vector<ClickableLabel *>::iterator itL   = m_rasterLabels.begin();
      std::vector<QPushButton *>::iterator itB      = m_rasterButtons.begin();
      for (; itP != m_rasterPages.end(); itP++, itL++, itB++) {
        QHBoxLayout *setLabelLay = new QHBoxLayout();
        setLabelLay->setMargin(0);
        setLabelLay->setSpacing(3);
        {
          setLabelLay->addWidget(*itB, 0);
          setLabelLay->addWidget(*itL, 0);
          setLabelLay->addStretch(1);
        }
        rasterLayout->addLayout(setLabelLay);
        rasterLayout->addWidget(*itP);
      }
      rasterLayout->addStretch();
    }
    QFrame *rasterFrame = new QFrame(this);
    rasterFrame->setMinimumWidth(50);
    rasterFrame->setLayout(rasterLayout);
    m_rasterArea = makeChooserPage(rasterFrame);
    m_rasterArea->setMinimumWidth(50);
    rasterOutsideLayout->addWidget(m_rasterArea);
  }
  rasterOutsideFrame->setLayout(rasterOutsideLayout);

  return rasterOutsideFrame;
}

//-----------------------------------------------------------------------------

void StyleEditor::updateTabBar() {
  m_styleBar->clearTabBar();
  if (m_enabled && !m_enabledOnlyFirstTab && !m_enabledFirstAndLastTab) {
    m_styleBar->addSimpleTab(tr("Color"));
    m_styleBar->addSimpleTab(tr("Texture"));
    m_styleBar->addSimpleTab(tr("Vector"));
    m_styleBar->addSimpleTab(tr("Raster"));
    m_styleBar->addSimpleTab(tr("Settings"));
  } else if (m_enabled && m_enabledOnlyFirstTab && !m_enabledFirstAndLastTab)
    m_styleBar->addSimpleTab(tr("Color"));
  else if (m_enabled && !m_enabledOnlyFirstTab && m_enabledFirstAndLastTab) {
    m_styleBar->addSimpleTab(tr("Color"));
    m_styleBar->addSimpleTab(tr("Settings"));
  } else {
    m_styleChooser->setCurrentIndex(m_styleChooser->count() - 1);
    return;
  }
  m_tabBarContainer->layout()->update();
  m_styleChooser->setCurrentIndex(0);
}

//-----------------------------------------------------------------------------

void StyleEditor::showEvent(QShowEvent *) {
  m_autoButton->setChecked(m_paletteController->isColorAutoApplyEnabled());
  onStyleSwitched();
  bool ret = true;
  ret      = ret && connect(m_paletteHandle, SIGNAL(colorStyleSwitched()),
                       SLOT(onStyleSwitched()));
  ret = ret && connect(m_paletteHandle, SIGNAL(colorStyleChanged(bool)),
                       SLOT(onStyleChanged(bool)));
  ret = ret && connect(m_paletteHandle, SIGNAL(paletteSwitched()), this,
                       SLOT(onStyleSwitched()));
  ret = ret && connect(m_paletteController, SIGNAL(checkPaletteLock()), this,
                       SLOT(checkPaletteLock()));
  if (m_cleanupPaletteHandle)
    ret =
        ret && connect(m_cleanupPaletteHandle, SIGNAL(colorStyleChanged(bool)),
                       SLOT(onCleanupStyleChanged(bool)));

  ret = ret && connect(m_paletteController, SIGNAL(colorAutoApplyEnabled(bool)),
                       this, SLOT(enableColorAutoApply(bool)));
  ret = ret && connect(m_paletteController,
                       SIGNAL(colorSampleChanged(const TPixel32 &)), this,
                       SLOT(setColorSample(const TPixel32 &)));

  m_plainColorPage->m_wheelFrame->setVisible(m_wheelAction->isChecked());
  m_plainColorPage->m_alphaFrame->setVisible(m_alphaAction->isChecked());
  m_plainColorPage->m_hsvFrame->setVisible(m_hsvAction->isChecked());
  m_plainColorPage->m_rgbFrame->setVisible(m_rgbAction->isChecked());
  updateOrientationButton();
  assert(ret);
}

//-----------------------------------------------------------------------------

void StyleEditor::hideEvent(QHideEvent *) {
  disconnect(m_paletteHandle, 0, this, 0);
  if (m_cleanupPaletteHandle) disconnect(m_cleanupPaletteHandle, 0, this, 0);
  disconnect(m_paletteController, 0, this, 0);
}

//-----------------------------------------------------------------------------

void StyleEditor::keyPressEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Alt:
    m_isAltPressed = true;
    break;
  case Qt::Key_Control:
    m_isCtrlPressed = true;
    break;
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::keyReleaseEvent(QKeyEvent *event) {
  switch (event->key()) {
  case Qt::Key_Alt:
    m_isAltPressed = false;
    break;
  case Qt::Key_Control:
    m_isCtrlPressed = false;
    break;
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::enterEvent(QEvent *event) {
  Qt::KeyboardModifiers modkeys = QGuiApplication::queryKeyboardModifiers();

  m_isAltPressed  = modkeys.testFlag(Qt::AltModifier);
  m_isCtrlPressed = modkeys.testFlag(Qt::ControlModifier);
};

//-----------------------------------------------------------------------------

void StyleEditor::focusInEvent(QFocusEvent *event) {
  Qt::KeyboardModifiers modkeys = QGuiApplication::queryKeyboardModifiers();
  m_isAltPressed                = modkeys.testFlag(Qt::AltModifier);
  m_isCtrlPressed               = modkeys.testFlag(Qt::ControlModifier);
}

//-----------------------------------------------------------------------------

void StyleEditor::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::RightButton) return;

  if (!m_isCtrlPressed) {
    clearSelection();
    update();
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::contextMenuEvent(QContextMenuEvent *event) {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  StyleChooserPage *page;
  if (tab == 1)  // Textures tab
    page = new TextureStyleChooserPage(TFilePath(), QString(), this);
  else if (tab == 2)  // Vector tab
    page = new CustomStyleChooserPage(TFilePath(), QString(), this);
  else if (tab == 3)  // Raster tab
    page = new MyPaintBrushStyleChooserPage(TFilePath(), QString(), this);

  page->processContextMenuEvent(event);
}

//-----------------------------------------------------------------------------

void StyleEditor::clearSelection() {
  std::vector<StyleChooserPage *>::iterator it;
  for (it = m_texturePages.begin(); it != m_texturePages.end(); it++) {
    StyleChooserPage *page = *it;
    page->clearSelection();
    page->update();
  }
  for (it = m_vectorPages.begin(); it != m_vectorPages.end(); it++) {
    StyleChooserPage *page = *it;
    page->clearSelection();
    page->update();
  }
  for (it = m_rasterPages.begin(); it != m_rasterPages.end(); it++) {
    StyleChooserPage *page = *it;
    page->clearSelection();
    page->update();
  }
}

//-----------------------------------------------------------------------------

bool StyleEditor::isSelecting() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return false;

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1)
    pages = &m_texturePages;
  else if (tab == 2)
    pages = &m_vectorPages;
  else if (tab == 3)
    pages = &m_rasterPages;

  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin(); it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    if (page->getSelection().size() > 0) return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

bool StyleEditor::isSelectingFavorites() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return false;

  if (tab == 1)
    return (m_texturePages[0]->getSelection().size() > 0);
  else if (tab == 2)
    return (m_vectorPages[0]->getSelection().size() > 0);
  else if (tab == 3)
    return (m_rasterPages[0]->getSelection().size() > 0);

  return true;
}

//-----------------------------------------------------------------------------

bool StyleEditor::isSelectingFavoritesOnly() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return false;

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1) {
    if (m_texturePages[0]->getSelection().size() == 0) return false;
    pages = &m_texturePages;
  } else if (tab == 2) {
    if (m_vectorPages[0]->getSelection().size() == 0) return false;
    pages = &m_vectorPages;
  } else if (tab == 3) {
    if (m_rasterPages[0]->getSelection().size() == 0) return false;
    pages = &m_rasterPages;
  }

  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin() + 1; it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    if (page->getSelection().size() > 0) return false;
  }

  return true;
}

//-----------------------------------------------------------------------------

bool StyleEditor::isSelectingNonFavoritesOnly() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return false;

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1) {
    if (m_texturePages[0]->getSelection().size() >= 0) return false;
    pages = &m_texturePages;
  } else if (tab == 2) {
    if (m_vectorPages[0]->getSelection().size() >= 0) return false;
    pages = &m_vectorPages;
  } else if (tab == 3) {
    if (m_rasterPages[0]->getSelection().size() >= 0) return false;
    pages = &m_rasterPages;
  }

  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin() + 1; it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    if (page->getSelection().size() > 0) return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

void StyleEditor::updateOrientationButton() {
  if (m_plainColorPage->getIsVertical()) {
    m_toggleOrientationAction->setIcon(createQIcon("orientation_h"));
  } else {
    m_toggleOrientationAction->setIcon(createQIcon("orientation_v"));
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onStyleSwitched() {
  TPalette *palette = getPalette();

  if (!palette) {
    // set the current page to empty
    m_styleChooser->setCurrentIndex(m_styleChooser->count() - 1);
    enable(false);
    m_colorParameterSelector->clear();
    m_oldStyle    = TColorStyleP();
    m_editedStyle = TColorStyleP();

    m_parent->setWindowTitle(tr("No Style Selected"));
    return;
  }

  int styleIndex = getStyleIndex();
  setEditedStyleToStyle(palette->getStyle(styleIndex));

  bool isStyleNull    = setStyle(m_editedStyle.getPointer());
  bool isColorInField = palette->getPaletteName() == L"EmptyColorFieldPalette";
  bool isValidIndex   = styleIndex > 0 || isColorInField;
  bool isCleanUpPalette = palette->isCleanupPalette();

  /* ------ update the status text ------ */
  if (!isStyleNull && isValidIndex) {
    QString statusText;
    // palette type
    if (isCleanUpPalette)
      statusText = tr("Cleanup ");
    else if (palette->getGlobalName() != L"")
      statusText = tr("Studio ");
    else
      statusText = tr("Level ");

    // palette name
    statusText += tr("Palette") + " : " +
                  QString::fromStdWString(palette->getPaletteName());

    // style name
    statusText += QString::fromStdWString(L" | #");
    statusText += QString::number(styleIndex);
    statusText += QString::fromStdWString(L" : " + m_editedStyle->getName());
    TPoint pickedPos = m_editedStyle->getPickedPosition().pos;
    if (pickedPos != TPoint())
      statusText +=
          QString(" (Picked from %1,%2)").arg(pickedPos.x).arg(pickedPos.y);

    m_parent->setWindowTitle(statusText);
  } else {
    m_parent->setWindowTitle(tr("Style Editor - No Valid Style Selected"));
  }
  enable(!isStyleNull && isValidIndex, isColorInField, isCleanUpPalette);
}

//-----------------------------------------------------------------------------

void StyleEditor::onStyleChanged(bool isDragging) {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleIndex = getStyleIndex();
  assert(0 <= styleIndex && styleIndex < palette->getStyleCount());

  setEditedStyleToStyle(palette->getStyle(styleIndex));
  if (!isDragging) {
    setOldStyleToStyle(
        m_editedStyle
            .getPointer());  // This line is needed for proper undo behavior
  }
  m_plainColorPage->setColor(*m_editedStyle, getColorParam());
  m_colorParameterSelector->setStyle(*m_editedStyle);
  m_settingsPage->setStyle(m_editedStyle);
  m_newColor->setStyle(*m_editedStyle);
  int tag = m_editedStyle->getTagId();
  if (tag == 4 || tag == 2000 || tag == 2800 || getStyleIndex() == 0) {
    m_fillColorWidget->hide();
  } else {
    m_fillColorWidget->show();

    // TPixel32 color  = m_editedStyle->getMainColor();
    TPixel32 color   = m_editedStyle->getColorParamValue(getColorParam());
    QString hexColor = color2Hex(color);
    m_hexLineEdit->setText(hexColor);
    m_newColor->setToolTip(hexColor);
    m_fillColorWidget->setToolTip(hexColor);
    QString myColor = QString::number(color.r) + ", " +
                      QString::number(color.g) + ", " +
                      QString::number(color.b);
    std::string myColorStr = myColor.toStdString();
    QString styleSheet     = "QFrame {background-color: rgb(%1);}";
    m_fillColorWidget->setStyleSheet(styleSheet.arg(myColor));
  }
  m_oldColor->setStyle(
      *m_oldStyle);  // This line is needed for proper undo behavior
}

//-----------------------------------------------------------------------

void StyleEditor::onCleanupStyleChanged(bool isDragging) {
  if (!m_cleanupPaletteHandle) return;

  onStyleChanged(isDragging);
}

//-----------------------------------------------------------------------------

void StyleEditor::copyEditedStyleToPalette(bool isDragging) {
  TPalette *palette = getPalette();
  assert(palette);

  int styleIndex = getStyleIndex();
  assert(0 <= styleIndex && styleIndex < palette->getStyleCount());

  if (!(*m_oldStyle == *m_editedStyle) &&
      (!isDragging || m_paletteController->isColorAutoApplyEnabled()) &&
      m_editedStyle->getGlobalName() != L"" &&
      m_editedStyle->getOriginalName() != L"") {
    // If the adited style is linked to the studio palette, then activate the
    // edited flag
    m_editedStyle->setIsEditedFlag(true);
  }

  palette->setStyle(styleIndex,
                    m_editedStyle->clone());  // Must be done *before* setting
                                              // the eventual palette keyframe
  if (!isDragging) {
    if (!(*m_oldStyle == *m_editedStyle)) {
      // do not register undo if the edited color is special one (e.g. changing
      // the ColorField in the fx settings)
      if (palette->getPaletteName() != L"EmptyColorFieldPalette")
        TUndoManager::manager()->add(new UndoPaletteChange(
            m_paletteHandle, styleIndex, *m_oldStyle, *m_editedStyle));
    }

    setOldStyleToStyle(m_editedStyle.getPointer());

    // In case the frame is a keyframe, update it
    if (palette->isKeyframe(styleIndex, palette->getFrame()))  // here
      palette->setKeyframe(styleIndex, palette->getFrame());   //

    palette->setDirtyFlag(true);
  }

  m_paletteHandle->notifyColorStyleChanged(isDragging);
}

//-----------------------------------------------------------------------------

void StyleEditor::onColorChanged(const ColorModel &color, bool isDragging) {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleIndex = getStyleIndex();
  if (styleIndex < 0 || styleIndex > palette->getStyleCount()) return;

  setEditedStyleToStyle(palette->getStyle(styleIndex));  // CLONES the argument

  if (m_editedStyle)  // Should be styleIndex's style at this point
  {
    TPixel tColor = color.getTPixel();

    if (m_editedStyle->hasMainColor()) {
      int index = getColorParam(), count = m_editedStyle->getColorParamCount();

      if (0 <= index && index < count)
        m_editedStyle->setColorParamValue(index, tColor);
      else
        m_editedStyle->setMainColor(tColor);

      m_editedStyle->invalidateIcon();
    } else {
      // The argument has NO (main) COLOR. Since color data is being updated, a
      // 'fake'
      // solid style will be created and operated on.
      TSolidColorStyle *style = new TSolidColorStyle(tColor);
      style->assignNames(m_editedStyle.getPointer());

      setEditedStyleToStyle(style);  // CLONES the argument

      delete style;
    }

    m_newColor->setStyle(*m_editedStyle);
    int tag = m_editedStyle->getTagId();
    if (tag == 4 || tag == 2000 || tag == 2800 || getStyleIndex() == 0) {
      m_fillColorWidget->hide();
    } else {
      m_fillColorWidget->show();
      // TPixel32 color  = m_editedStyle->getMainColor();
      TPixel32 color   = m_editedStyle->getColorParamValue(getColorParam());
      QString hexColor = color2Hex(color);
      m_hexLineEdit->setText(hexColor);
      m_newColor->setToolTip(hexColor);
      m_fillColorWidget->setToolTip(hexColor);
      QString myColor = QString::number(color.r) + ", " +
                        QString::number(color.g) + ", " +
                        QString::number(color.b);
      std::string myColorStr = myColor.toStdString();
      QString styleSheet     = "QFrame {background-color: rgb(%1);}";
      m_fillColorWidget->setStyleSheet(styleSheet.arg(myColor));
    }
    m_colorParameterSelector->setStyle(*m_editedStyle);

    if (m_autoButton->isChecked()) {
      copyEditedStyleToPalette(isDragging);
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::enable(bool enabled, bool enabledOnlyFirstTab,
                         bool enabledFirstAndLastTab) {
  if (m_enabled != enabled || m_enabledOnlyFirstTab != enabledOnlyFirstTab ||
      m_enabledFirstAndLastTab != enabledFirstAndLastTab) {
    m_enabled                = enabled;
    m_enabledOnlyFirstTab    = enabledOnlyFirstTab;
    m_enabledFirstAndLastTab = enabledFirstAndLastTab;
    updateTabBar();
    // m_autoButton->setEnabled(enabled);
    m_applyButton->setDisabled(!enabled || m_autoButton->isChecked());
    m_oldColor->setEnable(enabled);
    m_newColor->setEnable(enabled);

    if (enabled == false) {
      m_oldColor->setColor(TPixel32::Transparent);
      m_newColor->setColor(TPixel32::Transparent);
      m_fillColorWidget->setStyleSheet("background-color: rgba(0, 0, 0, 0);");
      m_fillColorWidget->hide();
    } else
      m_fillColorWidget->show();
  }

  // lock button behavior
  TPalette *palette = getPalette();
  if (palette && enabled) {
    // when the palette is locked
    if (palette->isLocked()) {
      m_applyButton->setEnabled(false);
      m_autoButton->setChecked(false);
      // m_autoButton->setEnabled(false);
    } else  // when the palette is unlocked
    {
      m_applyButton->setDisabled(m_autoButton->isChecked());
      // m_autoButton->setEnabled(true);
      m_autoButton->setChecked(true);
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::checkPaletteLock() {
  if (getPalette() && getPalette()->isLocked()) {
    m_autoButton->setChecked(false);
  } else {
    m_autoButton->setChecked(true);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onOldStyleClicked(const TColorStyle &) {
  if (!m_enabled) return;
  selectStyle(*(m_oldColor->getStyle()));
}

//-----------------------------------------------------------------------------

void StyleEditor::setPage(int index) {
  if (!m_enabledFirstAndLastTab) {
    if (index == 1)
      m_texturePages[0]->loadItems();
    else if (index == 2)
      m_vectorPages[0]->loadItems();
    else if (index == 3)
      m_rasterPages[0]->loadItems();
    m_styleChooser->setCurrentIndex(index);
    return;
  }

  // Se sono nel caso first and last page enable e index == 1 la pagina che
  // voglio settare e' l'ultima!
  if (index == 1)
    index = m_styleChooser->count() -
            2;  // 2 perche' alla fine c'e' una pagina vuota
  m_styleChooser->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void StyleEditor::applyButtonClicked() {
  TPalette *palette = getPalette();
  int styleIndex    = getStyleIndex();
  if (!palette || styleIndex < 0 || styleIndex > palette->getStyleCount())
    return;

  copyEditedStyleToPalette(false);
}

//-----------------------------------------------------------------------------

void StyleEditor::autoCheckChanged(bool value) {
  m_paletteController->enableColorAutoApply(value);

  if (!m_enabled) return;

  m_applyButton->setDisabled(value);
}

//-----------------------------------------------------------------------------

void StyleEditor::enableColorAutoApply(bool enabled) {
  if (m_autoButton->isChecked() != enabled) {
    m_autoButton->setChecked(enabled);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::setColorSample(const TPixel32 &color) {
  // m_colorParameterSelector->setColor(*style);
  ColorModel cm;
  cm.setTPixel(color);
  onColorChanged(cm, true);
}

//-----------------------------------------------------------------------------

bool StyleEditor::setStyle(TColorStyle *currentStyle) {
  assert(currentStyle);

  bool isStyleNull = false;

  QString gname = QString::fromStdWString(currentStyle->getGlobalName());
  // if(!gname.isEmpty() && gname == "ColorFieldSimpleColor")
  //	isStyleNull = true;
  // else
  if (!gname.isEmpty() && gname[0] != L'-') {
    currentStyle = 0;
    isStyleNull  = true;
  }

  if (currentStyle) {
    m_colorParameterSelector->setStyle(*currentStyle);
    m_plainColorPage->setColor(*currentStyle, getColorParam());
    m_oldColor->setStyle(*currentStyle);
    m_newColor->setStyle(*currentStyle);

    int tag = currentStyle->getTagId();
    if (tag == 4 || tag == 2000 || tag == 2800 || getStyleIndex() == 0) {
      m_fillColorWidget->hide();
    } else {
      m_fillColorWidget->show();
      // TPixel32 color  = currentStyle->getMainColor();
      TPixel32 color   = m_editedStyle->getColorParamValue(getColorParam());
      QString hexColor = color2Hex(color);
      m_hexLineEdit->setText(hexColor);
      m_newColor->setToolTip(hexColor);
      m_fillColorWidget->setToolTip(hexColor);
      QString myColor = QString::number(color.r) + ", " +
                        QString::number(color.g) + ", " +
                        QString::number(color.b);
      std::string myColorStr = myColor.toStdString();
      QString styleSheet     = "QFrame {background-color: rgb(%1);}";
      m_fillColorWidget->setStyleSheet(styleSheet.arg(myColor));
    }
    setOldStyleToStyle(currentStyle);
  }

  // Va fatto anche se non c'e' lo style perche' svuota la pagina
  m_settingsPage->setStyle(m_editedStyle);

  return isStyleNull;
}

//-----------------------------------------------------------------------------

void StyleEditor::setEditedStyleToStyle(const TColorStyle *style) {
  if (style == m_editedStyle.getPointer()) return;

  m_editedStyle = TColorStyleP(style->clone());
}

//-----------------------------------------------------------------------------

void StyleEditor::setOldStyleToStyle(const TColorStyle *style) {
  if (style == m_oldStyle.getPointer()) return;
  m_oldStyle = TColorStyleP(style->clone());
}

//-----------------------------------------------------------------------------

void StyleEditor::selectStyle(const TColorStyle &newStyle) {
  TPalette *palette = m_paletteHandle->getPalette();
  if (!palette) return;

  int styleIndex = m_paletteHandle->getStyleIndex();
  if (styleIndex < 0 || palette->getStyleCount() <= styleIndex) return;

  // Register the new previous/edited style pairs
  setOldStyleToStyle(palette->getStyle(styleIndex));
  setEditedStyleToStyle(&newStyle);

  m_editedStyle->assignNames(
      m_oldStyle.getPointer());  // Copy original name stored in the palette

  // For convenience's sake, copy the main color from the old color, if both
  // have one
  if (m_oldStyle && m_oldStyle->hasMainColor() && m_editedStyle &&
      m_editedStyle->hasMainColor())
    m_editedStyle->setMainColor(m_oldStyle->getMainColor());

  if (m_autoButton->isChecked()) {
    // If the adited style is linked to the studio palette, then activate the
    // edited flag
    if (m_editedStyle->getGlobalName() != L"" &&
        m_editedStyle->getOriginalName() != L"")
      m_editedStyle->setIsEditedFlag(true);

    // Apply new style, if required
    TUndoManager::manager()->add(new UndoPaletteChange(
        m_paletteHandle, styleIndex, *m_oldStyle, *m_editedStyle));

    palette->setStyle(styleIndex, m_editedStyle->clone());

    m_paletteHandle->notifyColorStyleChanged(false);
    palette->setDirtyFlag(true);
  }

  // Update editor widgets
  m_newColor->setStyle(*m_editedStyle);
  int tag = m_editedStyle->getTagId();
  if (tag == 4 || tag == 2000 || tag == 2800 || getStyleIndex() == 0) {
    m_fillColorWidget->hide();
  } else {
    m_fillColorWidget->show();
    // TPixel32 color  = m_editedStyle->getMainColor();
    TPixel32 color   = m_editedStyle->getColorParamValue(getColorParam());
    QString hexColor = color2Hex(color);
    m_hexLineEdit->setText(hexColor);
    m_newColor->setToolTip(hexColor);
    m_fillColorWidget->setToolTip(hexColor);
    QString myColor = QString::number(color.r) + ", " +
                      QString::number(color.g) + ", " +
                      QString::number(color.b);
    std::string myColorStr = myColor.toStdString();
    QString styleSheet     = "QFrame {background-color: rgb(%1); }";
    m_fillColorWidget->setStyleSheet(styleSheet.arg(myColor));
  }
  m_plainColorPage->setColor(*m_editedStyle, getColorParam());
  m_colorParameterSelector->setStyle(*m_editedStyle);
  m_settingsPage->setStyle(m_editedStyle);
}

//-----------------------------------------------------------------------------

void StyleEditor::addToPalette(const TColorStyle &newStyle) {
  TPalette *palette = m_paletteHandle->getPalette();
  if (!palette || palette->isLocked()) return;

  PaletteViewer *viewer = m_paletteController->getCurrentPaletteViewer();
  int pageIndex         = viewer ? viewer->geCurrentPageIndex() : 0;

  int styleIndex =
      PaletteCmd::createStyle(m_paletteHandle, palette->getPage(pageIndex));

  m_paletteController->getCurrentLevelPalette()->setStyleIndex(styleIndex);

  setEditedStyleToStyle(&newStyle);

  palette->setStyle(styleIndex, m_editedStyle->clone());

  m_paletteHandle->notifyColorStyleChanged(false);
  palette->setDirtyFlag(true);

  // Update editor widgets
  m_newColor->setStyle(*m_editedStyle);
  int tag = m_editedStyle->getTagId();
  if (tag == 4 || tag == 2000 || tag == 2800 || getStyleIndex() == 0) {
    m_fillColorWidget->hide();
  } else {
    m_fillColorWidget->show();
    // TPixel32 color  = m_editedStyle->getMainColor();
    TPixel32 color   = m_editedStyle->getColorParamValue(getColorParam());
    QString hexColor = color2Hex(color);
    m_hexLineEdit->setText(hexColor);
    m_newColor->setToolTip(hexColor);
    m_fillColorWidget->setToolTip(hexColor);
    QString myColor = QString::number(color.r) + ", " +
                      QString::number(color.g) + ", " +
                      QString::number(color.b);
    std::string myColorStr = myColor.toStdString();
    QString styleSheet     = "QFrame {background-color: rgb(%1); }";
    m_fillColorWidget->setStyleSheet(styleSheet.arg(myColor));
  }
  m_plainColorPage->setColor(*m_editedStyle, getColorParam());
  m_colorParameterSelector->setStyle(*m_editedStyle);
  m_settingsPage->setStyle(m_editedStyle);
}

//-----------------------------------------------------------------------------

void StyleEditor::onColorParamChanged() {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleIndex = getStyleIndex();
  if (styleIndex < 0 || palette->getStyleCount() <= styleIndex) return;

  m_paletteHandle->setStyleParamIndex(getColorParam());

  if (TColorStyle *currentStyle = palette->getStyle(styleIndex)) {
    setEditedStyleToStyle(currentStyle);

    m_plainColorPage->setColor(*m_editedStyle, getColorParam());
    m_settingsPage->setStyle(m_editedStyle);
  }
  int tag = m_editedStyle->getTagId();
  if (tag == 4 || tag == 2000 || tag == 2800 || getStyleIndex() == 0) {
    m_fillColorWidget->hide();
  } else {
    m_fillColorWidget->show();

    // TPixel32 color  = m_editedStyle->getMainColor();
    TPixel32 color   = m_editedStyle->getColorParamValue(getColorParam());
    QString hexColor = color2Hex(color);
    m_hexLineEdit->setText(hexColor);
    m_newColor->setToolTip(hexColor);
    m_fillColorWidget->setToolTip(hexColor);
    QString myColor = QString::number(color.r) + ", " +
                      QString::number(color.g) + ", " +
                      QString::number(color.b);
    std::string myColorStr = myColor.toStdString();
    QString styleSheet     = "QFrame {background-color: rgb(%1);}";
    m_fillColorWidget->setStyleSheet(styleSheet.arg(myColor));
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onParamStyleChanged(bool isDragging) {
  TPalette *palette = getPalette();
  if (!palette) return;

  int styleIndex = getStyleIndex();
  if (styleIndex < 0 || styleIndex > palette->getStyleCount()) return;

  if (m_autoButton->isChecked()) copyEditedStyleToPalette(isDragging);

  m_editedStyle->invalidateIcon();       // Refresh the new color icon
  m_newColor->setStyle(*m_editedStyle);  //

  int tag = m_editedStyle->getTagId();
  if (tag == 4 || tag == 2000 || tag == 2800 || getStyleIndex() == 0) {
    m_fillColorWidget->hide();
  } else {
    TPixel32 color = m_editedStyle->getColorParamValue(getColorParam());
    // TPixel32 color  = m_editedStyle->getMainColor();
    QString hexColor = color2Hex(color);
    m_hexLineEdit->setText(hexColor);
    m_newColor->setToolTip(hexColor);
    m_fillColorWidget->setToolTip(hexColor);
    QString myColor = QString::number(color.r) + ", " +
                      QString::number(color.g) + ", " +
                      QString::number(color.b);
    std::string myColorStr = myColor.toStdString();
    m_fillColorWidget->show();
    QString styleSheet = "QFrame {background-color: rgb(%1);}";
    m_fillColorWidget->setStyleSheet(styleSheet.arg(myColor));
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onHexEdited(const QString &text) {
  m_hsvAction->setDisabled(true);
  m_alphaAction->setDisabled(true);
  m_wheelAction->setDisabled(true);
  m_rgbAction->setDisabled(true);
}

//-----------------------------------------------------------------------------

void StyleEditor::onHideMenu() {
  m_hsvAction->setEnabled(true);
  m_alphaAction->setEnabled(true);
  m_wheelAction->setEnabled(true);
  m_rgbAction->setEnabled(true);
}

//-----------------------------------------------------------------------------

void StyleEditor::onPageChanged(int index) {
  m_styleSetsButton->setDisabled(false);
  switch (index) {
  case 1:  // Texture
    m_styleSetsButton->setMenu(m_textureMenu);
    break;
  case 2:  // Vector
    m_styleSetsButton->setMenu(m_vectorMenu);
    break;
  case 3:  // Raster
    m_styleSetsButton->setMenu(m_rasterMenu);
    break;
  default:
    m_styleSetsButton->setDisabled(true);
    break;
  }

  if (index < 1 || index > 3) return;

  onUpdateFavorites();
  update();
}
//-----------------------------------------------------------------------------

void StyleEditor::onHexChanged() {
  m_hsvAction->parentWidget()->clearFocus();
  QString hex = m_hexLineEdit->text();
  if (isHex(hex)) {
    TPixel32 color = hex2Color(hex);
    ColorModel cm;
    cm.setTPixel(color);
    onColorChanged(cm, false);
  }
}

//-----------------------------------------------------------------------------
QStringList StyleEditor::savePageStates(StylePageType pageType) const {
  QStringList pageStateData;
  QString pageData;

  if (pageType == StylePageType::Texture) {
    for (int i = 0; i < m_texturePages.size(); i++) {
      QString label = m_textureLabels[i]->text();
      QString hidden =
          m_textureMenu->actions()[i]->isChecked() ? "false" : "true";
      QString collapsed = m_textureButtons[i]->isChecked() ? "false" : "true";
      if (hidden == "false" && collapsed == "false")
        continue;  // Default state, don't save
      pageData = label + ":" + hidden + ":" + collapsed;
      pageStateData.push_back(pageData);
    }
  } else if (pageType == StylePageType::VectorCustom) {
    for (int i = 0; i < m_vectorPages.size(); i++) {
      QString label = m_vectorLabels[i]->text();
      QString hidden =
          m_vectorMenu->actions()[i]->isChecked() ? "false" : "true";
      QString collapsed = m_vectorButtons[i]->isChecked() ? "false" : "true";
      if (hidden == "false" && collapsed == "false")
        continue;  // Default state, don't save
      pageData = label + ":" + hidden + ":" + collapsed;
      pageStateData.push_back(pageData);
    }
  } else if (pageType == StylePageType::Raster) {
    for (int i = 0; i < m_rasterPages.size(); i++) {
      QString label = m_rasterLabels[i]->text();
      QString hidden =
          m_rasterMenu->actions()[i]->isChecked() ? "false" : "true";
      QString collapsed = m_rasterButtons[i]->isChecked() ? "false" : "true";
      if (hidden == "false" && collapsed == "false")
        continue;  // Default state, don't save
      pageData = label + ":" + hidden + ":" + collapsed;
      pageStateData.push_back(pageData);
    }
  }

  return pageStateData;
}

void StyleEditor::loadPageStates(StylePageType pageType,
                                 QStringList pageStateData) {
  for (int i = 0; i < pageStateData.size(); i++) {
    QStringList pageInfo(pageStateData[i].split(":"));
    if (pageInfo.size() != 3) continue;

    if (pageType == StylePageType::Texture) {
      for (int b = 0; b < m_textureButtons.size(); b++) {
        if (m_textureLabels[b]->text() != pageInfo[0]) continue;
        QPushButton *button = m_textureButtons[b];
        QAction *action     = m_textureMenu->actions()[b];
        if (pageInfo[2] == "true")
          button->setChecked(false);  // page collapsed -> checked is false
        if (pageInfo[1] == "true")
          action->setChecked(false);  // page hidden -> checked is false
        break;
      }
    } else if (pageType == StylePageType::VectorCustom) {
      for (int b = 0; b < m_vectorButtons.size(); b++) {
        if (m_vectorLabels[b]->text() != pageInfo[0]) continue;
        QPushButton *button = m_vectorButtons[b];
        QAction *action     = m_vectorMenu->actions()[b];
        if (pageInfo[2] == "true")
          button->setChecked(false);  // page collapsed -> checked is false
        if (pageInfo[1] == "true")
          action->setChecked(false);  // page hidden -> checked is false
        break;
      }
    } else if (pageType == StylePageType::Raster) {
      for (int b = 0; b < m_rasterButtons.size(); b++) {
        if (m_rasterLabels[b]->text() != pageInfo[0]) continue;
        QPushButton *button = m_rasterButtons[b];
        QAction *action     = m_rasterMenu->actions()[b];
        if (pageInfo[2] == "true")
          button->setChecked(false);  // page collapsed -> checked is false
        if (pageInfo[1] == "true")
          action->setChecked(false);  // page hidden -> checked is false
        break;
      }
    }
  }
}

void StyleEditor::save(QSettings &settings) const {
  settings.setValue("isVertical", m_plainColorPage->getIsVertical());
  int visibleParts = 0;
  if (m_wheelAction->isChecked()) visibleParts |= 0x01;
  if (m_hsvAction->isChecked()) visibleParts |= 0x02;
  if (m_alphaAction->isChecked()) visibleParts |= 0x04;
  if (m_rgbAction->isChecked()) visibleParts |= 0x08;
  settings.setValue("visibleParts", visibleParts);
  settings.setValue("splitterState", m_plainColorPage->getSplitterState());
  settings.setValue("texturePageStates",
                    savePageStates(StylePageType::Texture));
  settings.setValue("vectorPageStates",
                    savePageStates(StylePageType::VectorCustom));
  settings.setValue("rasterPageStates", savePageStates(StylePageType::Raster));
}
void StyleEditor::load(QSettings &settings) {
  QVariant isVertical = settings.value("isVertical");
  if (isVertical.canConvert(QVariant::Bool)) {
    m_colorPageIsVertical = isVertical.toBool();
    m_plainColorPage->setIsVertical(m_colorPageIsVertical);
  }
  QVariant visibleParts = settings.value("visibleParts");
  if (visibleParts.canConvert(QVariant::Int)) {
    int visiblePartsInt = visibleParts.toInt();

    if (visiblePartsInt & 0x01)
      m_wheelAction->setChecked(true);
    else
      m_wheelAction->setChecked(false);
    if (visiblePartsInt & 0x02)
      m_hsvAction->setChecked(true);
    else
      m_hsvAction->setChecked(false);
    if (visiblePartsInt & 0x04)
      m_alphaAction->setChecked(true);
    else
      m_alphaAction->setChecked(false);
    if (visiblePartsInt & 0x08)
      m_rgbAction->setChecked(true);
    else
      m_rgbAction->setChecked(false);
  }
  QVariant splitterState = settings.value("splitterState");
  if (splitterState.canConvert(QVariant::ByteArray))
    m_plainColorPage->setSplitterState(splitterState.toByteArray());

  QVariant texturePageStates = settings.value("texturePageStates");
  if (texturePageStates.canConvert(QVariant::StringList))
    loadPageStates(StylePageType::Texture, texturePageStates.toStringList());

  QVariant vectorPageStates = settings.value("vectorPageStates");
  if (vectorPageStates.canConvert(QVariant::StringList))
    loadPageStates(StylePageType::VectorCustom,
                   vectorPageStates.toStringList());

  QVariant rasterPageStates = settings.value("rasterPageStates");
  if (rasterPageStates.canConvert(QVariant::StringList))
    loadPageStates(StylePageType::Raster, rasterPageStates.toStringList());
}

//-----------------------------------------------------------------------------

void StyleEditor::updateColorCalibration() {
  m_plainColorPage->updateColorCalibration();
}

//-----------------------------------------------------------------------------

QString StyleEditor::getStylePageFilter(StylePageType pageType) {
  switch (pageType) {
  case StylePageType::Texture:
    return "*";
    break;
  case StylePageType::VectorCustom:
    return "*.pli *.tif *.png *.tga *.tiff *.sgi *.rgb *.gen";
    break;
  case StylePageType::VectorBrush:
    return "*.pli";
    break;
  case StylePageType::Raster:
    return "*.myb";
    break;
  }

  return "";
}

//-----------------------------------------------------------------------------

void StyleEditor::createStylePage(StylePageType pageType, TFilePath styleFolder,
                                  QString filters, bool isFavorite,
                                  int dirDepth) {
  // Let's look for files
  TFilePathSet fps;
  if (styleFolder != TFilePath()) {
    try {
      QDir patternDir(QString::fromStdWString(styleFolder.getWideString()));
      patternDir.setNameFilters(filters.split(' '));

      if (TFileStatus(styleFolder).doesExist())
        TSystem::readDirectory(fps, patternDir);
    } catch (...) {
      return;
    }
  }

  if (isFavorite || !fps.empty() ||
      pageType == StylePageType::VectorGenerated || dirDepth > 0) {
    // Set up style set page
    bool isExternal      = false;
    bool isMyFavoriteSet = false;

    if (isFavorite && dirDepth == 0) isMyFavoriteSet = true;

    QString labelText =
        (pageType == StylePageType::VectorGenerated
             ? tr("Generated")
             : (isMyFavoriteSet) ? tr("My Favorites")
                                 : styleFolder.withoutParentDir().getQString());
    if (isFavorite && dirDepth > 0)
      labelText += tr(" (Favorites)");
    else if (!isFavorite && pageType == StylePageType::Raster &&
             !styleFolder.getQString().contains("mypaint brushes")) {
      labelText += tr(" (External)");
      isExternal = true;
    }
    labelText[0]          = labelText[0].toUpper();
    ClickableLabel *label = new ClickableLabel(labelText, this);

    QPushButton *button = new QPushButton("", this);
    button->setObjectName("menuToggleButton");
    button->setFixedSize(15, 15);
    button->setIcon(createQIcon("menu_toggle"));
    button->setCheckable(true);
    button->setChecked(true);
    button->setFocusPolicy(Qt::NoFocus);

    QCheckBox *checkBox = new QCheckBox(label->text(), this);
    checkBox->setChecked(true);
    checkBox->setCheckable(true);

    switch (pageType) {
    case StylePageType::Texture: {
      TextureStyleChooserPage *newPage =
          new TextureStyleChooserPage(styleFolder, filters, this);
      newPage->setFolderDepth(dirDepth);
      newPage->setStyleSetName(label->text());
      if (isFavorite) newPage->setFavorite(true);
      if (!isFavorite || dirDepth > 0) newPage->setAllowFavorite(true);
      if (dirDepth == 0 || isExternal) newPage->setAllowPageDelete(false);
      newPage->setExternal(isExternal);
      newPage->setMyFavoriteSet(isMyFavoriteSet);
      m_texturePages.push_back(newPage);
      m_textureLabels.push_back(label);
      m_textureButtons.push_back(button);

      if (isFavorite && dirDepth == 0 && fps.size() == 0) {
        newPage->setHidden(true);
        label->setHidden(true);
        button->setHidden(true);
        button->setDisabled(true);
      }

      connect(newPage, SIGNAL(styleSelected(const TColorStyle &)), this,
              SLOT(selectStyle(const TColorStyle &)));
      connect(newPage, SIGNAL(refreshFavorites()), this,
              SLOT(onUpdateFavorites()));
      connect(button, SIGNAL(toggled(bool)), newPage, SLOT(onTogglePage(bool)));
      connect(label, SIGNAL(click()), button, SLOT(click()));
      label->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(label, SIGNAL(customContextMenuRequested(const QPoint &)),
              newPage, SLOT(onLabelContextMenu(const QPoint &)));
      connect(checkBox, SIGNAL(stateChanged(int)), this,
              SLOT(onToggleTextureSet(int)));

      connect(newPage, SIGNAL(customStyleSelected()), this,
              SLOT(onSwitchToSettings()));

      QWidgetAction *menuAction = new QWidgetAction(m_textureMenu);
      menuAction->setDefaultWidget(checkBox);

      // Favorites should be 1st
      if (isMyFavoriteSet)
        menuAction->setVisible(!m_textureButtons[0]->isHidden());

      m_textureMenu->insertAction(
          m_textureMenu->actions()[m_texturePages.size() - 1], menuAction);
      break;
    }
    case StylePageType::VectorGenerated: {
      SpecialStyleChooserPage *newPage =
          new SpecialStyleChooserPage(TFilePath(), QString(), this);
      newPage->setFolderDepth(dirDepth);
      newPage->setStyleSetName(label->text());
      newPage->setAllowFavorite(true);
      newPage->setAllowPageDelete(false);
      newPage->setExternal(false);
      newPage->setMyFavoriteSet(isMyFavoriteSet);
      m_vectorPages.push_back(newPage);
      m_vectorLabels.push_back(label);
      m_vectorButtons.push_back(button);

      connect(newPage, SIGNAL(styleSelected(const TColorStyle &)), this,
              SLOT(selectStyle(const TColorStyle &)));
      connect(newPage, SIGNAL(refreshFavorites()), this,
              SLOT(onUpdateFavorites()));
      connect(button, SIGNAL(toggled(bool)), newPage, SLOT(onTogglePage(bool)));
      connect(label, SIGNAL(click()), button, SLOT(click()));
      label->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(label, SIGNAL(customContextMenuRequested(const QPoint &)),
              newPage, SLOT(onLabelContextMenu(const QPoint &)));
      connect(checkBox, SIGNAL(stateChanged(int)), this,
              SLOT(onToggleVectorSet(int)));

      QWidgetAction *menuAction = new QWidgetAction(m_vectorMenu);
      menuAction->setDefaultWidget(checkBox);

      // Favorites should be 1st
      if (isMyFavoriteSet)
        menuAction->setVisible(!m_vectorButtons[0]->isHidden());

      m_vectorMenu->insertAction(
          m_vectorMenu->actions()[m_vectorPages.size() - 1], menuAction);
      break;
    }
    case StylePageType::VectorCustom: {
      CustomStyleChooserPage *newPage =
          new CustomStyleChooserPage(styleFolder, filters, this);
      newPage->setFolderDepth(dirDepth);
      newPage->setStyleSetName(label->text());
      if (isFavorite) newPage->setFavorite(true);
      if (!isFavorite || dirDepth > 0) newPage->setAllowFavorite(true);
      if (dirDepth == 0 || isExternal) newPage->setAllowPageDelete(false);
      newPage->setExternal(isExternal);
      newPage->setMyFavoriteSet(isMyFavoriteSet);
      m_vectorPages.push_back(newPage);
      m_vectorLabels.push_back(label);
      m_vectorButtons.push_back(button);

      if (isFavorite && dirDepth == 0 && fps.size() == 0) {
        newPage->setHidden(true);
        label->setHidden(true);
        button->setHidden(true);
        button->setDisabled(true);
      }

      connect(newPage, SIGNAL(styleSelected(const TColorStyle &)), this,
              SLOT(selectStyle(const TColorStyle &)));
      connect(newPage, SIGNAL(refreshFavorites()), this,
              SLOT(onUpdateFavorites()));
      connect(button, SIGNAL(toggled(bool)), newPage, SLOT(onTogglePage(bool)));
      connect(label, SIGNAL(click()), button, SLOT(click()));
      label->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(label, SIGNAL(customContextMenuRequested(const QPoint &)),
              newPage, SLOT(onLabelContextMenu(const QPoint &)));
      connect(checkBox, SIGNAL(stateChanged(int)), this,
              SLOT(onToggleVectorSet(int)));

      QWidgetAction *menuAction = new QWidgetAction(m_vectorMenu);
      menuAction->setDefaultWidget(checkBox);

      // Favorites should be 1st
      if (isMyFavoriteSet)
        menuAction->setVisible(!m_vectorButtons[0]->isHidden());

      m_vectorMenu->insertAction(
          m_vectorMenu->actions()[m_vectorPages.size() - 1], menuAction);
      break;
    }
    case StylePageType::VectorBrush: {
      VectorBrushStyleChooserPage *newPage =
          new VectorBrushStyleChooserPage(styleFolder, filters, this);
      newPage->setFolderDepth(dirDepth);
      newPage->setStyleSetName(label->text());
      if (isFavorite) newPage->setFavorite(true);
      if (!isFavorite || dirDepth > 0) newPage->setAllowFavorite(true);
      if (dirDepth == 0 || isExternal) newPage->setAllowPageDelete(false);
      newPage->setExternal(isExternal);
      newPage->setMyFavoriteSet(isMyFavoriteSet);
      m_vectorPages.push_back(newPage);
      m_vectorLabels.push_back(label);
      m_vectorButtons.push_back(button);

      if (isFavorite && dirDepth == 0 && fps.size() == 0) {
        newPage->setHidden(true);
        label->setHidden(true);
        button->setHidden(true);
        button->setDisabled(true);
      }

      connect(newPage, SIGNAL(styleSelected(const TColorStyle &)), this,
              SLOT(selectStyle(const TColorStyle &)));
      connect(newPage, SIGNAL(refreshFavorites()), this,
              SLOT(onUpdateFavorites()));
      connect(button, SIGNAL(toggled(bool)), newPage, SLOT(onTogglePage(bool)));
      connect(label, SIGNAL(click()), button, SLOT(click()));
      label->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(label, SIGNAL(customContextMenuRequested(const QPoint &)),
              newPage, SLOT(onLabelContextMenu(const QPoint &)));
      connect(checkBox, SIGNAL(stateChanged(int)), this,
              SLOT(onToggleVectorSet(int)));

      QWidgetAction *menuAction = new QWidgetAction(m_vectorMenu);
      menuAction->setDefaultWidget(checkBox);

      // Favorites should be 1st
      if (isMyFavoriteSet)
        menuAction->setVisible(!m_vectorButtons[0]->isHidden());

      m_vectorMenu->insertAction(
          m_vectorMenu->actions()[m_vectorPages.size() - 1], menuAction);
      break;
    }
    case StylePageType::Raster: {
      MyPaintBrushStyleChooserPage *newPage =
          new MyPaintBrushStyleChooserPage(styleFolder, filters, this);
      newPage->setFolderDepth(dirDepth);
      newPage->setStyleSetName(label->text());
      if (isFavorite) newPage->setFavorite(true);
      if (!isFavorite || dirDepth > 0) newPage->setAllowFavorite(true);
      if (dirDepth == 0 || isExternal) newPage->setAllowPageDelete(false);
      newPage->setExternal(isExternal);
      newPage->setMyFavoriteSet(isMyFavoriteSet);
      m_rasterPages.push_back(newPage);
      m_rasterLabels.push_back(label);
      m_rasterButtons.push_back(button);

      if (isFavorite && dirDepth == 0 && fps.size() == 0) {
        newPage->setHidden(true);
        label->setHidden(true);
        button->setHidden(true);
        button->setDisabled(true);
      }

      connect(newPage, SIGNAL(styleSelected(const TColorStyle &)), this,
              SLOT(selectStyle(const TColorStyle &)));
      connect(newPage, SIGNAL(refreshFavorites()), this,
              SLOT(onUpdateFavorites()));
      connect(button, SIGNAL(toggled(bool)), newPage, SLOT(onTogglePage(bool)));
      connect(label, SIGNAL(click()), button, SLOT(click()));
      label->setContextMenuPolicy(Qt::CustomContextMenu);
      connect(label, SIGNAL(customContextMenuRequested(const QPoint &)),
              newPage, SLOT(onLabelContextMenu(const QPoint &)));
      connect(checkBox, SIGNAL(stateChanged(int)), this,
              SLOT(onToggleRasterSet(int)));

      QWidgetAction *menuAction = new QWidgetAction(m_rasterMenu);
      menuAction->setDefaultWidget(checkBox);

      // Favorites should be 1st
      if (isMyFavoriteSet)
        menuAction->setVisible(!m_rasterButtons[0]->isHidden());

      m_rasterMenu->insertAction(
          m_rasterMenu->actions()[m_rasterPages.size() - 1], menuAction);
      break;
    }
    }
  }

  if (styleFolder == TFilePath()) return;

  // Load subfolders
  try {
    QStringList fpList;
    if (TFileStatus(styleFolder).doesExist())
      TSystem::readDirectory_DirItems(fpList, styleFolder);

    QStringList::iterator fpListIt;
    for (fpListIt = fpList.begin(); fpListIt != fpList.end(); fpListIt++)
      createStylePage(pageType, styleFolder + TFilePath(*fpListIt), filters,
                      isFavorite, (dirDepth + 1));

  } catch (...) {
    return;
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::initializeStyleMenus() {
  QPushButton *button;
  QWidgetAction *menuAction;

  m_textureMenu = new QMenu(this);
  m_textureMenu->addSeparator();
  button = new QPushButton(tr("Show All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onShowAllTextureSet()));
  menuAction = new QWidgetAction(m_textureMenu);
  menuAction->setDefaultWidget(button);
  m_textureMenu->addAction(menuAction);
  button = new QPushButton(tr("Hide All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onHideAllTextureSet()));
  menuAction = new QWidgetAction(m_textureMenu);
  menuAction->setDefaultWidget(button);
  m_textureMenu->addAction(menuAction);
  button = new QPushButton(tr("Collapse All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onCollapseAllTextureSet()));
  menuAction = new QWidgetAction(m_textureMenu);
  menuAction->setDefaultWidget(button);
  m_textureMenu->addAction(menuAction);
  button = new QPushButton(tr("Expand All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onExpandAllTextureSet()));
  menuAction = new QWidgetAction(m_textureMenu);
  menuAction->setDefaultWidget(button);
  m_textureMenu->addAction(menuAction);

  m_vectorMenu = new QMenu(this);
  m_vectorMenu->addSeparator();
  button = new QPushButton(tr("Show All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onShowAllVectorSet()));
  menuAction = new QWidgetAction(m_vectorMenu);
  menuAction->setDefaultWidget(button);
  m_vectorMenu->addAction(menuAction);
  button = new QPushButton(tr("Hide All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onHideAllVectorSet()));
  menuAction = new QWidgetAction(m_vectorMenu);
  menuAction->setDefaultWidget(button);
  m_vectorMenu->addAction(menuAction);
  button = new QPushButton(tr("Collapse All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onCollapseAllVectorSet()));
  menuAction = new QWidgetAction(m_vectorMenu);
  menuAction->setDefaultWidget(button);
  m_vectorMenu->addAction(menuAction);
  button = new QPushButton(tr("Expand All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onExpandAllVectorSet()));
  menuAction = new QWidgetAction(m_vectorMenu);
  menuAction->setDefaultWidget(button);
  m_vectorMenu->addAction(menuAction);

  m_rasterMenu = new QMenu(this);
  m_rasterMenu->addSeparator();
  button = new QPushButton(tr("Show All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onShowAllRasterSet()));
  menuAction = new QWidgetAction(m_rasterMenu);
  menuAction->setDefaultWidget(button);
  m_rasterMenu->addAction(menuAction);
  button = new QPushButton(tr("Hide All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onHideAllRasterSet()));
  menuAction = new QWidgetAction(m_rasterMenu);
  menuAction->setDefaultWidget(button);
  m_rasterMenu->addAction(menuAction);
  button = new QPushButton(tr("Collapse All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onCollapseAllRasterSet()));
  menuAction = new QWidgetAction(m_rasterMenu);
  menuAction->setDefaultWidget(button);
  m_rasterMenu->addAction(menuAction);
  button = new QPushButton(tr("Expand All"), this);
  connect(button, SIGNAL(clicked()), this, SLOT(onExpandAllRasterSet()));
  menuAction = new QWidgetAction(m_rasterMenu);
  menuAction->setDefaultWidget(button);
  m_rasterMenu->addAction(menuAction);
}

//-----------------------------------------------------------------------------

void StyleEditor::onToggleTextureSet(int checkedState) {
  bool checked = checkedState == Qt::Checked;

  QCheckBox *action = qobject_cast<QCheckBox *>(sender());
  QString s         = action->text();
  int index;
  for (index = 0; index < m_texturePages.size(); index++)
    if (m_texturePages[index]->getStyleSetName() == s) break;
  if (index >= m_texturePages.size()) return;
  m_textureButtons[index]->setVisible(checked);
  m_textureLabels[index]->setVisible(checked);
  if (m_textureButtons[index]->isChecked())
    m_texturePages[index]->setVisible(checked);
}

//-----------------------------------------------------------------------------

void StyleEditor::onToggleVectorSet(int checkedState) {
  bool checked = checkedState == Qt::Checked;

  QCheckBox *action = qobject_cast<QCheckBox *>(sender());
  QString s         = action->text();
  int index;
  for (index = 0; index < m_vectorPages.size(); index++)
    if (m_vectorPages[index]->getStyleSetName() == s) break;
  if (index >= m_vectorPages.size()) return;
  m_vectorButtons[index]->setVisible(checked);
  m_vectorLabels[index]->setVisible(checked);
  if (m_vectorButtons[index]->isChecked())
    m_vectorPages[index]->setVisible(checked);
}

//-----------------------------------------------------------------------------

void StyleEditor::onToggleRasterSet(int checkedState) {
  bool checked = checkedState == Qt::Checked;

  QCheckBox *action = qobject_cast<QCheckBox *>(sender());
  QString s         = action->text();
  int index;
  for (index = 0; index < m_rasterPages.size(); index++)
    if (m_rasterPages[index]->getStyleSetName() == s) break;
  if (index >= m_rasterPages.size()) return;
  m_rasterButtons[index]->setVisible(checked);
  m_rasterLabels[index]->setVisible(checked);
  if (m_rasterButtons[index]->isChecked())
    m_rasterPages[index]->setVisible(checked);
}

//-----------------------------------------------------------------------------

void StyleEditor::onShowAllTextureSet() {
  QList<QAction *> actions = m_textureMenu->actions();
  QList<QAction *>::Iterator it;

  int index = 0;
  for (it = actions.begin(); it != actions.end(); it++) {
    index++;
    if (index > m_texturePages.size()) break;
    QWidgetAction *action = qobject_cast<QWidgetAction *>(*it);
    if (action->isEnabled()) {
      QCheckBox *checkBox = dynamic_cast<QCheckBox *>(action->defaultWidget());
      checkBox->setChecked(true);
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onShowAllVectorSet() {
  QList<QAction *> actions = m_vectorMenu->actions();
  QList<QAction *>::Iterator it;

  int index = 0;
  for (it = actions.begin(); it != actions.end(); it++) {
    index++;
    if (index > m_vectorPages.size()) break;
    QWidgetAction *action = qobject_cast<QWidgetAction *>(*it);
    if (action->isEnabled()) {
      QCheckBox *checkBox = dynamic_cast<QCheckBox *>(action->defaultWidget());
      checkBox->setChecked(true);
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onShowAllRasterSet() {
  QList<QAction *> actions = m_rasterMenu->actions();
  QList<QAction *>::Iterator it;

  int index = 0;
  for (it = actions.begin(); it != actions.end(); it++) {
    index++;
    if (index > m_rasterPages.size()) break;
    QWidgetAction *action = qobject_cast<QWidgetAction *>(*it);
    if (action->isEnabled()) {
      QCheckBox *checkBox = dynamic_cast<QCheckBox *>(action->defaultWidget());
      checkBox->setChecked(true);
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onHideAllTextureSet() {
  QList<QAction *> actions = m_textureMenu->actions();
  QList<QAction *>::Iterator it;

  int index = 0;
  for (it = actions.begin(); it != actions.end(); it++) {
    index++;
    if (index > m_texturePages.size()) break;
    QWidgetAction *action = qobject_cast<QWidgetAction *>(*it);
    if (action->isEnabled()) {
      QCheckBox *checkBox = dynamic_cast<QCheckBox *>(action->defaultWidget());
      checkBox->setChecked(false);
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onHideAllVectorSet() {
  QList<QAction *> actions = m_vectorMenu->actions();
  QList<QAction *>::Iterator it;

  int index = 0;
  for (it = actions.begin(); it != actions.end(); it++) {
    index++;
    if (index > m_vectorPages.size()) break;
    QWidgetAction *action = qobject_cast<QWidgetAction *>(*it);
    if (action->isEnabled()) {
      QCheckBox *checkBox = dynamic_cast<QCheckBox *>(action->defaultWidget());
      checkBox->setChecked(false);
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onHideAllRasterSet() {
  QList<QAction *> actions = m_rasterMenu->actions();
  QList<QAction *>::Iterator it;

  int index = 0;
  for (it = actions.begin(); it != actions.end(); it++) {
    index++;
    if (index > m_rasterPages.size()) break;
    QWidgetAction *action = qobject_cast<QWidgetAction *>(*it);
    if (action->isEnabled()) {
      QCheckBox *checkBox = dynamic_cast<QCheckBox *>(action->defaultWidget());
      checkBox->setChecked(false);
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onCollapseAllTextureSet() {
  std::vector<QPushButton *>::iterator it;
  for (it = m_textureButtons.begin(); it != m_textureButtons.end(); it++) {
    QPushButton *button = *it;
    if (button->isEnabled() && button->isVisible()) button->setChecked(false);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onCollapseAllVectorSet() {
  std::vector<QPushButton *>::iterator it;
  for (it = m_vectorButtons.begin(); it != m_vectorButtons.end(); it++) {
    QPushButton *button = *it;
    if (button->isEnabled() && button->isVisible()) button->setChecked(false);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onCollapseAllRasterSet() {
  std::vector<QPushButton *>::iterator it;
  for (it = m_rasterButtons.begin(); it != m_rasterButtons.end(); it++) {
    QPushButton *button = *it;
    if (button->isEnabled() && button->isVisible()) button->setChecked(false);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onExpandAllTextureSet() {
  std::vector<QPushButton *>::iterator it;
  for (it = m_textureButtons.begin(); it != m_textureButtons.end(); it++) {
    QPushButton *button = *it;
    if (button->isEnabled() && button->isVisible()) button->setChecked(true);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onExpandAllVectorSet() {
  std::vector<QPushButton *>::iterator it;
  for (it = m_vectorButtons.begin(); it != m_vectorButtons.end(); it++) {
    QPushButton *button = *it;
    if (button->isEnabled() && button->isVisible()) button->setChecked(true);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onExpandAllRasterSet() {
  std::vector<QPushButton *>::iterator it;
  for (it = m_rasterButtons.begin(); it != m_rasterButtons.end(); it++) {
    QPushButton *button = *it;
    if (button->isEnabled() && button->isVisible()) button->setChecked(true);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::setUpdated(TFilePath setPath) {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3 || setPath.isEmpty()) return;

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1)
    pages = &m_texturePages;
  else if (tab == 2)
    pages = &m_vectorPages;
  else if (tab == 3)
    pages = &m_rasterPages;

  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin(); it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    if (page->getStylesFolder() == setPath) {
      page->loadItems();
      break;
    }
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onUpdateFavorites() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  int chipSize;
  QPushButton *button;
  ClickableLabel *label;
  QMenu *menu;
  StyleChooserPage *page;
  int minChipCount = 2;
  if (tab == 1) {
    chipSize     = m_texturePages[0]->getChipCount();
    button       = m_textureButtons[0];
    label        = m_textureLabels[0];
    page         = m_texturePages[0];
    menu         = m_textureMenu;
    minChipCount = 3;
  } else if (tab == 2) {
    chipSize = m_vectorPages[0]->getChipCount();
    button   = m_vectorButtons[0];
    label    = m_vectorLabels[0];
    page     = m_vectorPages[0];
    menu     = m_vectorMenu;
  } else if (tab == 3) {
    chipSize = m_rasterPages[0]->getChipCount();
    button   = m_rasterButtons[0];
    label    = m_rasterLabels[0];
    page     = m_rasterPages[0];
    menu     = m_rasterMenu;
  }

  if (chipSize >= minChipCount || page->isLoading()) {
    button->setDisabled(false);
    QWidgetAction *action = qobject_cast<QWidgetAction *>(menu->actions()[0]);
    action->setVisible(true);
    QCheckBox *checkBox = dynamic_cast<QCheckBox *>(action->defaultWidget());
    if (!checkBox->isChecked()) return;
    label->setHidden(false);
    button->setHidden(false);
    if (button->isChecked()) page->setHidden(false);
  } else {
    button->setDisabled(true);
    QWidgetAction *action = qobject_cast<QWidgetAction *>(menu->actions()[0]);
    action->setVisible(false);
    QCheckBox *checkBox = dynamic_cast<QCheckBox *>(action->defaultWidget());
    if (!checkBox->isChecked()) return;
    label->setHidden(true);
    button->setHidden(true);
    page->setHidden(true);
  }

  update();
}

//-----------------------------------------------------------------------------

void StyleEditor::onRemoveSelectedStylesFromFavorites() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  StyleChooserPage *page;
  if (tab == 1)
    page = m_texturePages[0];
  else if (tab == 2)
    page = m_vectorPages[0];
  else if (tab == 3)
    page = m_rasterPages[0];

  page->removeSelectedStylesFromSet(page->getSelection());
  clearSelection();
  update();
}

//-----------------------------------------------------------------------------

void StyleEditor::onAddSelectedStylesToFavorites() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  std::vector<StyleChooserPage *> *pages;
  TFilePath setPath;
  if (tab == 1) {
    pages   = &m_texturePages;
    setPath = m_texturePages[0]->getStylesFolder();
  } else if (tab == 2) {
    pages   = &m_vectorPages;
    setPath = m_vectorPages[0]->getStylesFolder();
  } else if (tab == 3) {
    pages   = &m_rasterPages;
    setPath = m_rasterPages[0]->getStylesFolder();
  }

  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin() + 1; it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    page->addSelectedStylesToSet(page->getSelection(), setPath);
  }

  clearSelection();
  update();
};

//-----------------------------------------------------------------------------

void StyleEditor::onAddSelectedStylesToPalette() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1)
    pages = &m_texturePages;
  else if (tab == 2)
    pages = &m_vectorPages;
  else if (tab == 3)
    pages = &m_rasterPages;

  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin(); it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    page->addSelectedStylesToPalette(page->getSelection());
  }

  clearSelection();
};

//-----------------------------------------------------------------------------

void StyleEditor::onCopySelectedStylesToSet() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  QAction *action = dynamic_cast<QAction *>(sender());
  QString setName = action->data().toString();

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1)
    pages = &m_texturePages;
  else if (tab == 2)
    pages = &m_vectorPages;
  else if (tab == 3)
    pages = &m_rasterPages;

  StyleChooserPage *dstPage = 0;
  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin(); it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    if (page->getStyleSetName() == setName) {
      dstPage = page;
      break;
    }
  }
  if (!dstPage) return;

  for (it = pages->begin(); it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    page->addSelectedStylesToSet(page->getSelection(),
                                 dstPage->getStylesFolder());
  }

  clearSelection();
  update();
}

//-----------------------------------------------------------------------------

void StyleEditor::onMoveSelectedStylesToSet() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  QAction *action = dynamic_cast<QAction *>(sender());
  QString setName = action->data().toString();

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1)
    pages = &m_texturePages;
  else if (tab == 2)
    pages = &m_vectorPages;
  else if (tab == 3)
    pages = &m_rasterPages;

  StyleChooserPage *dstPage = 0;
  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin(); it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    if (page->getStyleSetName() == setName) {
      dstPage = page;
      break;
    }
  }
  if (!dstPage) return;

  for (it = pages->begin(); it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    page->addSelectedStylesToSet(page->getSelection(),
                                 dstPage->getStylesFolder());
    page->removeSelectedStylesFromSet(page->getSelection());
  }

  clearSelection();
  update();
}

//-----------------------------------------------------------------------------

void StyleEditor::onRemoveSelectedStyleFromSet() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  if (!isSelectingFavoritesOnly()) {
    int ret = DVGui::MsgBox(
        QObject::tr("Removing the selected Styles will permanently "
                    "delete style files from their sets. "
                    "This cannot be undone!\nAre you sure?"),
        QObject::tr("Ok"), QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) return;
  }

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1)
    pages = &m_texturePages;
  else if (tab == 2)
    pages = &m_vectorPages;
  else if (tab == 3)
    pages = &m_rasterPages;

  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin(); it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    page->removeSelectedStylesFromSet(page->getSelection());
  }

  clearSelection();
}

//-----------------------------------------------------------------------------

TFilePath StyleEditor::getSetStyleFolder(QString setName) {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return TFilePath();

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1)
    pages = &m_texturePages;
  else if (tab == 2)
    pages = &m_vectorPages;
  else if (tab == 3)
    pages = &m_rasterPages;

  StyleChooserPage *dstPage = 0;
  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin(); it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    if (page->getStyleSetName() == setName) return page->getStylesFolder();
  }

  return TFilePath();
}

//-----------------------------------------------------------------------------

void StyleEditor::updatePage(int pageIndex) {
  if (pageIndex < 1 || pageIndex > 3) return;

  TFilePathSet fps;
  std::vector<StyleChooserPage *> *pages;
  if (pageIndex == 1) {
    fps   = TStyleManager::instance()->getTextureStyleFolders();
    pages = &m_texturePages;
  } else if (pageIndex == 2) {
    fps   = TStyleManager::instance()->getCustomStyleFolders();
    pages = &m_vectorPages;
  } else if (pageIndex == 3) {
    fps   = TStyleManager::instance()->getBrushStyleFolders();
    pages = &m_rasterPages;
  }

  // Remove pages that were deleted
  std::vector<StyleChooserPage *>::reverse_iterator rit;
  int i = pages->size();
  for (rit = pages->rbegin(); rit != pages->rend(); rit++) {
    StyleChooserPage *page = *rit;
    i--;
    TFilePathSet::iterator fpsit =
        std::find(fps.begin(), fps.end(), page->getStylesFolder());
    if (fpsit != fps.end()) {
      fps.erase(fpsit);
      continue;
    }
    removeStyleSetAtIndex(i, pageIndex);
  }

  // Add any pages that are new
  TFilePathSet::iterator it;
  for (it = fps.begin(); it != fps.end(); it++) {
    TFilePath fp = *it;
    if (fp.isEmpty()) continue;
    bool isFavorite = ToonzFolder::getMyFavoritesFolder().isAncestorOf(fp);
    StylePageType pageType;
    if (pageIndex == 1)
      pageType = StylePageType::Texture;
    else if (pageIndex == 2) {
      pageType = StylePageType::VectorCustom;
      if ((ToonzFolder::getLibraryFolder() + TFilePath("vector brushes"))
              .isAncestorOf(fp))
        pageType = StylePageType::VectorBrush;
    } else if (pageIndex == 3)
      pageType = StylePageType::Raster;

    createNewStyleSet(pageType, *it, isFavorite);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onAddNewStyleSet() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  StylePageType pageType;

  if (tab == 1)
    pageType = StylePageType::Texture;
  else if (tab == 2)
    pageType = StylePageType::VectorCustom;
  else if (tab == 3)
    pageType = StylePageType::Raster;

  NewStyleSetPopup *popup = new NewStyleSetPopup(pageType, this);
  popup->exec();
}

//-----------------------------------------------------------------------------

void StyleEditor::onScanStyleSetChanges() {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  TFilePath libPath = ToonzFolder::getLibraryFolder();
  TFilePath favoritesLibPath =
      ToonzFolder::getMyFavoritesFolder() + TFilePath("library");
  StylePageType pageType;

  TFilePathSet fps;

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1) {
    pages    = &m_texturePages;
    pageType = StylePageType::Texture;

    fps.push_back(libPath + TFilePath("textures"));
    fps.push_back(favoritesLibPath + TFilePath("textures"));
  } else if (tab == 2) {
    pages    = &m_vectorPages;
    pageType = StylePageType::VectorCustom;

    fps.push_back(libPath + TFilePath("custom styles"));
    fps.push_back(libPath + TFilePath("vector brushes"));
    fps.push_back(favoritesLibPath + TFilePath("vector styles"));
  } else if (tab == 3) {
    pages    = &m_rasterPages;
    pageType = StylePageType::Raster;

    fps.push_back(favoritesLibPath + TFilePath("raster styles"));

    TFilePathSet dirs = TMyPaintBrushStyle::getBrushesDirs();
    if (!dirs.empty()) fps.merge(dirs);
  }

  QString filters = getStylePageFilter(pageType);

  QStringList fpList;
  try {
    QStringList tmpList;
    QStringList::iterator tmpIt;

    TFilePathSet::iterator fpsIt;
    for (fpsIt = fps.begin(); fpsIt != fps.end(); fpsIt++) {
      if (!TFileStatus(*fpsIt).doesExist()) continue;
      tmpList.clear();
      TSystem::readDirectory_DirItems(tmpList, *fpsIt);
      for (tmpIt = tmpList.begin(); tmpIt != tmpList.end(); tmpIt++)
        fpList.push_back((TFilePath(*fpsIt) + TFilePath(*tmpIt)).getQString());
    }
  } catch (...) {
  }

  // Let's remove anything that was deleted while excluding what still exists
  std::vector<StyleChooserPage *>::reverse_iterator rit;
  int i = pages->size();
  for (rit = pages->rbegin(); rit != pages->rend(); rit++) {
    StyleChooserPage *page = *rit;
    TFilePath styleFolder  = page->getStylesFolder();
    i--;
    if (styleFolder.isEmpty()) continue;  // likely the Generated set
    if (i == 0 || page->isRootFolder()) {
      page->loadItems();
      continue;
    }
    QStringList::iterator sit =
        std::find(fpList.begin(), fpList.end(), styleFolder.getQString());
    if (sit != fpList.end()) {
      fpList.erase(sit);
      page->loadItems();
      continue;
    }
    removeStyleSetAtIndex(i, tab);
  }

  // Add anything new that is left in the list
  QStringList::iterator fpListIt;
  for (fpListIt = fpList.begin(); fpListIt != fpList.end(); fpListIt++) {
    TFilePath fp(*fpListIt);
    if (tab == 2)
      pageType = (libPath + TFilePath("vector brushes")).isAncestorOf(fp)
                     ? StylePageType::VectorBrush
                     : StylePageType::VectorCustom;
    createNewStyleSet(pageType, fp, false);
  }
}

//-----------------------------------------------------------------------------

void StyleEditor::onSwitchToSettings() { m_styleBar->setCurrentIndex(4); }

//-----------------------------------------------------------------------------

void StyleEditor::createNewStyleSet(StylePageType pageType, TFilePath pagePath,
                                    bool isFavorite) {
  if (pagePath == TFilePath()) return;

  try {
    TSystem::mkDir(pagePath);
  } catch (TSystemException se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  }

  QString filters;
  int pageIndex;
  switch (pageType) {
  case StylePageType::Texture:
    pageIndex = 1;
    break;
  case StylePageType::VectorCustom:
    pageIndex = 2;
    break;
  case StylePageType::VectorBrush:
    pageIndex = 2;
    break;
  case StylePageType::Raster:
    pageIndex = 3;
    break;
  }

  filters = getStylePageFilter(pageType);

  createStylePage(pageType, pagePath, filters, isFavorite, 1);

  QWidget *oldPage;
  switch (pageType) {
  case StylePageType::Texture: {
    oldPage = m_textureOutsideArea->takeWidget();
    m_textureOutsideArea->setWidget(
        makeChooserPageWithoutScrollBar(createTexturePage()));
    break;
  }
  case StylePageType::VectorBrush:
  case StylePageType::VectorCustom: {
    oldPage = m_vectorOutsideArea->takeWidget();
    m_vectorOutsideArea->setWidget(
        makeChooserPageWithoutScrollBar(createVectorPage()));
    break;
  }
  case StylePageType::Raster: {
    oldPage = m_rasterOutsideArea->takeWidget();
    m_rasterOutsideArea->setWidget(
        makeChooserPageWithoutScrollBar(createRasterPage()));
    break;
  }
  }
  delete oldPage;

  update();
}

//-----------------------------------------------------------------------------

void StyleEditor::removeStyleSet(StyleChooserPage *styleSetPage) {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  std::vector<StyleChooserPage *> *pages;
  if (tab == 1)
    pages = &m_texturePages;
  else if (tab == 2)
    pages = &m_vectorPages;
  else if (tab == 3)
    pages = &m_rasterPages;

  int i = 1;
  std::vector<StyleChooserPage *>::iterator it;
  for (it = pages->begin() + 1; it != pages->end(); it++) {
    StyleChooserPage *page = *it;
    if (page == styleSetPage) {
      removeStyleSetAtIndex(i, tab);
      break;
    }
    i++;
  }

  TStyleEditorHandler::instance()->updateEditorPage(tab, this);
}

//-----------------------------------------------------------------------------

void StyleEditor::removeStyleSetAtIndex(int index, int pageIndex) {
  if (pageIndex < 1 || pageIndex > 3) return;

  std::vector<StyleChooserPage *> *pages;
  std::vector<ClickableLabel *> *labels;
  std::vector<QPushButton *> *buttons;
  QMenu *menu;
  QScrollArea *outsideArea;

  if (pageIndex == 1) {
    pages       = &m_texturePages;
    labels      = &m_textureLabels;
    buttons     = &m_textureButtons;
    menu        = m_textureMenu;
    outsideArea = m_textureOutsideArea;
  } else if (pageIndex == 2) {
    pages       = &m_vectorPages;
    labels      = &m_vectorLabels;
    buttons     = &m_vectorButtons;
    menu        = m_vectorMenu;
    outsideArea = m_vectorOutsideArea;
  } else if (pageIndex == 3) {
    pages       = &m_rasterPages;
    labels      = &m_rasterLabels;
    buttons     = &m_rasterButtons;
    menu        = m_rasterMenu;
    outsideArea = m_rasterOutsideArea;
  }

  if (index >= pages->size()) return;

  StyleChooserPage *page = *(pages->begin() + index);

  buttons->erase(buttons->begin() + index);
  labels->erase(labels->begin() + index);
  pages->erase(pages->begin() + index);
  menu->removeAction(menu->actions()[index]);

  QWidget *oldPage = outsideArea->takeWidget();
  QFrame *newPageLayout;
  if (pageIndex == 1)
    newPageLayout = createTexturePage();
  else if (pageIndex == 2)
    newPageLayout = createVectorPage();
  else if (pageIndex == 3)
    newPageLayout = createRasterPage();
  outsideArea->setWidget(makeChooserPageWithoutScrollBar(newPageLayout));
  delete oldPage;

  update();
}

//-----------------------------------------------------------------------------

void StyleEditor::editStyleSetName(StyleChooserPage *styleSetPage) {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  std::vector<ClickableLabel *> *labels;
  QScrollArea *scrollArea;
  if (tab == 1) {
    labels     = &m_textureLabels;
    scrollArea = m_textureArea;
  } else if (tab == 2) {
    labels     = &m_vectorLabels;
    scrollArea = m_vectorArea;
  } else if (tab == 3) {
    labels     = &m_rasterLabels;
    scrollArea = m_rasterArea;
  }

  std::vector<ClickableLabel *>::iterator it;
  ClickableLabel *label;
  int i = 0;
  for (it = labels->begin(); it != labels->end(); it++) {
    label = *it;
    if (label->text() == styleSetPage->getStyleSetName()) break;
    i++;
  }

  if (i >= labels->size()) return;
  m_renameStyleSet->setStyleSetPage(styleSetPage);

  QScrollBar *vScrollBar = scrollArea->verticalScrollBar();
  int v                  = vScrollBar->value();
  QRect rect             = label->rect();
  QPoint topLeft         = label->pos() - QPoint(0, v) + QPoint(-4, 19);
  ;
  rect.moveTopLeft(topLeft);
  m_renameStyleSet->show(rect);
}

//-----------------------------------------------------------------------------

void StyleEditor::renameStyleSet(StyleChooserPage *styleSetPage,
                                 QString newName) {
  int tab = m_styleBar->currentIndex();
  if (tab < 1 || tab > 3) return;

  std::vector<ClickableLabel *> *labels;
  QMenu *menu;
  if (tab == 1) {
    labels = &m_textureLabels;
    menu   = m_textureMenu;
  } else if (tab == 2) {
    labels = &m_vectorLabels;
    menu   = m_vectorMenu;
  } else if (tab == 3) {
    labels = &m_rasterLabels;
    menu   = m_rasterMenu;
  }

  std::vector<ClickableLabel *>::iterator it;
  ClickableLabel *label;
  int i = 0;
  for (it = labels->begin(); it != labels->end(); it++) {
    label = *it;
    if (label->text() == styleSetPage->getStyleSetName()) break;
    i++;
  }

  if (i >= labels->size()) return;

  TFilePath newPath =
      styleSetPage->getStylesFolder().withName(newName.toStdWString());

  try {
    TSystem::copyDir(newPath, styleSetPage->getStylesFolder());
    TSystem::rmDirTree(styleSetPage->getStylesFolder());
  } catch (TSystemException se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  }

  if (styleSetPage->isFavorite()) newName += tr(" (Favorites)");
  styleSetPage->changeStyleSetFolder(newPath);
  styleSetPage->setStyleSetName(newName);
  label->setText(newName);

  QWidgetAction *action = qobject_cast<QWidgetAction *>(menu->actions()[i]);
  QCheckBox *checkBox   = dynamic_cast<QCheckBox *>(action->defaultWidget());
  checkBox->setText(newName);

  update();
}

//-----------------------------------------------------------------------------

std::vector<StyleChooserPage *> StyleEditor::getStyleSetList(
    StylePageType pageType) {
  if (pageType == StylePageType::Texture)
    return m_texturePages;
  else if (pageType == StylePageType::VectorBrush ||
           pageType == StylePageType::VectorCustom ||
           pageType == StylePageType::VectorGenerated)
    return m_vectorPages;
  else if (pageType == StylePageType::Raster)
    return m_rasterPages;
}

bool StyleEditor::isStyleNameValid(QString name, StylePageType pageType,
                                   bool isFavorite) {
  TFilePath path, altPath;
  bool checkAltPath = false;
  QFileInfo fi(name);

  if (!isValidFileName(fi.baseName())) {
    error(
        tr("Style Set Name cannot be empty or contain any of the following "
           "characters:\n \\ / : * ? \" < > |"));
    return false;
  }

  if (isReservedFileName_message(fi.baseName())) return false;

  if (isFavorite) {
    path = ToonzFolder::getMyFavoritesFolder() + TFilePath("library");

    if (pageType == StylePageType::Texture)
      path += TFilePath("textures");
    else if (pageType == StylePageType::VectorBrush ||
             pageType == StylePageType::VectorCustom) {
      pageType = StylePageType::VectorCustom;
      path += TFilePath("vector styles");
    } else if (pageType == StylePageType::Raster)
      path += TFilePath("raster styles");
  } else {
    path    = ToonzFolder::getLibraryFolder();
    altPath = path;

    if (pageType == StylePageType::Texture)
      path += TFilePath("textures");
    else if (pageType == StylePageType::VectorBrush) {
      pageType = StylePageType::VectorBrush;
      path += TFilePath("vector brushes");
      altPath += TFilePath("custom styles");
      checkAltPath = true;
    } else if (pageType == StylePageType::VectorCustom) {
      path += TFilePath("custom styles");
      altPath += TFilePath("vector brushes");
      checkAltPath = true;
    } else if (pageType == StylePageType::Raster)
      path += TFilePath("mypaint brushes");
  }

  path += TFilePath(name);
  if (checkAltPath) altPath += TFilePath(name);

  TFileStatus fp(path), altFp(altPath);

  if (fp.doesExist() || (checkAltPath && altFp.doesExist()) ||
      (name == tr("Generated") && (pageType == StylePageType::VectorBrush ||
                                   pageType == StylePageType::VectorCustom))) {
    DVGui::error(tr("Style Set Name already exists. Please try another name."));
    return false;
  }

  return true;
}

//=============================================================================
// NewStyleSetPopup
//-----------------------------------------------------------------------------

NewStyleSetPopup::NewStyleSetPopup(StylePageType pageType, QWidget *parent)
    : Dialog(parent, true, true, "New Style Set"), m_pageType(pageType) {
  m_editor = dynamic_cast<StyleEditor *>(parent);

  setWindowTitle(tr("New Style Set"));
  setFixedWidth(400);

  QLabel *nameLabel = new QLabel(tr("Style Set Name:"), this);
  m_nameFld         = new LineEdit();
  m_isFavorite      = new CheckBox(tr("Create as Favorite"));
  connect(m_isFavorite, SIGNAL(toggled(bool)), this, SLOT(onFavoriteToggled()));

  QLabel *typeLabel            = new QLabel(tr("Style Set Type:"), this);
  QButtonGroup *m_styleSetType = new QButtonGroup(this);

  m_texture = new QRadioButton("Textures");
  m_texture->setEnabled(false);
  if (pageType == StylePageType::Texture) m_texture->setChecked(true);
  m_styleSetType->addButton(m_texture);

  m_vectorCustom = new QRadioButton("Custom Styles");
  m_vectorCustom->setCheckable(true);
  if (pageType != StylePageType::VectorCustom)
    m_vectorCustom->setEnabled(false);
  else
    m_vectorCustom->setChecked(true);
  m_styleSetType->addButton(m_vectorCustom);

  m_vectorBrush = new QRadioButton("Vector Brush");
  m_vectorBrush->setCheckable(true);
  if (pageType != StylePageType::VectorCustom) m_vectorBrush->setEnabled(false);
  m_styleSetType->addButton(m_vectorBrush);

  m_raster = new QRadioButton("Raster");
  m_raster->setEnabled(false);
  if (pageType == StylePageType::Raster) m_raster->setChecked(true);
  m_styleSetType->addButton(m_raster);

  QPushButton *okBtn     = new QPushButton(tr("OK"), this);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(createStyleSet()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(20);
  {
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(okBtn);
    m_buttonLayout->addWidget(cancelBtn);
  }

  //----layout
  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(10);
  {
    QGridLayout *upperLayout = new QGridLayout();
    upperLayout->setMargin(5);
    upperLayout->setHorizontalSpacing(5);
    upperLayout->setVerticalSpacing(10);
    {
      upperLayout->addWidget(nameLabel, 0, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      upperLayout->addWidget(m_nameFld, 0, 1);
      upperLayout->addWidget(m_isFavorite, 1, 1);
      upperLayout->addWidget(typeLabel, 2, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      upperLayout->addWidget(m_texture, 2, 1);
      upperLayout->addWidget(m_vectorCustom, 3, 1);
      upperLayout->addWidget(m_vectorBrush, 4, 1);
      upperLayout->addWidget(m_raster, 5, 1);
    }
    upperLayout->setColumnStretch(0, 0);
    upperLayout->setColumnStretch(1, 1);

    m_topLayout->addLayout(upperLayout);
  }
}

//-----------------------------------------------------------------------------

void NewStyleSetPopup::onFavoriteToggled() {
  if (m_pageType == StylePageType::Texture ||
      m_pageType == StylePageType::Raster)
    return;

  if (m_isFavorite->isChecked()) {
    m_pageType = StylePageType::VectorCustom;
    m_vectorBrush->setChecked(false);
    m_vectorBrush->setDisabled(true);
    m_vectorCustom->setChecked(true);
    m_vectorCustom->setDisabled(true);
  } else {
    m_vectorBrush->setDisabled(false);
    m_vectorCustom->setDisabled(false);
  }
}

//-----------------------------------------------------------------------------

void NewStyleSetPopup::createStyleSet() {
  TFilePath path;
  bool isFavorite = m_isFavorite->isChecked();

  if (!m_editor->isStyleNameValid(m_nameFld->text(), m_pageType, isFavorite)) {
    m_nameFld->setFocus();
    m_nameFld->selectAll();
    return;
  }

  if (isFavorite) {
    path = ToonzFolder::getMyFavoritesFolder() + TFilePath("library");

    if (m_texture->isChecked())
      path += TFilePath("textures");
    else if (m_vectorBrush->isChecked() || m_vectorCustom->isChecked()) {
      m_pageType = StylePageType::VectorCustom;
      path += TFilePath("vector styles");
    } else if (m_raster->isChecked())
      path += TFilePath("raster styles");
  } else {
    path = ToonzFolder::getLibraryFolder();

    if (m_texture->isChecked())
      path += TFilePath("textures");
    else if (m_vectorBrush->isChecked()) {
      m_pageType = StylePageType::VectorBrush;
      path += TFilePath("vector brushes");
    } else if (m_vectorCustom->isChecked()) {
      path += TFilePath("custom styles");
    } else if (m_raster->isChecked())
      path += TFilePath("mypaint brushes");
  }

  path += TFilePath(m_nameFld->text());

  m_editor->createNewStyleSet(m_pageType, path, isFavorite);

  int pageIndex;
  if (m_pageType == StylePageType::Texture)
    pageIndex = 1;
  else if (m_pageType == StylePageType::Raster)
    pageIndex = 3;
  else
    pageIndex = 2;
  TStyleEditorHandler::instance()->updateEditorPage(pageIndex, m_editor);

  accept();
}

//=============================================================================
// ClickableLabel
//-----------------------------------------------------------------------------

ClickableLabel::ClickableLabel(const QString &text, QWidget *parent,
                               Qt::WindowFlags f)
    : QLabel(text, parent, f) {}

//-----------------------------------------------------------------------------

ClickableLabel::~ClickableLabel() {}

//-----------------------------------------------------------------------------

void ClickableLabel::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::RightButton) return;
  emit click();
}

//=============================================================================
// RenameStyleSet
//-----------------------------------------------------------------------------

RenameStyleSet::RenameStyleSet(QWidget *parent) : QLineEdit(parent) {
  m_editor = dynamic_cast<StyleEditor *>(parent);

  setFixedSize(200, 20);
  connect(this, SIGNAL(returnPressed()), SLOT(renameSet()));
}

//-----------------------------------------------------------------------------

void RenameStyleSet::show(const QRect &rect) {
  if (!m_page) return;

  move(rect.topLeft());
  QString fontName = Preferences::instance()->getInterfaceFont();
  if (fontName == "") {
#ifdef _WIN32
    fontName = "Arial";
#else
    fontName = "Helvetica";
#endif
  }
  static QFont font(fontName, -1, QFont::Normal);
  setFont(font);

  QString name = m_page->getStyleSetName().replace(tr(" (Favorites)"), "");

  setText(name);
  selectAll();

  m_validatingName = false;
  QWidget::show();
  raise();
  setFocus();
}

//-----------------------------------------------------------------------------

void RenameStyleSet::renameSet() {
  if (!m_page ||
      text() == m_page->getStyleSetName().replace(tr(" (Favorites)"), "")) {
    setText("");
    hide();
    return;
  }

  m_validatingName = true;
  bool isNameValid = m_editor->isStyleNameValid(text(), m_page->getPageType(),
                                                m_page->isFavorite());
  m_validatingName = false;

  if (!isNameValid) {
    setText(m_page->getStyleSetName().replace(tr(" (Favorites)"), ""));
    setFocus();
    selectAll();
    return;
  }

  QString newName = text();

  setText("");
  hide();

  m_editor->renameStyleSet(m_page, newName);
}

//-----------------------------------------------------------------------------

void RenameStyleSet::focusOutEvent(QFocusEvent *e) {
  if (!m_validatingName) {
    std::wstring newName = text().toStdWString();
    if (!newName.empty())
      renameSet();
    else
      hide();
  }

  QLineEdit::focusOutEvent(e);
}
