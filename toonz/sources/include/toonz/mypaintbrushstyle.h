#pragma once

#ifndef MYPAINTBRUSHSTYLE_H
#define MYPAINTBRUSHSTYLE_H

#include "mypaint.h"

// TnzCore includes
#include "imagestyles.h"

#undef DVAPI
#undef DVVAR

#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//**********************************************************************************
//    TMyPaintBrushStyle declaration
//**********************************************************************************

class DVAPI TMyPaintBrushStyle final : public TColorStyle, TImageStyle {
private:
  TFilePath m_path;
  TFilePath m_fullpath;
  mypaint::Brush m_brushOriginal;
  mypaint::Brush m_brushModified;
  TRasterP m_preview;
  TPixel32 m_color;

  typedef struct {
    float m_baseValue;
    std::map<MyPaintBrushInput, int> m_mappingN;
    std::map<std::pair<MyPaintBrushInput, int>, TPointD> m_mappingPoints;
  } ModifiedBrushData;

  std::map<MyPaintBrushSetting, ModifiedBrushData> m_modifiedData;

  TFilePath decodePath(const TFilePath &path) const;
  void loadBrush(const TFilePath &path);

public:
  TMyPaintBrushStyle();
  TMyPaintBrushStyle(const TFilePath &path);
  TMyPaintBrushStyle(const TMyPaintBrushStyle &other);
  ~TMyPaintBrushStyle();

  TColorStyle *clone() const override { return new TMyPaintBrushStyle(*this); }
  TColorStyle *clone(std::string brushIdName) const override;

  TColorStyle &copy(const TColorStyle &other) override;

  static std::string getBrushType();
  static TFilePathSet getBrushesDirs();

  const TFilePath &getPath() const { return m_path; }
  const mypaint::Brush &getBrush() const { return m_brushModified; }
  const TRasterP &getPreview() const { return m_preview; }

  TStrokeProp *makeStrokeProp(const TStroke * /* stroke */) override;
  TRegionProp *makeRegionProp(const TRegion * /* region */) override;

  bool isRegionStyle() const override { return true; }
  bool isStrokeStyle() const override { return true; }

  bool hasMainColor() const override { return true; }
  TPixel32 getMainColor() const override { return m_color; }
  void setMainColor(const TPixel32 &color) override { m_color = color; }

  int getTagId() const override { return 4001; }

  QString getDescription() const override;
  std::string getBrushIdName() const override;

  void setBaseValue(MyPaintBrushSetting id, bool enable, float value);
  void resetBaseValues();

  void setBaseValue(MyPaintBrushSetting id, float value) {
    setBaseValue(id, true, value);
  }

  void setBaseValueEnabled(MyPaintBrushSetting id, bool enable) {
    setBaseValue(id, enable, getBaseValue(id));
  }

  void setMappingN(MyPaintBrushSetting id, MyPaintBrushInput input, bool enable,
                   int value);
  void resetMapping();
  void resetMapping(MyPaintBrushSetting id);
  void resetMapping(MyPaintBrushSetting id, MyPaintBrushInput input);

  void setMappingN(MyPaintBrushSetting id, MyPaintBrushInput input, int value) {
    setMappingN(id, input, true, value);
  }

  void setMappingNEnabled(MyPaintBrushSetting id, MyPaintBrushInput input,
                          bool enable) {
    setMappingN(id, input, enable, getMappingN(id, input));
  }

  void setMappingPoint(MyPaintBrushSetting id, MyPaintBrushInput input,
                       int index, bool enable, TPointD pt);

  void setMappingPoint(MyPaintBrushSetting id, MyPaintBrushInput input,
                       int index, TPointD pt) {
    setMappingPoint(id, input, index, true, pt);
  }

  void setMappingPointEnabled(MyPaintBrushSetting id, MyPaintBrushInput input,
                              int index, bool enable) {
    setMappingPoint(id, input, index, enable,
                    getMappingPoint(id, input, index));
  }

  const std::map<MyPaintBrushSetting, ModifiedBrushData> getBaseValues() const {
    return m_modifiedData;
  }

  float getBaseValue(MyPaintBrushSetting id) const {
    std::map<MyPaintBrushSetting, ModifiedBrushData>::const_iterator i =
        m_modifiedData.find(id);
    return i == m_modifiedData.end() ? m_brushOriginal.getBaseValue(id)
                                     : i->second.m_baseValue;
  }

  float getDefaultBaseValue(MyPaintBrushSetting id) const {
    return m_brushOriginal.getBaseValue(id);
  }

  bool getBaseValueEnabled(MyPaintBrushSetting id) const {
    std::map<MyPaintBrushSetting, ModifiedBrushData>::const_iterator i =
        m_modifiedData.find(id);
    return i != m_modifiedData.end();
  }

  int getMappingN(MyPaintBrushSetting id, MyPaintBrushInput input) const;

  int getDefaultMappingN(MyPaintBrushSetting id, MyPaintBrushInput input) const;

  bool getMappingNEnabled(MyPaintBrushSetting id,
                          MyPaintBrushInput input) const {
    std::map<MyPaintBrushSetting, ModifiedBrushData>::const_iterator i =
        m_modifiedData.find(id);
    return i != m_modifiedData.end() &&
           i->second.m_mappingN.find(input) != i->second.m_mappingN.end();
  }

  TPointD getMappingPoint(MyPaintBrushSetting id, MyPaintBrushInput input,
                          int index) const;

  TPointD getDefaultMappingPoint(MyPaintBrushSetting id,
                                 MyPaintBrushInput input, int index) const;

  bool getMappingPointEnabled(MyPaintBrushSetting id, MyPaintBrushInput input,
                              int index) const {
    std::map<MyPaintBrushSetting, ModifiedBrushData>::const_iterator i =
        m_modifiedData.find(id);

    return (i != m_modifiedData.end() &&
            i->second.m_mappingPoints.find({input, index}) !=
                i->second.m_mappingPoints.end());
  }

  QString getInputName(MyPaintBrushInput input) const;
  void getInputRange(MyPaintBrushInput input, double &min, double &max) const;

  int getParamCount() const override;
  QString getParamNames(int index) const override;
  ParamType getParamType(int index) const override;
  bool hasParamDefault(int index) const override;
  void setParamDefault(int index) override;
  bool isParamDefault(int index) const override;
  void getParamRange(int index, double &min, double &max) const override;
  void setParamValue(int index, double value) override;
  double getParamValue(double_tag, int index) const override;

  bool isMappingDefault(MyPaintBrushSetting id) const;

  void resetStyle();

protected:
  void makeIcon(const TDimension &d) override;
  void loadData(TInputStreamInterface &) override;
  void saveData(TOutputStreamInterface &) const override;
};

#endif
