#pragma once

#ifndef BRUSHTIPMANAGER_H
#define BRUSHTIPMANAGER_H

#include "tfilepath.h"
#include "traster.h"
#include "tthread.h"
#include "tlevel.h"

#include "toonz/toonzfolders.h"

#include <QList>
#include <QSize>
#include <QString>
#include <QImage>

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

#define DEFAULTBRUSHTIPID "defaultBrushTip"

//********************************************************************************
//    BrushTipManager declaration
//********************************************************************************

class DVAPI BrushTipData final : public TSmartObject {
public:
  QImage *m_image;
  QString m_brushTipName;
  QString m_brushTipFolderName;
  TFilePath m_path;
  std::string m_idName;  // brush id name
  TLevelP m_level;
  std::vector<QPolygonF> m_imageContour;

  BrushTipData()
      : m_image(0)
      , m_brushTipName("")
      , m_brushTipFolderName("")
      , m_path(TFilePath())
      , m_idName("")
      , m_level(0)
      , m_imageContour(0) {}

  BrushTipData(BrushTipData &src)
      : m_image(src.m_image)
      , m_brushTipName(src.m_brushTipName)
      , m_brushTipFolderName(src.m_brushTipFolderName)
      , m_path(src.m_path)
      , m_idName(src.m_idName)
      , m_level(src.m_level)
      , m_imageContour(src.m_imageContour) {}

  ~BrushTipData() {}

  int getLevelFrameCount() { return m_level->getFrameCount(); }

  TImageP getImage(int index) {
    TLevel::Iterator frameIt = m_level->begin();
    int i                    = index % getLevelFrameCount();
    std::advance(frameIt, i);
    return frameIt->second;
  }
};

// singleton
class DVAPI TBrushTipManager final : public QObject {
  Q_OBJECT

  TFilePath m_brushTipFolder;
  QString m_filters;
  QSize m_chipSize;
  QImage *m_defaultBrushTipImage;

  QList<BrushTipData *> m_brushTips;

  TThread::Executor m_executor;
  bool m_started;
  std::vector<TFilePath> m_activeLoads;
  int m_itemsLoaded;

  bool m_isIndexed;
  QList<int> m_indexes;
  QString m_searchText;

  TBrushTipManager() {
    TFilePath libraryPath  = ToonzFolder::getLibraryFolder();
    m_brushTipFolder       = libraryPath + TFilePath("brush tips");
    m_filters              = "*.tif *.png *.tga *.tiff *.rgb *.bmp";
    m_chipSize             = QSize(64, 64);
    m_defaultBrushTipImage = new QImage(":Resources/default_brushtip.png");
  }

public:
  static TBrushTipManager *instance() {
    static TBrushTipManager theInstance;
    return &theInstance;
  }

  ~TBrushTipManager() {}

  QSize getChipSize() { return m_chipSize; }
  QImage *getDefaultBrushTipImage() { return m_defaultBrushTipImage; }

  void loadItems();
  void loadItemFinished(TFilePath file);
  bool isLoading() { return m_activeLoads.size() > 0; }

  void signalLoadsFinished() { emit itemsUpdated(); }

  void addBrushTip(BrushTipData *data) { m_brushTips.push_back(data); }

  int getBrushTipCount();
  BrushTipData *getBrushTip(int index);
  BrushTipData *getBrushTipById(std::string id);

  void applyFilter();
  void applyFilter(const QString text) {
    m_searchText = text;
    applyFilter();
  }
  QString getSearchText() const { return m_searchText; }

signals:
  void itemsUpdated();
};

#endif  // BRUSHTIPMANAGER_H
