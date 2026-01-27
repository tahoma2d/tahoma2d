
#include <streambuf>

#include <QStandardPaths>

#include "tstroke.h"
#include "tfilepath_io.h"
#include "timage_io.h"
#include "trop.h"
#include "tsystem.h"
#include "tvectorimage.h"
#include "tpixelutils.h"
#include "tstrokeprop.h"

#include "toonz/toonzscene.h"

#include "toonz/mypaintbrushstyle.h"

#include "tw/stringtable.h"

#include <QDebug>

#include <sstream>

//*************************************************************************************
//    TMyPaintBrushStyle  implementation
//*************************************************************************************

TMyPaintBrushStyle::TMyPaintBrushStyle() {}

//-----------------------------------------------------------------------------

TMyPaintBrushStyle::TMyPaintBrushStyle(const TFilePath &path) {
  loadBrush(path);
}

//-----------------------------------------------------------------------------

TMyPaintBrushStyle::TMyPaintBrushStyle(const TMyPaintBrushStyle &other)
    : TColorStyle(other)
    , m_path(other.m_path)
    , m_fullpath(other.m_fullpath)
    , m_brushOriginal(other.m_brushOriginal)
    , m_brushModified(other.m_brushModified)
    , m_preview(other.m_preview)
    , m_color(other.m_color)
    , m_modifiedData(other.m_modifiedData) {}

//-----------------------------------------------------------------------------

TMyPaintBrushStyle::~TMyPaintBrushStyle() {}

//-----------------------------------------------------------------------------

TColorStyle *TMyPaintBrushStyle::clone(std::string brushIdName) const {
  TMyPaintBrushStyle *style = new TMyPaintBrushStyle(*this);
  style->loadBrush(TFilePath(getBrushIdNameParam(brushIdName)));
  return style;
}

//-----------------------------------------------------------------------------

TColorStyle &TMyPaintBrushStyle::copy(const TColorStyle &other) {
  const TMyPaintBrushStyle *otherBrushStyle =
      dynamic_cast<const TMyPaintBrushStyle *>(&other);
  if (otherBrushStyle) {
    m_path          = otherBrushStyle->m_path;
    m_fullpath      = otherBrushStyle->m_fullpath;
    m_brushOriginal = otherBrushStyle->m_brushOriginal;
    m_brushModified = otherBrushStyle->m_brushModified;
    m_preview       = otherBrushStyle->m_preview;
    m_modifiedData  = otherBrushStyle->m_modifiedData;
  }
  assignBlend(other, other, 0.0);
  return *this;
}

//-----------------------------------------------------------------------------

QString TMyPaintBrushStyle::getDescription() const {
  return "MyPaintBrushStyle";
}

//-----------------------------------------------------------------------------

std::string TMyPaintBrushStyle::getBrushIdName() const {
  std::wstring ws = m_path.getWideString();
  const std::string s(ws.begin(), ws.end());
  return "MyPaintBrushStyle:" + s;
}

//-----------------------------------------------------------------------------

std::string TMyPaintBrushStyle::getBrushType() { return "myb"; }

//-----------------------------------------------------------------------------

TFilePathSet TMyPaintBrushStyle::getBrushesDirs() {
  TFilePathSet paths;
  paths.push_back(m_libraryDir + "mypaint brushes");
  QStringList genericLocations =
      QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
  for (QStringList::iterator i = genericLocations.begin();
       i != genericLocations.end(); ++i)
    paths.push_back(TFilePath(*i) + "mypaint" + "brushes");
  return paths;
}

//-----------------------------------------------------------------------------

TFilePath TMyPaintBrushStyle::decodePath(const TFilePath &path) const {
  if (path.isAbsolute()) return path;

  if (m_currentScene) {
    TFilePath p = m_currentScene->decodeFilePath(path);
    TFileStatus fs(p);
    if (fs.doesExist() && !fs.isDirectory()) return p;
  }

  TFilePathSet paths = getBrushesDirs();
  for (TFilePathSet::iterator i = paths.begin(); i != paths.end(); ++i) {
    TFilePath p = *i + path;
    TFileStatus fs(p);
    if (fs.doesExist() && !fs.isDirectory()) return p;
  }

  return path;
}

//-----------------------------------------------------------------------------
static std::string mybToVersion3(std::string origStr) {
  std::string outStr   = "";
  std::string comment  = "";
  bool settingsStarted = false;

  std::stringstream strstream(origStr);

  outStr += "{\n";
  for (std::string line; std::getline(strstream, line);) {
    std::replace(line.begin(), line.end(), '\r', ' ');
    if (line.find("version 2") != std::string::npos)
      continue;  // Ignore version.  We'll add version back later as version 3

    if (line[0] == '#') {
      comment = comment + line;
      continue;
    }

    if (!settingsStarted) {
      settingsStarted = true;
      outStr += "    \"comment\": \"" + comment + "\",\n";
      outStr += "    \"group\": \"\",\n";
      outStr += "    \"parent_brush_name\": \"\",\n";
      outStr += "    \"settings\": {\n";
    } else
      outStr += ",\n";

    int startPos = 0;
    int pipe     = line.find("|");

    std::string settingInfo = line.substr(startPos, pipe);
    std::string setting     = settingInfo.substr(startPos, line.find(" "));
    std::string baseValue =
        settingInfo.substr(setting.length(), settingInfo.find_last_not_of(" "));
    outStr += "        \"" + setting + "\": {\n";
    outStr += "            \"base_value\": " + baseValue + ",\n";
    if (pipe == std::string::npos)
      outStr += "            \"inputs\": {}";
    else {
      outStr += "            \"inputs\": {\n";
      while (pipe != line.length()) {
        if (startPos > 0) outStr += ",\n";
        startPos = pipe + 1;
        pipe     = line.find("|", startPos);
        if (pipe == std::string::npos) pipe = line.length();
        settingInfo      = line.substr(startPos, pipe - startPos);
        int firstCharPos = settingInfo.find_first_not_of(" ");
        setting          = settingInfo.substr(firstCharPos,
                                              settingInfo.find(" ", firstCharPos) - 1);
        outStr += "                \"" + setting + "\": [\n";
        firstCharPos = settingInfo.find_first_not_of(" ", setting.length() + 1);
        baseValue    = settingInfo.substr(firstCharPos, pipe - startPos);
        int comma    = baseValue.find(", ");
        if (comma == std::string::npos) comma = baseValue.length();
        std::string value = baseValue.substr(0, comma);
        std::replace(value.begin(), value.end(), '(', '[');
        std::replace(value.begin(), value.end(), ')', ']');
        std::replace(value.begin(), value.end(), ' ', ',');
        outStr += "                    " + value;
        int prevComma;
        while (comma != baseValue.length()) {
          outStr += ",\n";
          prevComma = comma + 2;
          comma     = baseValue.find(", ", prevComma);
          if (comma == std::string::npos) comma = baseValue.length();
          value = baseValue.substr(prevComma, comma - prevComma);
          std::replace(value.begin(), value.end(), '(', '[');
          std::replace(value.begin(), value.end(), ')', ']');
          std::replace(value.begin(), value.end(), ' ', ',');
          outStr += "                    " + value;
        }
        outStr += "\n                ]";  // Ends Input setting
      }
      outStr += "\n            }";  // End Inputs
    }
    outStr += "\n        }";  // End Setting
  }
  outStr += "\n";

  if (settingsStarted) {
    outStr += "    },\n";  // End Settings
    outStr += "    \"version\": 3\n";
  }

  outStr += "}";  // Ends file
  return outStr;
}

void TMyPaintBrushStyle::loadBrush(const TFilePath &path) {
  m_path     = path;
  m_fullpath = decodePath(path);
  m_brushOriginal.fromDefaults();

  Tifstream is(m_fullpath);
  if (is) {
    std::string str;
    str.assign(std::istreambuf_iterator<char>(is),
               std::istreambuf_iterator<char>());
    if (str.find("version 2") != std::string::npos) str = mybToVersion3(str);
    m_brushOriginal.fromString(str);
  }

  m_brushModified = m_brushOriginal;
  std::map<MyPaintBrushSetting, ModifiedBrushData> modifiedDataCopy;
  modifiedDataCopy.swap(m_modifiedData);
  for (std::map<MyPaintBrushSetting, ModifiedBrushData>::const_iterator i =
           modifiedDataCopy.begin();
       i != modifiedDataCopy.end(); ++i)
    setBaseValue(i->first, i->second.m_baseValue);

  TFilePath preview_path =
      m_fullpath.getParentDir() + (m_fullpath.getWideName() + L"_prev.png");
  TImageReader::load(preview_path, m_preview);

  invalidateIcon();
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setBaseValue(MyPaintBrushSetting id, bool enable,
                                      float value) {
  float def = m_brushOriginal.getBaseValue(id);
  if (enable && fabsf(value - def) > 0.01) {
    m_modifiedData[id].m_baseValue = value;
    m_brushModified.setBaseValue(id, value);
  } else {
    if (m_modifiedData.find(id) != m_modifiedData.end()) {
      // Delete mapping for id only if there is no modified curve data
      if (!m_modifiedData[id].m_mappingN.size())
        m_modifiedData.erase(id);
      else
        m_modifiedData[id].m_baseValue = def;
    }
    m_brushModified.setBaseValue(id, def);
  }
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::resetBaseValues() {
  for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i)
    setBaseValueEnabled((MyPaintBrushSetting)i, false);
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setMappingN(MyPaintBrushSetting id,
                                     MyPaintBrushInput input, bool enable,
                                     int value) {
  int def = m_brushOriginal.getMappingN(id, input);
  if (enable) {
    // If mapping for id does not exist, add mapping entry with default base
    // value
    if (m_modifiedData.find(id) == m_modifiedData.end())
      m_modifiedData[id].m_baseValue = getBaseValue(id);

    m_modifiedData[id].m_mappingN[input] = value;
    m_brushModified.setMappingN(id, input, value);
  } else {
    if (m_modifiedData.find(id) != m_modifiedData.end()) {
      m_modifiedData[id].m_mappingN.erase(input);

      // Delete mapping for id when the last input is removed and if the base
      // value matches the default base value
      float defBase = m_brushOriginal.getBaseValue(id);
      if (!m_modifiedData[id].m_mappingN.size() &&
          m_modifiedData[id].m_baseValue == defBase)
        m_modifiedData.erase(id);
    }
    m_brushModified.setMappingN(id, input, def);
  }
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setMappingPoint(MyPaintBrushSetting id,
                                         MyPaintBrushInput input, int index,
                                         bool enable, TPointD pt) {
  if (enable) {
    // If mapping for id does not exist, add mapping entry with default base
    // value
    if (m_modifiedData.find(id) == m_modifiedData.end())
      m_modifiedData[id].m_baseValue = getBaseValue(id);

    // If mapping input for id doesn't exist or the value is less than current
    // index, then add/update it
    if (m_modifiedData[id].m_mappingN.find(input) ==
            m_modifiedData[id].m_mappingN.end() ||
        m_modifiedData[id].m_mappingN[input] < index)
      m_modifiedData[id].m_mappingN[input] = index;

    m_modifiedData[id].m_mappingPoints[{input, index}] = pt;
    m_brushModified.setMappingPoint(id, input, index, pt.x, pt.y);
  } else {
    if (m_modifiedData.find(id) != m_modifiedData.end()) {
      m_modifiedData[id].m_mappingPoints.erase({input, index});
    }
    int n = m_brushOriginal.getMappingN(id, input);
    if (index < n) {
      float x, y;
      m_brushOriginal.getMappingPoint(id, input, index, x, y);
      m_brushModified.setMappingPoint(id, input, index, x, y);
    }
  }
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::resetMapping() {
  for (int i = 0; i < MYPAINT_BRUSH_SETTINGS_COUNT; ++i) {
    for (int j = 0; j < MYPAINT_BRUSH_INPUTS_COUNT; ++j)
      resetMapping((MyPaintBrushSetting)i, (MyPaintBrushInput)j);
  }
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::resetMapping(MyPaintBrushSetting id) {
  for (int j = 0; j < MYPAINT_BRUSH_INPUTS_COUNT; ++j)
    resetMapping(id, (MyPaintBrushInput)j);
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::resetMapping(MyPaintBrushSetting id,
                                      MyPaintBrushInput input) {
  int n = getMappingN(id, input);
  setMappingNEnabled(id, input, false);
  for (int k = 0; k < n; k++) setMappingPointEnabled(id, input, k, false);

  n = getDefaultMappingN(id, input);
  for (int k = 0; k < n; k++) setMappingPointEnabled(id, input, k, false);
}

//-----------------------------------------------------------------------------

int TMyPaintBrushStyle::getMappingN(MyPaintBrushSetting id,
                                    MyPaintBrushInput input) const {
  std::map<MyPaintBrushSetting, ModifiedBrushData>::const_iterator i =
      m_modifiedData.find(id);
  return i == m_modifiedData.end() ||
                 i->second.m_mappingN.find(input) == i->second.m_mappingN.end()
             ? m_brushOriginal.getMappingN(id, input)
             : i->second.m_mappingN.at(input);
}

//-----------------------------------------------------------------------------

int TMyPaintBrushStyle::getDefaultMappingN(MyPaintBrushSetting id,
                                           MyPaintBrushInput input) const {
  return m_brushOriginal.getMappingN(id, input);
}

//-----------------------------------------------------------------------------

TPointD TMyPaintBrushStyle::getMappingPoint(MyPaintBrushSetting id,
                                            MyPaintBrushInput input,
                                            int index) const {
  std::map<MyPaintBrushSetting, ModifiedBrushData>::const_iterator i =
      m_modifiedData.find(id);

  if (i == m_modifiedData.end() ||
      i->second.m_mappingPoints.find({input, index}) ==
          i->second.m_mappingPoints.end()) {
    int n = m_brushOriginal.getMappingN(id, input);
    if (index >= n) return TPointD(0, 0);

    float x, y;
    m_brushOriginal.getMappingPoint(id, input, index, x, y);
    return TPointD(x, y);
  }
  return i->second.m_mappingPoints.at({input, index});
}

//-----------------------------------------------------------------------------

TPointD TMyPaintBrushStyle::getDefaultMappingPoint(MyPaintBrushSetting id,
                                                   MyPaintBrushInput input,
                                                   int index) const {
  float x, y;
  m_brushOriginal.getMappingPoint(id, input, index, x, y);
  return TPointD(x, y);
}

//-----------------------------------------------------------------------------

QString TMyPaintBrushStyle::getInputName(MyPaintBrushInput input) const {
  switch ((MyPaintBrushInput)input) {
  case MYPAINT_BRUSH_INPUT_PRESSURE:
    return QObject::tr("Pressure");
  case MYPAINT_BRUSH_INPUT_SPEED1:
    return QObject::tr("Fine Speed");
  case MYPAINT_BRUSH_INPUT_SPEED2:
    return QObject::tr("Gross Speed");
  case MYPAINT_BRUSH_INPUT_RANDOM:
    return QObject::tr("Random");
  case MYPAINT_BRUSH_INPUT_STROKE:
    return QObject::tr("Stroke");
  case MYPAINT_BRUSH_INPUT_DIRECTION:
    return QObject::tr("Direction");
  case MYPAINT_BRUSH_INPUT_DIRECTION_ANGLE:
    return QObject::tr("Direction 360");
  case MYPAINT_BRUSH_INPUT_ATTACK_ANGLE:
    return QObject::tr("Attack Angle");
  case MYPAINT_BRUSH_INPUT_TILT_DECLINATION:
    return QObject::tr("Declination");
  case MYPAINT_BRUSH_INPUT_TILT_ASCENSION:
    return QObject::tr("Ascension");
  case MYPAINT_BRUSH_INPUT_GRIDMAP_X:
    return QObject::tr("GridMap X");
  case MYPAINT_BRUSH_INPUT_GRIDMAP_Y:
    return QObject::tr("GridMap Y");
  case MYPAINT_BRUSH_INPUT_BRUSH_RADIUS:
    return QObject::tr("Base Brush Radius");
  case MYPAINT_BRUSH_INPUT_CUSTOM:
    return QObject::tr("Custom");
  default:
    break;
  }

  return QString::fromStdString(mypaint::Input::byId(input).name);
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::getInputRange(MyPaintBrushInput input, double &min,
                                       double &max) const {
  min = mypaint::Input::byId(input).softMin;
  max = mypaint::Input::byId(input).softMax;
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::makeIcon(const TDimension &d) {
  TFilePath path =
      m_fullpath.getParentDir() + (m_fullpath.getWideName() + L"_prev.png");
  TPointD offset(0, 0);
  if (!m_preview) {
    m_icon = TRaster32P(d);
    m_icon->fill(TPixel32::Red);
  } else if (m_preview->getSize() == d) {
    m_icon = m_preview;
  } else {
    m_icon = TRaster32P(d);
    if (d.lx != d.ly) {
      TPixel32 col = getMainColor();
      if (col.m == 255)
        m_icon->fill(col);
      else {
        TRaster32P fg(d);
        fg->fill(premultiply(col));
        TRop::checkBoard(m_icon, TPixel32::Black, TPixel32::White,
                         TDimensionD(6, 6), TPointD());
        TRop::over(m_icon, fg);
      }
    }
    double sx    = (double)d.lx / (double)m_preview->getLx();
    double sy    = (double)d.ly / (double)m_preview->getLy();
    double scale = std::min(sx, sy);
    TRaster32P resamplePreview(m_preview->getLx(), m_preview->getLy());
    TRop::resample(resamplePreview, m_preview, TScale(scale),
                   TRop::ResampleFilterType::Hamming3);
    TRop::over(m_icon, resamplePreview);
  }

  // paint color marker
  // Only show color marker when the icon size is 22x22
  if (d.lx == d.ly && d.lx <= 22) {
    int size       = std::min(1 + std::min(d.lx, d.ly) * 2 / 3,
                              1 + std::max(d.lx, d.ly) / 2);
    TPixel32 color = getMainColor();
    color.m        = 255;  // show full opac color
    for (int y = 0; y < size; ++y) {
      TPixel32 *p    = m_icon->pixels(d.ly - y - 1);
      TPixel32 *endp = p + size - y - 1;
      for (; p != endp; ++p) *p = color;
      *p = blend(*p, color, 0.5);
    }
  }
}

//-----------------------------------------------------------------

TStrokeProp *TMyPaintBrushStyle::makeStrokeProp(const TStroke *stroke) {
  // For Vector rendering, treat MyPaint brushes as a solid stroke
  TSolidColorStyle *color = new TSolidColorStyle();
  color->addRef();
  color->setMainColor(getAverageColor());
  return new OutlineStrokeProp(stroke, color);
}

//-----------------------------------------------------------------

TRegionProp *TMyPaintBrushStyle::makeRegionProp(const TRegion *region) {
  TSolidColorStyle *color = new TSolidColorStyle();
  color->addRef();
  color->setMainColor(getAverageColor());
  return color->makeRegionProp(region);
}

//------------------------------------------------------------

void TMyPaintBrushStyle::loadData(TInputStreamInterface &is) {
  std::string path;
  is >> path;
  is >> m_color;
  loadBrush(TFilePath(path));

  int baseSettingsCount = 0;
  is >> baseSettingsCount;

  const mypaint::Setting *setting;
  const mypaint::Input *input;
  int index = 0;
  TPointD pt;
  for (int i = 0; i < baseSettingsCount; ++i) {
    std::string key;
    double value    = 0.0;
    int inputsCount = 0;
    is >> key;
    is >> value;
    if (key == "x") {
      if (!setting || !input) continue;
      pt.x = value;
    } else if (key == "y") {
      if (!setting || !input) continue;
      pt.y = value;
      setMappingPoint(setting->id, input->id, index++, pt);
    } else if (input = mypaint::Input::findByKey(key)) {
      setMappingN(setting->id, input->id, (int)value);
      index = 0;
    } else if (setting = mypaint::Setting::findByKey(key)) {
      setBaseValue(setting->id, value);
      index = 0;
    }
  }
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::saveData(TOutputStreamInterface &os) const {
  std::wstring wstr = m_path.getWideString();
  std::string str;
  str.assign(wstr.begin(), wstr.end());
  os << str;
  os << m_color;

  int x = (int)m_modifiedData.size();

  std::map<MyPaintBrushSetting, ModifiedBrushData>::const_iterator i;
  std::map<MyPaintBrushInput, int>::const_iterator mi;

  // Precalculate curve information
  for (i = m_modifiedData.begin(); i != m_modifiedData.end(); i++) {
    std::map<MyPaintBrushInput, int>::const_iterator mi;
    for (mi = i->second.m_mappingN.begin(); mi != i->second.m_mappingN.end();
         mi++) {
      // 1 for input name and point count (ie pressure 2)
      // 2 for each individual point w/label (ie x 0 y 0)
      x += 1 + (mi->second * 2);
    }
  }

  os << x;  // Count of pairs of data that follows

  for (i = m_modifiedData.begin(); i != m_modifiedData.end(); ++i) {
    // Parameter and value
    os << mypaint::Setting::byId(i->first).key;
    os << (double)i->second.m_baseValue;
    // Curve data
    for (mi = i->second.m_mappingN.begin(); mi != i->second.m_mappingN.end();
         mi++) {
      os << mypaint::Input::byId(mi->first).key;
      os << (double)mi->second;

      for (int index = 0; index < mi->second; index++) {
        TPointD pt = i->second.m_mappingPoints.at({mi->first, index});
        os << "x";
        os << pt.x;
        os << "y";
        os << pt.y;
      }
    }
  }
}

//-----------------------------------------------------------------------------

int TMyPaintBrushStyle::getParamCount() const {
  return MYPAINT_BRUSH_SETTINGS_COUNT;
}

//-----------------------------------------------------------------------------

QString TMyPaintBrushStyle::getParamNames(int index) const {
  switch ((MyPaintBrushSetting)index) {
  case MYPAINT_BRUSH_SETTING_OPAQUE:
    return QObject::tr("Opacity");
  case MYPAINT_BRUSH_SETTING_OPAQUE_MULTIPLY:
    return QObject::tr("Opacity multiply");
  case MYPAINT_BRUSH_SETTING_OPAQUE_LINEARIZE:
    return QObject::tr("Opacity linearize");
  case MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC:
    return QObject::tr("Radius");
  case MYPAINT_BRUSH_SETTING_HARDNESS:
    return QObject::tr("Hardness");
  case MYPAINT_BRUSH_SETTING_ANTI_ALIASING:
    return QObject::tr("Pixel feather");
  case MYPAINT_BRUSH_SETTING_DABS_PER_BASIC_RADIUS:
    return QObject::tr("Dabs per basic radius");
  case MYPAINT_BRUSH_SETTING_DABS_PER_ACTUAL_RADIUS:
    return QObject::tr("Dabs per actual radius");
  case MYPAINT_BRUSH_SETTING_DABS_PER_SECOND:
    return QObject::tr("Dabs per second");
  case MYPAINT_BRUSH_SETTING_GRIDMAP_SCALE:
    return QObject::tr("GridMap Scale");
  case MYPAINT_BRUSH_SETTING_GRIDMAP_SCALE_X:
    return QObject::tr("GridMap Scale X");
  case MYPAINT_BRUSH_SETTING_GRIDMAP_SCALE_Y:
    return QObject::tr("GridMap Scale Y");
  case MYPAINT_BRUSH_SETTING_RADIUS_BY_RANDOM:
    return QObject::tr("Radius by random");
  case MYPAINT_BRUSH_SETTING_SPEED1_SLOWNESS:
    return QObject::tr("Fine speed filter");
  case MYPAINT_BRUSH_SETTING_SPEED2_SLOWNESS:
    return QObject::tr("Gross speed filter");
  case MYPAINT_BRUSH_SETTING_SPEED1_GAMMA:
    return QObject::tr("Fine speed gamma");
  case MYPAINT_BRUSH_SETTING_SPEED2_GAMMA:
    return QObject::tr("Gross speed gamma");
  case MYPAINT_BRUSH_SETTING_OFFSET_BY_RANDOM:
    return QObject::tr("Jitter");
  case MYPAINT_BRUSH_SETTING_OFFSET_Y:
    return QObject::tr("Offset Y");
  case MYPAINT_BRUSH_SETTING_OFFSET_X:
    return QObject::tr("Offset X");
  case MYPAINT_BRUSH_SETTING_OFFSET_ANGLE:
    return QObject::tr("Angular Offset: Direction");
  case MYPAINT_BRUSH_SETTING_OFFSET_ANGLE_ASC:
    return QObject::tr("Angular Offset: Ascension");
  case MYPAINT_BRUSH_SETTING_OFFSET_ANGLE_2:
    return QObject::tr("Angular Offset Mirrored: Direction");
  case MYPAINT_BRUSH_SETTING_OFFSET_ANGLE_2_ASC:
    return QObject::tr("Angular Offset Mirrored: Ascension");
  case MYPAINT_BRUSH_SETTING_OFFSET_ANGLE_ADJ:
    return QObject::tr("Angular Offsets Adjustment");
  case MYPAINT_BRUSH_SETTING_OFFSET_MULTIPLIER:
    return QObject::tr("Offsets Multiplier");
  case MYPAINT_BRUSH_SETTING_OFFSET_BY_SPEED:
    return QObject::tr("Offset by speed");
  case MYPAINT_BRUSH_SETTING_OFFSET_BY_SPEED_SLOWNESS:
    return QObject::tr("Offset by speed filter");
  case MYPAINT_BRUSH_SETTING_SLOW_TRACKING:
    return QObject::tr("Slow position tracking");
  case MYPAINT_BRUSH_SETTING_SLOW_TRACKING_PER_DAB:
    return QObject::tr("Slow tracking per dab");
  case MYPAINT_BRUSH_SETTING_TRACKING_NOISE:
    return QObject::tr("Tracking noise");
  case MYPAINT_BRUSH_SETTING_COLOR_H:
    return QObject::tr("Color hue");
  case MYPAINT_BRUSH_SETTING_COLOR_S:
    return QObject::tr("Color saturation");
  case MYPAINT_BRUSH_SETTING_COLOR_V:
    return QObject::tr("Color value");
  case MYPAINT_BRUSH_SETTING_RESTORE_COLOR:
    return QObject::tr("Save color");
  case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_H:
    return QObject::tr("Change color hue");
  case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_L:
    return QObject::tr("Change color lightness (HSL)");
  case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSL_S:
    return QObject::tr("Change color satur. (HSL)");
  case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_V:
    return QObject::tr("Change color value (HSV)");
  case MYPAINT_BRUSH_SETTING_CHANGE_COLOR_HSV_S:
    return QObject::tr("Change color satur. (HSV)");
  case MYPAINT_BRUSH_SETTING_SMUDGE:
    return QObject::tr("Smudge");
  case MYPAINT_BRUSH_SETTING_SMUDGE_LENGTH:
    return QObject::tr("Smudge length");
  case MYPAINT_BRUSH_SETTING_SMUDGE_RADIUS_LOG:
    return QObject::tr("Smudge radius");
  case MYPAINT_BRUSH_SETTING_ERASER:
    return QObject::tr("Eraser");
  case MYPAINT_BRUSH_SETTING_STROKE_THRESHOLD:
    return QObject::tr("Stroke threshold");
  case MYPAINT_BRUSH_SETTING_STROKE_DURATION_LOGARITHMIC:
    return QObject::tr("Stroke duration");
  case MYPAINT_BRUSH_SETTING_STROKE_HOLDTIME:
    return QObject::tr("Stroke hold time");
  case MYPAINT_BRUSH_SETTING_CUSTOM_INPUT:
    return QObject::tr("Custom input");
  case MYPAINT_BRUSH_SETTING_CUSTOM_INPUT_SLOWNESS:
    return QObject::tr("Custom input filter");
  case MYPAINT_BRUSH_SETTING_ELLIPTICAL_DAB_RATIO:
    return QObject::tr("Elliptical dab: ratio");
  case MYPAINT_BRUSH_SETTING_ELLIPTICAL_DAB_ANGLE:
    return QObject::tr("Elliptical dab: angle");
  case MYPAINT_BRUSH_SETTING_DIRECTION_FILTER:
    return QObject::tr("Direction filter");
  case MYPAINT_BRUSH_SETTING_LOCK_ALPHA:
    return QObject::tr("Lock alpha");
  case MYPAINT_BRUSH_SETTING_COLORIZE:
    return QObject::tr("Colorize");
  case MYPAINT_BRUSH_SETTING_SNAP_TO_PIXEL:
    return QObject::tr("Snap to pixel");
  case MYPAINT_BRUSH_SETTING_PRESSURE_GAIN_LOG:
    return QObject::tr("Pressure gain");
  default:
    break;
  }

  return QString::fromStdString(
      mypaint::Setting::byId((MyPaintBrushSetting)index).name);
}

//-----------------------------------------------------------------------------

TColorStyle::ParamType TMyPaintBrushStyle::getParamType(int index) const {
  return DOUBLE;
}

//-----------------------------------------------------------------------------

bool TMyPaintBrushStyle::hasParamDefault(int index) const { return true; }

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setParamDefault(int index) {
  setBaseValueEnabled((MyPaintBrushSetting)index, false);
}

//-----------------------------------------------------------------------------

bool TMyPaintBrushStyle::isParamDefault(int index) const {
  return getBaseValueEnabled((MyPaintBrushSetting)index);
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::getParamRange(int index, double &min,
                                       double &max) const {
  const mypaint::Setting &setting =
      mypaint::Setting::byId((MyPaintBrushSetting)index);
  min = setting.min;
  max = setting.max;
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::setParamValue(int index, double value) {
  setBaseValue((MyPaintBrushSetting)index, value);
}

//-----------------------------------------------------------------------------

double TMyPaintBrushStyle::getParamValue(double_tag, int index) const {
  return getBaseValue((MyPaintBrushSetting)index);
}

//-----------------------------------------------------------------------------

bool TMyPaintBrushStyle::isMappingDefault(MyPaintBrushSetting id) const {
  for (int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; i++) {
    if (getMappingNEnabled(id, (MyPaintBrushInput)i)) return true;
  }

  return false;
}

//-----------------------------------------------------------------------------

void TMyPaintBrushStyle::resetStyle() {
  resetBaseValues();
  resetMapping();
}

//-----------------------------------------------------------------------------

namespace {
TColorStyle::Declaration mypaintBrushStyle(new TMyPaintBrushStyle());
}
