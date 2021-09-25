#pragma once

#ifndef STYLEMANAGER_H
#define STYLEMANAGER_H

#include "tfilepath.h"
#include "tthread.h"
#include "traster.h"

#include "toonz/mypaintbrushstyle.h"

#include <QSize>
#include <QList>
#include <QString>

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//-------------------------------------------------------------------------

//  Forward declarations
class QImage;

//-------------------------------------------------------------------------

//********************************************************************************
//    CustomStyleManager declaration
//********************************************************************************

class DVAPI CustomStyleManager final : public QObject {
  Q_OBJECT

public:
  struct DVAPI PatternData {
    QImage *m_image;
    std::string m_patternName;
    bool m_isVector;
    bool m_isGenerated;
    TFilePath m_path;

    PatternData()
        : m_image(0)
        , m_patternName("")
        , m_isVector(false)
        , m_isGenerated(false)
        , m_path(TFilePath()) {}
  };

  class StyleLoaderTask;
  friend class CustomStyleManager::StyleLoaderTask;

private:
  QList<PatternData> m_patterns;
  TFilePath m_stylesFolder;
  QString m_filters;
  QSize m_chipSize;

  TThread::Executor m_executor;
  bool m_started;
  std::vector<TFilePath> m_activeLoads;
  int m_itemsLoaded;

public:
  CustomStyleManager(const TFilePath &stylesFolder, QString filters = QString(),
                     QSize chipSize = QSize(30, 30));

  void setStyleFolder(TFilePath styleFolder);
  const TFilePath &stylesFolder() const { return m_stylesFolder; }
  QSize getChipSize() const { return m_chipSize; }
  QString getFilters() const { return m_filters; }

  int getPatternCount();
  PatternData getPattern(int index);

  void loadItems();

  void loadItemFinished(TFilePath file);

  void loadGeneratedStyle(TFilePath file);

  bool isLoading() { return m_activeLoads.size() > 0; }
  bool hasLoadedItems() { return m_itemsLoaded > 0; }
  void signalLoadDone() {
    m_itemsLoaded = 0;
    emit itemsUpdated();
  }

private:
  void addPattern(const TFilePath &path);

signals:

  void itemsUpdated();
};

//********************************************************************************
//    TextureStyleManager declaration
//********************************************************************************

class DVAPI TextureStyleManager final : public QObject {
  Q_OBJECT

public:
  struct DVAPI TextureData {
    TRaster32P m_raster;
    std::string m_textureName;
    TFilePath m_path;

    TextureData() : m_raster(0), m_textureName(""), m_path(TFilePath()) {}
  };

private:
  QList<TextureData> m_textures;
  TFilePath m_stylesFolder;
  QString m_filters;
  QSize m_chipSize;

public:
  TextureStyleManager(const TFilePath &stylesFolder,
                      QString filters = QString(),
                      QSize chipSize  = QSize(30, 30));

  void setStyleFolder(TFilePath styleFolder);
  const TFilePath &stylesFolder() const { return m_stylesFolder; }
  QSize getChipSize() const { return m_chipSize; }
  QString getFilters() const { return m_filters; }

  int getTextureCount();
  TextureData getTexture(int index);

  void loadTexture(TFilePath &fp);
  void loadItems();

private:
  void addTexture(const TFilePath &path);

signals:

  void itemsUpdated();
};

//********************************************************************************
//    BrushStyleManager declaration
//********************************************************************************

class DVAPI BrushStyleManager final : public QObject {
  Q_OBJECT

public:
  struct DVAPI BrushData {
    TMyPaintBrushStyle m_brush;
    std::string m_brushName;
    TFilePath m_path;

    BrushData() : m_brush(), m_brushName(""), m_path(TFilePath()) {}
  };

private:
  QList<BrushData> m_brushes;
  TFilePath m_stylesFolder;
  QString m_filters;
  QSize m_chipSize;

public:
  BrushStyleManager(const TFilePath &stylesFolder, QString filters = QString(),
                    QSize chipSize = QSize(30, 30));

  void setStyleFolder(TFilePath styleFolder);
  const TFilePath &stylesFolder() const { return m_stylesFolder; }
  QSize getChipSize() const { return m_chipSize; }
  QString getFilters() const { return m_filters; }

  int getBrushCount();
  BrushData getBrush(int index);

  void loadItems();

signals:

  void itemsUpdated();
};

//********************************************************************************
//    StyleManager declaration
//********************************************************************************

// singleton
class DVAPI TStyleManager {
  std::vector<std::pair<TFilePath, QString>> m_customStyleFolders;
  std::vector<CustomStyleManager *> m_customStyleManagers;

  std::vector<std::pair<TFilePath, QString>> m_textureStyleFolders;
  std::vector<TextureStyleManager *> m_textureStyleManagers;

  std::vector<std::pair<TFilePath, QString>> m_brushStyleFolders;
  std::vector<BrushStyleManager *> m_brushStyleManagers;

  TStyleManager() {}

public:
  static TStyleManager *instance() {
    static TStyleManager theInstance;
    return &theInstance;
  }

  ~TStyleManager() {}

  CustomStyleManager *getCustomStyleManager(TFilePath stylesFolder,
                                            QString filters = QString("*"),
                                            QSize chipSize  = QSize(30, 30));

  TextureStyleManager *getTextureStyleManager(TFilePath stylesFolder,
                                              QString filters = QString("*"),
                                              QSize chipSize  = QSize(30, 30));

  BrushStyleManager *getBrushStyleManager(TFilePath stylesFolder,
                                          QString filters = QString("*"),
                                          QSize chipSize  = QSize(30, 30));

  TFilePathSet getCustomStyleFolders();
  TFilePathSet getTextureStyleFolders();
  TFilePathSet getBrushStyleFolders();

  void removeCustomStyleFolder(TFilePath styleFolder);
  void removeTextureStyleFolder(TFilePath styleFolder);
  void removeBrushStyleFolder(TFilePath styleFolder);

  bool isLoading();
  void signalLoadsFinished();

  void changeStyleSetFolder(CustomStyleManager *styleManager,
                            TFilePath newPath);
  void changeStyleSetFolder(TextureStyleManager *styleManager,
                            TFilePath newPath);
  void changeStyleSetFolder(BrushStyleManager *styleManager, TFilePath newPath);
};

#endif  // STYLEMANAGER_H
