

// Toonz includes
#include "tvectorimage.h"
#include "trasterimage.h"
#include "tlevel_io.h"
#include "tofflinegl.h"
#include "tropcm.h"
#include "tvectorrenderdata.h"
#include "tsystem.h"
#include "tvectorgl.h"
#include "traster.h"
#include "tcolorstyles.h"

#include "toonzqt/gutil.h"

#include "toonz/imagestyles.h"
#include "toonz/mypaintbrushstyle.h"

// Qt includes
#include <QDir>
#include <QImage>
#include <QOffscreenSurface>
#include <QThread>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>

#include "toonz/stylemanager.h"

//********************************************************************************
//    Local namespace stuff
//********************************************************************************

namespace {

void convertRaster32ToImage(TRaster32P ras, QImage *image) {
  int lx = ras->getLx();
  int ly = ras->getLy();
  int i, j;
  ras->lock();
  for (i = 0; i < lx; i++)
    for (j = 0; j < ly; j++) {
      TPixel32 pix = ras->pixels(ly - 1 - j)[i];
      QRgb value;
      value = qRgba(pix.r, pix.g, pix.b, pix.m);
      image->setPixel(i, j, value);
    }
  ras->unlock();
}

}  // namespace

//********************************************************************************
//    StyleLoaderTask definition
//********************************************************************************

class CustomStyleManager::StyleLoaderTask final : public TThread::Runnable {
  CustomStyleManager *m_manager;
  TFilePath m_fp;
  PatternData m_data;
  std::shared_ptr<QOffscreenSurface> m_offScreenSurface;

public:
  StyleLoaderTask(CustomStyleManager *manager, const TFilePath &fp);

  void run() override;

  void onFinished(TThread::RunnableP sender) override;
};

//-----------------------------------------------------------------------------

CustomStyleManager::StyleLoaderTask::StyleLoaderTask(
    CustomStyleManager *manager, const TFilePath &fp)
    : m_manager(manager), m_fp(fp) {
  connect(this, SIGNAL(finished(TThread::RunnableP)), this,
          SLOT(onFinished(TThread::RunnableP)));

  if (QThread::currentThread() == qGuiApp->thread()) {
    m_offScreenSurface.reset(new QOffscreenSurface());
    m_offScreenSurface->setFormat(QSurfaceFormat::defaultFormat());
    m_offScreenSurface->create();
  }
}

//-----------------------------------------------------------------------------

void CustomStyleManager::StyleLoaderTask::run() {
  try {
    // Fetch the level
    TLevelReaderP lr(m_fp);
    TLevelP level = lr->loadInfo();
    if (!level || level->getFrameCount() == 0) return;

    // Fetch the image of the first frame in the level
    TLevel::Iterator frameIt = level->begin();
    if (frameIt == level->end()) return;
    TImageP img = lr->getFrameReader(frameIt->first)->load();

    // Process the image
    const QSize &qChipSize = m_manager->getChipSize();
    TDimension chipSize(qChipSize.width(), qChipSize.height());

    TVectorImageP vimg = img;
    TRasterImageP rimg = img;

    TRaster32P ras;

    QImage *image = nullptr;

    if (vimg) {
      assert(level->getPalette());
      TPalette *vPalette = level->getPalette();
      assert(vPalette);
      vimg->setPalette(vPalette);

#ifdef LINUX
	  TOfflineGL *glContext = 0;
	  glContext = TOfflineGL::getStock(chipSize);
	  glContext->clear(TPixel32::White);
#else
      QOpenGLContext *glContext = new QOpenGLContext();
      if (QOpenGLContext::currentContext())
        glContext->setShareContext(QOpenGLContext::currentContext());
      glContext->setFormat(QSurfaceFormat::defaultFormat());
      glContext->create();
      glContext->makeCurrent(m_offScreenSurface.get());
      // attaching stencil buffer here as some styles use it
      QOpenGLFramebufferObject fb(
          chipSize.lx, chipSize.ly,
          QOpenGLFramebufferObject::CombinedDepthStencil);

      fb.bind();
      // Draw
      glViewport(0, 0, chipSize.lx, chipSize.ly);
      glClearColor(1.0, 1.0, 1.0, 1.0);  // clear with white color
      glClear(GL_COLOR_BUFFER_BIT);

      glMatrixMode(GL_PROJECTION);
      glLoadIdentity();
      gluOrtho2D(0, chipSize.lx, 0, chipSize.ly);

      glMatrixMode(GL_MODELVIEW);
      glLoadIdentity();
#endif

      TRectD bbox = img->getBBox();
      double scx  = 0.8 * chipSize.lx / bbox.getLx();
      double scy  = 0.8 * chipSize.ly / bbox.getLy();
      double sc   = std::min(scx, scy);
      double dx   = 0.5 * chipSize.lx;
      double dy   = 0.5 * chipSize.ly;

      TAffine aff =
          TTranslation(dx, dy) * TScale(sc) *
          TTranslation(-0.5 * (bbox.x0 + bbox.x1), -0.5 * (bbox.y0 + bbox.y1));
      TVectorRenderData rd(aff, chipSize, vPalette, 0, true);

#ifdef LINUX
	  glContext->draw(img, rd);
	  // No need to clone! The received raster already is a copy of the	
	  // context's buffer	
	  ras = glContext->getRaster();  //->clone();
#else
      tglDraw(rd, vimg.getPointer());

      image = new QImage(fb.toImage().scaled(QSize(chipSize.lx, chipSize.ly),
                                             Qt::IgnoreAspectRatio,
                                             Qt::SmoothTransformation));
      fb.release();
      glContext->deleteLater();
#endif
    } else if (rimg) {
      TDimension size = rimg->getRaster()->getSize();
      if (size == chipSize)
        ras = rimg->getRaster()->clone();  // Yep, this may be necessary
      else {
        TRaster32P rout(chipSize);

        TRop::resample(rout, rimg->getRaster(),
                       TScale((double)chipSize.lx / size.lx,
                              (double)chipSize.ly / size.ly));

        TRop::addBackground(rout, TPixel::White);
        ras = rout;
      }
#ifndef LINUX
      image = new QImage(chipSize.lx, chipSize.ly, QImage::Format_RGB32);
      convertRaster32ToImage(ras, image);
#endif
    } else
      assert(!"unsupported type for custom styles!");

#ifdef LINUX
	image = new QImage(chipSize.lx, chipSize.ly, QImage::Format_RGB32);
	convertRaster32ToImage(ras, image);
#endif

    m_data.m_path        = m_fp;
    m_data.m_patternName = m_fp.getName();
    m_data.m_isVector    = (m_fp.getType() == "pli" || m_fp.getType() == "svg");
    m_data.m_image       = image;
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------

void CustomStyleManager::StyleLoaderTask::onFinished(
    TThread::RunnableP sender) {
  // On the main thread...
  if (m_data.m_image)  // Everything went ok
  {
    m_manager->m_patterns.push_back(m_data);
  }
  m_manager->loadItemFinished(m_data.m_path);
}

//********************************************************************************
//    CustomStyleManager implementation
//********************************************************************************

CustomStyleManager::CustomStyleManager(const TFilePath &stylesFolder,
                                       QString filters, QSize chipSize)
    : m_stylesFolder(stylesFolder), m_filters(filters), m_chipSize(chipSize) {
  m_executor.setMaxActiveTasks(1);
}

//-----------------------------------------------------------------------------

void CustomStyleManager::loadItemFinished(TFilePath file) {
  std::vector<TFilePath>::iterator it =
      std::find(m_activeLoads.begin(), m_activeLoads.end(), file);
  if (it != m_activeLoads.end()) m_activeLoads.erase(it);
  m_itemsLoaded++;
  if (!m_activeLoads.size() && !TStyleManager::instance()->isLoading())
    TStyleManager::instance()->signalLoadsFinished();
};

//-----------------------------------------------------------------------------

int CustomStyleManager::getPatternCount() { return m_patterns.size(); }

//-----------------------------------------------------------------------------

CustomStyleManager::PatternData CustomStyleManager::getPattern(int index) {
  return (index < 0 || index >= m_patterns.size()) ? PatternData()
                                                   : m_patterns[index];
}

//-----------------------------------------------------------------------------

void CustomStyleManager::loadItems() {
  // Build the folder to be read
  if (m_stylesFolder == TFilePath()) return;

  QDir patternDir(QString::fromStdWString(m_stylesFolder.getWideString()));
  patternDir.setNameFilters(m_filters.split(' '));

  // Read the said folder
  TFilePathSet fps;
  try {
    TSystem::readDirectory(fps, patternDir);
  } catch (...) {
    return;
  }

  // Delete patterns no longer in the folder
  TFilePathSet newFps;
  TFilePathSet::iterator it;
  int i;
  bool patternsUpdated = false;
  for (i = 0; i < m_patterns.size(); i++) {
    PatternData data = m_patterns.at(i);
    for (it = fps.begin(); it != fps.end(); ++it) {
      if (data.m_path.getLevelName() == it->getLevelName()) break;
    }

    if (it == fps.end()) {
      m_patterns.removeAt(i);
      i--;
      patternsUpdated = true;
    } else
      fps.erase(it);  // The style is not new, so don't generate tasks for it
  }

  // For each (now new) file entry, generate a fetching task
  // NOTE: after all adds have finished, a separate itemsUpdated() signal is
  // emitted
  for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++) {
    TFilePath file = *it;
    // bogus file for internally generated styles
    if (file.getType() == "gen") {
      loadGeneratedStyle(file);
      patternsUpdated = true;
    } else {
      std::vector<TFilePath>::iterator it =
          std::find(m_activeLoads.begin(), m_activeLoads.end(), file);
      if (it != m_activeLoads.end()) continue;
      m_activeLoads.push_back(file);
      m_executor.addTask(new StyleLoaderTask(this, file));
    }
  }

  if (patternsUpdated && !m_activeLoads.size()) emit itemsUpdated();
}

//-----------------------------------------------------------------------------

void CustomStyleManager::loadGeneratedStyle(TFilePath file) {
  PatternData pattern;

  QString name          = QString::fromStdString(file.getName());
  QStringList nameParts = name.split("-");
  int tagId             = std::stoi(nameParts[1].toStdString());

  TColorStyle *style = TColorStyle::create(tagId);
  TDimension chipSize(m_chipSize.width(), m_chipSize.height());
  QImage *image =
      new QImage(m_chipSize.width(), m_chipSize.height(), QImage::Format_RGB32);

  convertRaster32ToImage(style->getIcon(chipSize), image);

  pattern.m_path        = file;
  pattern.m_patternName = nameParts[0].toStdString();
  pattern.m_isGenerated = true;
  pattern.m_image       = image;

  m_patterns.push_back(pattern);
}

//-----------------------------------------------------------------------------

void CustomStyleManager::setStyleFolder(TFilePath styleFolder) {
  m_stylesFolder = styleFolder;

  for (int i = 0; i < m_patterns.size(); i++)
    m_patterns[i].m_path =
        styleFolder + TFilePath(m_patterns[i].m_path.getLevelName());
}

//********************************************************************************
//    TextureStyleManager implementation
//********************************************************************************

TextureStyleManager::TextureStyleManager(const TFilePath &stylesFolder,
                                         QString filters, QSize chipSize)
    : m_stylesFolder(stylesFolder), m_filters(filters), m_chipSize(chipSize) {}

//-----------------------------------------------------------------------------

int TextureStyleManager::getTextureCount() { return m_textures.size(); }

//-----------------------------------------------------------------------------

TextureStyleManager::TextureData TextureStyleManager::getTexture(int index) {
  return (index < 0 || index >= m_textures.size()) ? TextureData()
                                                   : m_textures[index];
}

//-----------------------------------------------------------------------------

void TextureStyleManager::loadItems() {
  // Build the folder to be read

  if (m_stylesFolder == TFilePath()) return;

  QDir patternDir(QString::fromStdWString(m_stylesFolder.getWideString()));
  patternDir.setNameFilters(m_filters.split(' '));

  // Read the said folder
  TFilePathSet fps;
  try {
    TSystem::readDirectory(fps, patternDir);
  } catch (...) {
    return;
  }

  // Delete textures no longer in the folder
  TFilePathSet newFps;
  TFilePathSet::iterator it;
  int i;
  bool texturesUpdated = false;
  for (i = 0; i < m_textures.size(); i++) {
    TextureData data = m_textures.at(i);
    for (it = fps.begin(); it != fps.end(); ++it) {
      if (data.m_path.getLevelName() == it->getLevelName()) break;
    }

    if (it == fps.end()) {
      // Custom style is always removed and added back again later
      // Don't treat it as a refresh
      if (m_textures[i].m_path != TFilePath()) texturesUpdated = true;
      m_textures.removeAt(i);
      i--;
    } else
      fps.erase(it);  // The style is not new, so don't generate tasks for it
  }

  // For each (now new) file entry, load it
  for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++) {
    loadTexture(*it);
    texturesUpdated = true;
  }

  TFilePath *empty = new TFilePath();
  loadTexture(*empty);  // custom texture

  if (texturesUpdated) emit itemsUpdated();
}

//-----------------------------------------------------------------------------

void TextureStyleManager::loadTexture(TFilePath &fp) {
  if (fp == TFilePath()) {
    TRaster32P ras(25, 25);
    TTextureStyle::fillCustomTextureIcon(ras);
    TextureData customText;
    customText.m_raster      = ras;
    customText.m_textureName = std::string("");
    customText.m_path        = fp;

    m_textures.push_back(customText);
    return;
  }

  TRasterP ras;
  TImageReader::load(fp, ras);
  if (!ras || ras->getLx() < 2 || ras->getLy() < 2) return;

  TRaster32P ras32 = ras;
  if (!ras32) return;

  TDimension d(2, 2);
  while (d.lx < 256 && d.lx * 2 <= ras32->getLx()) d.lx *= 2;
  while (d.ly < 256 && d.ly * 2 <= ras32->getLy()) d.ly *= 2;

  TRaster32P texture;
  if (d == ras32->getSize())
    texture = ras32;
  else {
    texture = TRaster32P(d);
    TScale sc((double)texture->getLx() / ras32->getLx(),
              (double)texture->getLy() / ras32->getLy());
    TRop::resample(texture, ras32, sc);
  }

  TextureData text;
  text.m_raster      = texture;
  text.m_textureName = fp.getName();
  text.m_path        = fp;

  m_textures.push_back(text);
}

//-----------------------------------------------------------------------------

void TextureStyleManager::setStyleFolder(TFilePath styleFolder) {
  m_stylesFolder = styleFolder;

  for (int i = 0; i < m_textures.size(); i++)
    m_textures[i].m_path =
        styleFolder + TFilePath(m_textures[i].m_path.getLevelName());
}

//********************************************************************************
//    BrushStyleManager implementation
//********************************************************************************

BrushStyleManager::BrushStyleManager(const TFilePath &stylesFolder,
                                     QString filters, QSize chipSize)
    : m_stylesFolder(stylesFolder), m_filters(filters), m_chipSize(chipSize) {}

//-----------------------------------------------------------------------------

int BrushStyleManager::getBrushCount() { return m_brushes.size(); }

//-----------------------------------------------------------------------------

BrushStyleManager::BrushData BrushStyleManager::getBrush(int index) {
  return (index < 0 || index >= m_brushes.size()) ? BrushData()
                                                  : m_brushes[index];
}

//-----------------------------------------------------------------------------

void BrushStyleManager::loadItems() {
  // Build the folder to be read

  if (m_stylesFolder == TFilePath()) return;

  QDir patternDir(QString::fromStdWString(m_stylesFolder.getWideString()));
  patternDir.setNameFilters(m_filters.split(' '));

  // Read the said folder
  TFilePathSet fps;
  try {
    TSystem::readDirectory(fps, patternDir);
  } catch (...) {
    return;
  }

  // Delete brushes no longer in the folder
  TFilePathSet newFps;
  TFilePathSet::iterator it;
  int i;
  bool brushesUpdated = false;
  for (i = 0; i < m_brushes.size(); i++) {
    BrushData data = m_brushes.at(i);
    for (it = fps.begin(); it != fps.end(); ++it) {
      if (data.m_path.getLevelName() == it->getLevelName()) break;
    }

    if (it == fps.end()) {
      m_brushes.removeAt(i);
      i--;
      brushesUpdated = true;
    } else
      fps.erase(it);  // The style is not new, so don't generate tasks for it
  }

  // For each (now new) file entry, load it now
  for (TFilePathSet::iterator it = fps.begin(); it != fps.end(); it++) {
    BrushData brush;
    brush.m_brush     = TMyPaintBrushStyle(*it);
    brush.m_brushName = it->getName();
    brush.m_path      = *it;

    m_brushes.push_back(brush);
    brushesUpdated = true;
  }

  if (brushesUpdated) emit itemsUpdated();
}

//-----------------------------------------------------------------------------

void BrushStyleManager::setStyleFolder(TFilePath styleFolder) {
  m_stylesFolder = styleFolder;

  for (int i = 0; i < m_brushes.size(); i++)
    m_brushes[i].m_path =
        styleFolder + TFilePath(m_brushes[i].m_path.getLevelName());
}

//********************************************************************************
//    StyleManager definition
//********************************************************************************

//---------------------------------------------------------

CustomStyleManager *TStyleManager::getCustomStyleManager(TFilePath stylesFolder,
                                                         QString filters,
                                                         QSize chipSize) {
  std::pair<TFilePath, QString> styleFolderKey =
      std::pair<TFilePath, QString>(stylesFolder, filters);

  // Return the manager if it was previously created
  for (int index = 0; index < m_customStyleFolders.size(); index++) {
    if (m_customStyleFolders[index] == styleFolderKey)
      return m_customStyleManagers[index];
  }

  // Create the manager if one was not found
  CustomStyleManager *cm =
      new CustomStyleManager(stylesFolder, filters, chipSize);
  m_customStyleManagers.push_back(cm);
  m_customStyleFolders.push_back(styleFolderKey);

  return cm;
}

//---------------------------------------------------------

TextureStyleManager *TStyleManager::getTextureStyleManager(
    TFilePath stylesFolder, QString filters, QSize chipSize) {
  std::pair<TFilePath, QString> styleFolderKey =
      std::pair<TFilePath, QString>(stylesFolder, filters);

  // Return the manager if it was previously created
  for (int index = 0; index < m_textureStyleFolders.size(); index++) {
    if (m_textureStyleFolders[index] == styleFolderKey)
      return m_textureStyleManagers[index];
  }

  // Create the manager if one was not found
  TextureStyleManager *tm =
      new TextureStyleManager(stylesFolder, filters, chipSize);
  m_textureStyleManagers.push_back(tm);
  m_textureStyleFolders.push_back(styleFolderKey);

  return tm;
}

//---------------------------------------------------------

BrushStyleManager *TStyleManager::getBrushStyleManager(TFilePath stylesFolder,
                                                       QString filters,
                                                       QSize chipSize) {
  std::pair<TFilePath, QString> styleFolderKey =
      std::pair<TFilePath, QString>(stylesFolder, filters);

  // Return the manager if it was previously created
  for (int index = 0; index < m_brushStyleFolders.size(); index++) {
    if (m_brushStyleFolders[index] == styleFolderKey)
      return m_brushStyleManagers[index];
  }

  // Create the manager if one was not found
  BrushStyleManager *rm =
      new BrushStyleManager(stylesFolder, filters, chipSize);
  m_brushStyleManagers.push_back(rm);
  m_brushStyleFolders.push_back(styleFolderKey);

  return rm;
}

//---------------------------------------------------------

bool TStyleManager::isLoading() {
  std::vector<CustomStyleManager *>::iterator it;
  for (it = m_customStyleManagers.begin(); it != m_customStyleManagers.end();
       it++) {
    CustomStyleManager *cm = *it;
    if (cm->isLoading()) return true;
  }

  return false;
}

//---------------------------------------------------------

void TStyleManager::signalLoadsFinished() {
  std::vector<CustomStyleManager *>::iterator it;
  for (it = m_customStyleManagers.begin(); it != m_customStyleManagers.end();
       it++) {
    CustomStyleManager *cm = *it;
    if (cm->hasLoadedItems()) cm->signalLoadDone();
  }
}

//---------------------------------------------------------

TFilePathSet TStyleManager::getCustomStyleFolders() {
  TFilePathSet fps;

  std::vector<CustomStyleManager *>::iterator it;
  for (it = m_customStyleManagers.begin(); it != m_customStyleManagers.end();
       it++) {
    CustomStyleManager *cm = *it;
    fps.push_back(cm->stylesFolder());
  }

  return fps;
}

//---------------------------------------------------------

TFilePathSet TStyleManager::getTextureStyleFolders() {
  TFilePathSet fps;

  std::vector<TextureStyleManager *>::iterator it;
  for (it = m_textureStyleManagers.begin(); it != m_textureStyleManagers.end();
       it++) {
    TextureStyleManager *tm = *it;
    fps.push_back(tm->stylesFolder());
  }

  return fps;
}

//---------------------------------------------------------

TFilePathSet TStyleManager::getBrushStyleFolders() {
  TFilePathSet fps;

  std::vector<BrushStyleManager *>::iterator it;
  for (it = m_brushStyleManagers.begin(); it != m_brushStyleManagers.end();
       it++) {
    BrushStyleManager *rm = *it;
    fps.push_back(rm->stylesFolder());
  }

  return fps;
}

//---------------------------------------------------------

void TStyleManager::removeCustomStyleFolder(TFilePath styleFolder) {
  std::vector<std::pair<TFilePath, QString>>::iterator it;
  int i = 0;
  for (int i = 0; i < m_customStyleFolders.size(); i++) {
    std::pair<TFilePath, QString> fpInfo = m_customStyleFolders[i];
    if (fpInfo.first != styleFolder) continue;
    m_customStyleFolders.erase(m_customStyleFolders.begin() + i);
    m_customStyleManagers.erase(m_customStyleManagers.begin() + i);
    break;
  }
}

//---------------------------------------------------------

void TStyleManager::removeTextureStyleFolder(TFilePath styleFolder) {
  std::vector<std::pair<TFilePath, QString>>::iterator it;
  int i = 0;
  for (int i = 0; i < m_textureStyleFolders.size(); i++) {
    std::pair<TFilePath, QString> fpInfo = m_textureStyleFolders[i];
    if (fpInfo.first != styleFolder) continue;
    m_textureStyleFolders.erase(m_textureStyleFolders.begin() + i);
    m_textureStyleManagers.erase(m_textureStyleManagers.begin() + i);
    break;
  }
}

//---------------------------------------------------------

void TStyleManager::removeBrushStyleFolder(TFilePath styleFolder) {
  std::vector<std::pair<TFilePath, QString>>::iterator it;
  int i = 0;
  for (int i = 0; i < m_brushStyleFolders.size(); i++) {
    std::pair<TFilePath, QString> fpInfo = m_brushStyleFolders[i];
    if (fpInfo.first != styleFolder) continue;
    m_brushStyleFolders.erase(m_brushStyleFolders.begin() + i);
    m_brushStyleManagers.erase(m_brushStyleManagers.begin() + i);
    break;
  }
}

//---------------------------------------------------------

void TStyleManager::changeStyleSetFolder(CustomStyleManager *styleManager,
                                         TFilePath newPath) {
  std::pair<TFilePath, QString> oldKey(styleManager->stylesFolder(),
                                       styleManager->getFilters());
  std::pair<TFilePath, QString> newKey(newPath, styleManager->getFilters());

  std::vector<std::pair<TFilePath, QString>>::iterator it;
  int i;
  for (it = m_customStyleFolders.begin(); it != m_customStyleFolders.end();
       it++) {
    if (*it == oldKey) {
      m_customStyleFolders.erase(it);
      break;
    }
    i++;
  }

  if (i < m_customStyleManagers.size())
    m_customStyleManagers.erase(m_customStyleManagers.begin() + i);

  styleManager->setStyleFolder(newPath);
  m_customStyleFolders.push_back(newKey);
  m_customStyleManagers.push_back(styleManager);
  styleManager->loadItems();
}

//---------------------------------------------------------

void TStyleManager::changeStyleSetFolder(TextureStyleManager *styleManager,
                                         TFilePath newPath) {
  std::pair<TFilePath, QString> oldKey(styleManager->stylesFolder(),
                                       styleManager->getFilters());
  std::pair<TFilePath, QString> newKey(newPath, styleManager->getFilters());

  std::vector<std::pair<TFilePath, QString>>::iterator it;
  int i;
  for (it = m_textureStyleFolders.begin(); it != m_textureStyleFolders.end();
       it++) {
    if (*it == oldKey) {
      m_textureStyleFolders.erase(it);
      break;
    }
    i++;
  }

  if (i < m_textureStyleManagers.size())
    m_textureStyleManagers.erase(m_textureStyleManagers.begin() + i);

  styleManager->setStyleFolder(newPath);
  m_textureStyleFolders.push_back(newKey);
  m_textureStyleManagers.push_back(styleManager);
  styleManager->loadItems();
}

//---------------------------------------------------------

void TStyleManager::changeStyleSetFolder(BrushStyleManager *styleManager,
                                         TFilePath newPath) {
  std::pair<TFilePath, QString> oldKey(styleManager->stylesFolder(),
                                       styleManager->getFilters());
  std::pair<TFilePath, QString> newKey(newPath, styleManager->getFilters());

  std::vector<std::pair<TFilePath, QString>>::iterator it;
  int i;
  for (it = m_brushStyleFolders.begin(); it != m_brushStyleFolders.end();
       it++) {
    if (*it == oldKey) {
      m_brushStyleFolders.erase(it);
      break;
    }
    i++;
  }

  if (i < m_brushStyleManagers.size())
    m_brushStyleManagers.erase(m_brushStyleManagers.begin() + i);

  styleManager->setStyleFolder(newPath);
  m_brushStyleFolders.push_back(newKey);
  m_brushStyleManagers.push_back(styleManager);
  styleManager->loadItems();
}
