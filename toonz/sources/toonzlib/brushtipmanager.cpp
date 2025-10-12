

// Toonz includes
#include "tlevel_io.h"
#include "toonz/imagestyles.h"
#include "trasterimage.h"
#include "tropcm.h"
#include "tsystem.h"

// Qt includes
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <QThread>
#include <QPainter>

#include <opencv2/opencv.hpp>

#include "toonz/brushtipmanager.h"

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

//-----------------------------------------------------------------------------

TRaster32P makeTexture(const TRaster32P &ras) {
  if (!ras || ras->getLx() < 2 || ras->getLy() < 2) return TRaster32P();
  TRaster32P ras32 = ras;
  if (!ras32) return TRaster32P();
  TDimension d(2, 2);
  while (d.lx < 256 && d.lx * 2 <= ras32->getLx()) d.lx *= 2;
  while (d.ly < 256 && d.ly * 2 <= ras32->getLy()) d.ly *= 2;
  if (d == ras32->getSize())
    return ras32;
  else {
    TRaster32P texture(d);
    TScale sc((double)d.lx / ras32->getLx(), (double)d.ly / ras32->getLy());
    TRop::resample(texture, ras32, sc);
    return texture;
  }
}

//-----------------------------------------------------------------------------

cv::Mat QImageToCvMat(QImage *inImage) {
  switch (inImage->format()) {
  case QImage::Format_ARGB32:
  case QImage::Format_ARGB32_Premultiplied:
  case QImage::Format_RGB32: {
    cv::Mat mat(inImage->height(), inImage->width(), CV_8UC4,
                (void *)inImage->constBits(), inImage->bytesPerLine());
    return mat.clone();  // Clone to ensure data ownership
  }
  case QImage::Format_RGB888: {
    cv::Mat mat(inImage->height(), inImage->width(), CV_8UC3,
                (void *)inImage->constBits(), inImage->bytesPerLine());
    cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);  // OpenCV expects BGR
    return mat.clone();
  }
  case QImage::Format_Grayscale8: {
    cv::Mat mat(inImage->height(), inImage->width(), CV_8UC1,
                (void *)inImage->constBits(), inImage->bytesPerLine());
    return mat.clone();
  }
  default:
    return cv::Mat();
  }
}

std::vector<QPolygonF> findImageContours(const cv::Mat &image) {
  cv::Mat gray = image, gray_mask, binary;

  // Convert to gray scale.  We use this method because image can be black or
  // white
  cv::Scalar white_color(255, 255, 255);
  cv::Scalar gray_color(128, 128, 128);
  cv::Scalar black_color(0, 0, 0);
  cv::inRange(image, white_color, black_color, gray_mask);
  gray.setTo(gray_color, gray_mask);

  // Edge detection
  cv::Canny(gray, binary, 0.0, 255.0);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(binary, contours, cv::RETR_EXTERNAL,
                   cv::CHAIN_APPROX_SIMPLE);

  cv::Size size = image.size();

  // Center image for later use
  std::vector<QPolygonF> imageContours;
  for (const auto &contourPolygon : contours) {
    QPolygonF polygon;
    for (const auto &point : contourPolygon) {
      polygon.push_back(
          QPointF(point.x - (size.width * 0.5), point.y - (size.height * 0.5)));
    }
    imageContours.push_back(polygon);
  }

  return imageContours;
}

std::vector<QPolygonF> createImagecontour(QImage *originalQImage) {
  std::vector<QPolygonF> imageContours;

  QPoint center = originalQImage->rect().center();
  QTransform t;
  t.scale(1.0, -1.0);  // Image is upside down, flip

  QImage image = originalQImage->transformed(t);
  cv::Mat cvMatImage = QImageToCvMat(&image);
  if (!cvMatImage.empty()) imageContours = findImageContours(cvMatImage);

  return imageContours;
}

}  // namespace

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

class BrushTipLoaderTask final : public TThread::Runnable {
  TFilePath m_fp;
  QString m_rpath;
  BrushTipData *m_data;
  std::shared_ptr<QOffscreenSurface> m_offScreenSurface;

public:
  BrushTipLoaderTask(const TFilePath &fp, QString rpath);

  void run() override;

  void onFinished(TThread::RunnableP sender) override;
};

//-----------------------------------------------------------------------------

BrushTipLoaderTask::BrushTipLoaderTask(const TFilePath &fp, QString rpath)
    : m_fp(fp), m_rpath(rpath), m_data(0) {
  connect(this, SIGNAL(finished(TThread::RunnableP)), this,
          SLOT(onFinished(TThread::RunnableP)));

  if (QThread::currentThread() == qGuiApp->thread()) {
    m_offScreenSurface.reset(new QOffscreenSurface());
    m_offScreenSurface->setFormat(QSurfaceFormat::defaultFormat());
    m_offScreenSurface->create();
  }
}

//-----------------------------------------------------------------------------

void BrushTipLoaderTask::run() {
  try {
    // Fetch the level
    TLevelReaderP lr(m_fp);
    TLevelP level = lr->loadInfo();
    if (!level || level->getFrameCount() == 0) return;

    TLevel::Iterator frameIt = level->begin();
    if (frameIt == level->end()) return;

    const QSize &qChipSize = TBrushTipManager::instance()->getChipSize();
    TDimension chipSize(qChipSize.width(), qChipSize.height());
    QImage *image = nullptr;
    QImage *tmp   = nullptr;
    TRaster32P rasChip;
    std::vector<QPolygonF> imageContour;

    for (frameIt = level->begin(); frameIt != level->end(); ++frameIt) {
      TImageP img = lr->getFrameReader(frameIt->first)->load();
      if (TRasterImageP ri = img) {
        TRaster32P ras = ri->getRaster();
        if (!ras) continue;

        // First image is the chip
        if (frameIt == level->begin()) {
          tmp = new QImage(chipSize.lx, chipSize.ly, QImage::Format_ARGB32);

          TDimension size = ri->getRaster()->getSize();
          if (size == chipSize) {
            rasChip = ras->clone();  // Yep, this may be necessary
            convertRaster32ToImage(rasChip, tmp);
          } else {
            TRaster32P rout(chipSize);

            TRop::resample(rout, ras,
                           TScale((double)chipSize.lx / size.lx,
                                  (double)chipSize.ly / size.ly));
            convertRaster32ToImage(rout, tmp);

            // Gray background since brush tip can be black or white
            TRop::addBackground(rout, TPixel32(160, 160, 160));
            rasChip = rout;
          }
#ifndef LINUX
          image = new QImage(chipSize.lx, chipSize.ly, QImage::Format_RGB32);
          convertRaster32ToImage(rasChip, image);
          imageContour = createImagecontour(tmp);
#endif
        }

        // aggiusta le dimensioni
        ras = makeTexture(ras);
        if (!ras) continue;
        level->setFrame(frameIt->first, new TRasterImage(ras));
      }
    }

#ifdef LINUX
    image = new QImage(chipSize.lx, chipSize.ly, QImage::Format_RGB32);
    convertRaster32ToImage(rasChip, image);
    imageContour = createImagecontour(tmp);
#endif

    QString name   = QString::fromStdString(m_fp.getName());
    std::string id = TTextureStyle::staticBrushIdName(m_rpath.toStdWString());
    QString folder = (m_rpath.toStdString() != m_fp.getLevelName())
                         ? TFilePath(m_rpath).getParentDir().getQString()
                         : "";

    m_data = new BrushTipData();

    m_data->m_path               = m_fp;
    m_data->m_brushTipName       = name;
    m_data->m_brushTipFolderName = folder;
    m_data->m_image              = image;
    m_data->m_idName             = id;
    m_data->m_level              = level;
    m_data->m_imageContour       = imageContour;
  } catch (...) {
  }
}

//-----------------------------------------------------------------------------

void BrushTipLoaderTask::onFinished(TThread::RunnableP sender) {
  if (!m_data) return;

  // On the main thread...
  if (m_data->m_image)  // Everything went ok
  {
    TBrushTipManager::instance()->addBrushTip(m_data);
  }
  TBrushTipManager::instance()->loadItemFinished(m_data->m_path);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

void TBrushTipManager::loadItems() {
  // Build the folder to be read
  if (m_brushTipFolder == TFilePath()) return;

  bool brushTipsUpdated = false;

  TFilePath fp;
  std::list<TFilePath> queue;
  queue.push_front(m_brushTipFolder);
  while (!queue.empty()) {
    TFilePath dir = queue.front();
    queue.pop_front();

    QDir brushTipDir(QString::fromStdWString(dir.getWideString()));
    brushTipDir.setNameFilters(m_filters.split(' '));

    // Read the said folder
    TFilePathSet fps;
    try {
      TSystem::readDirectory(fps, brushTipDir);

      // For each file entry, generate a fetching task
      // NOTE: after all adds have finished, a separate itemsUpdated() signal is
      // emitted
      TFilePathSet::iterator fpIt;
      for (fpIt = fps.begin(); fpIt != fps.end(); ++fpIt) {
        // Load brushtip
        TFilePath file = *fpIt;
        std::vector<TFilePath>::iterator it2 =
            std::find(m_activeLoads.begin(), m_activeLoads.end(), file);
        if (it2 != m_activeLoads.end()) continue;

        // Ignore it if it's already there
        bool tipExists = false;
        for (int i = 0; i < m_brushTips.count(); i++) {
          tipExists = m_brushTips[i]->m_path == file;
          if (tipExists) break;
        }

        if (!tipExists) {
          m_activeLoads.push_back(file);
          TFilePath rpath = file - TFilePath(m_brushTipFolder);
          m_executor.addTask(new BrushTipLoaderTask(file, rpath.getQString()));
        }
      }
    } catch (...) {
      return;
    }

    QStringList fpList;
    TSystem::readDirectory_DirItems(fpList, dir);
    for (int i = 0; i < fpList.count(); i++)
      queue.push_front(dir + TFilePath(fpList[i]));
  }

  if (!m_activeLoads.size()) emit itemsUpdated();
}

//-----------------------------------------------------------------------------

void TBrushTipManager::loadItemFinished(TFilePath file) {
  std::vector<TFilePath>::iterator it =
      std::find(m_activeLoads.begin(), m_activeLoads.end(), file);
  if (it != m_activeLoads.end()) m_activeLoads.erase(it);
  m_itemsLoaded++;
  if (!m_activeLoads.size() && !isLoading()) signalLoadsFinished();
};

//-----------------------------------------------------------------------------

int TBrushTipManager::getBrushTipCount() {
  return m_isIndexed ? m_indexes.count() : m_brushTips.size();
}

//-----------------------------------------------------------------------------

BrushTipData *TBrushTipManager::getBrushTip(int index) {
  if (m_isIndexed)
    return (index < 0 || index >= m_indexes.count())
               ? 0
               : m_brushTips[m_indexes[index]];

  return (index < 0 || index >= m_brushTips.size()) ? 0 : m_brushTips[index];
}

//-----------------------------------------------------------------------------

BrushTipData *TBrushTipManager::getBrushTipById(std::string id) {
  for (int i = 0; i < m_brushTips.count(); i++) {
    if (m_brushTips[i]->m_idName == id) return m_brushTips[i];
  }

  return 0;
}

//-----------------------------------------------------------------------------

void TBrushTipManager::applyFilter() {
  QList<int> indexes;

  m_indexes.clear();
  int len = m_brushTips.count();
  for (int i = 0; i < len; i++) {
    auto &chip = m_brushTips[i];
    if (chip->m_brushTipName.indexOf(m_searchText, 0, Qt::CaseInsensitive) >= 0)
      m_indexes.append(i);
    if (!m_indexes.contains(i) &&
        chip->m_brushTipFolderName.indexOf(m_searchText, 0,
                                           Qt::CaseInsensitive) >= 0)
      m_indexes.append(i);
  }

  m_indexes.append(indexes);
  m_isIndexed = (m_indexes.count() != len);
}
