#include "tiio_ffmpeg.h"
#include "../toonz/tapp.h"
#include "tsystem.h"
#include "tsound.h"
#include "tenv.h"
#include "timageinfo.h"
#include "toonz/stage.h"

#include <QProcess>
#include <QEventLoop>
#include <QTimer>
#include <QDir>
#include <QtGui/QImage>
#include <QRegExp>
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "tmsgcore.h"
#include "thirdparty.h"

Ffmpeg::Ffmpeg() {
  m_ffmpegTimeout      = ThirdParty::getFFmpegTimeout() * 1000;
  if (m_ffmpegTimeout <= 0)
    m_ffmpegTimeout    = -1;
  m_intermediateFormat = "png";
  m_startNumber        = 2147483647;  // Lowest frame determines starting frame
}
Ffmpeg::~Ffmpeg() {}

bool Ffmpeg::checkFormat(std::string format) {
  static std::string strResults = "";
  if (strResults.empty()) {
    QStringList args;
    args << "-formats";
    QProcess ffmpeg;
    ThirdParty::runFFmpeg(ffmpeg, args);
    ffmpeg.waitForFinished();
    QString results = ffmpeg.readAllStandardError();
    results += ffmpeg.readAllStandardOutput();
    ffmpeg.close();
    strResults = results.toStdString();
  }
  std::string::size_type n;
  n = strResults.find(format);
  if (n != std::string::npos)
    return true;

  return false;
}

bool Ffmpeg::checkCodecs(std::string codec) {
  QStringList args;
  args << "-codecs";
  QProcess ffmpeg;
  ThirdParty::runFFmpeg(ffmpeg, args);
  ffmpeg.waitForFinished();
  QString results = ffmpeg.readAllStandardError();
  results += ffmpeg.readAllStandardOutput();
  ffmpeg.close();
  std::string strResults = results.toStdString();
  std::string::size_type n;
  n = strResults.find(codec);
  if (n != std::string::npos)
    return true;

  return false;
}

TFilePath Ffmpeg::getFfmpegCache() {
  QString cacheRoot = ToonzFolder::getCacheRootFolder().getQString();
  if (!TSystem::doesExistFileOrLevel(TFilePath(cacheRoot + "/ffmpeg"))) {
    TSystem::mkDir(TFilePath(cacheRoot + "/ffmpeg"));
  }
  std::string ffmpegPath =
      TFilePath(cacheRoot + "/ffmpeg").getQString().toStdString();
  return TFilePath(cacheRoot + "/ffmpeg");
}

void Ffmpeg::setFrameRate(double fps) { m_frameRate = fps; }

void Ffmpeg::setPath(TFilePath path) { m_path = path; }

void Ffmpeg::createIntermediateImage(const TImageP &img, int frameIndex) {
  m_frameCount++;
  frameIndex--;  // ffmpeg start at 0 by default
  if (frameIndex < m_startNumber) m_startNumber = frameIndex;
  QString tempPath = getFfmpegCache().getQString() + "//" +
                     QString::fromStdString(m_path.getName()) + "tempOut" +
                     QString::number(frameIndex) + "." + m_intermediateFormat;
  std::string saveStatus = "";
  TRasterImageP tempImage(img);
  TRasterImage *image = (TRasterImage *)tempImage->cloneImage();

  m_lx           = image->getRaster()->getLx();
  m_ly           = image->getRaster()->getLy();
  m_bpp          = image->getRaster()->getPixelSize();
  int totalBytes = m_lx * m_ly * m_bpp;
  image->getRaster()->yMirror();

  // lock raster to get data
  image->getRaster()->lock();
  void *buffin = image->getRaster()->getRawData();
  assert(buffin);
  void *buffer = malloc(totalBytes);
  memcpy(buffer, buffin, totalBytes);

  image->getRaster()->unlock();

  // create QImage save format
  QByteArray ba      = m_intermediateFormat.toUpper().toLatin1();
  const char *format = ba.data();

  QImage *qi = new QImage((uint8_t *)buffer, m_lx, m_ly, QImage::Format_ARGB32);
  qi->save(tempPath, format, -1);
  free(buffer);
  m_cleanUpList.push_back(tempPath);

  delete qi;
  delete image;
}

void Ffmpeg::runFfmpeg(QStringList preIArgs, QStringList postIArgs,
                       bool includesInPath, bool includesOutPath,
                       bool overWriteFiles, bool asyncProcess) {
  QString tempName = "//" + QString::fromStdString(m_path.getName()) +
                     "tempOut%d." + m_intermediateFormat;
  tempName = getFfmpegCache().getQString() + tempName;

  QStringList args;
  args = args + preIArgs;
  if (!includesInPath) {  // NOTE:  if including the in path, it needs to be in
                          // the preIArgs argument.
    if (m_startNumber != 0) {
      args << "-start_number";
      args << QString::number(m_startNumber);
    }
    args << "-i";
    args << tempName;
  }
  if (m_hasSoundTrack) args = args + m_audioArgs;
  args                      = args + postIArgs;
  if (overWriteFiles && !includesOutPath) {  // if includesOutPath is true, you
                                             // need to include the overwrite in
                                             // your postIArgs.
    args << "-y";
  }
  if (!includesOutPath) {
    args << m_path.getQString();
  }

  // write the file
  QProcess ffmpeg;
  ThirdParty::runFFmpeg(ffmpeg, args);
  if (waitFfmpeg(ffmpeg, asyncProcess)) {
    QString results = ffmpeg.readAllStandardError();
    results += ffmpeg.readAllStandardOutput();
    int exitCode = ffmpeg.exitCode();
    std::string strResults = results.toStdString();
  }
  ffmpeg.close();
}

QString Ffmpeg::runFfprobe(QStringList args) {
  QProcess ffmpeg;
  ThirdParty::runFFprobe(ffmpeg, args);
  if (!waitFfmpeg(ffmpeg, false)) {
    throw TImageException(m_path, "error accessing ffprobe.");
  }
  QString results = ffmpeg.readAllStandardError();
  results += ffmpeg.readAllStandardOutput();
  int exitCode = ffmpeg.exitCode();
  ffmpeg.close();
  // If the url cannot be opened or recognized as a multimedia file, ffprobe
  // returns a positive exit code.
  if (exitCode > 0) throw TImageException(m_path, "error reading info.");
  std::string strResults = results.toStdString();
  return results;
}

bool Ffmpeg::waitFfmpeg(QProcess &ffmpeg, bool asyncProcess) {
  if (!asyncProcess) {
    bool status = ffmpeg.waitForFinished(m_ffmpegTimeout);
    if (!status) {
      DVGui::warning(
          QObject::tr("FFmpeg timed out.\n"
                      "Please check the file for errors.\n"
                      "If the file doesn't play or is incomplete, \n"
                      "Please try raising the FFmpeg timeout in Preferences."));
    }
    return status;
  }

  int exitCode = ThirdParty::waitAsyncProcess(ffmpeg, m_ffmpegTimeout);
  if (exitCode == 0) return true;
  if (exitCode == -1) {
    DVGui::warning(
        QObject::tr("FFmpeg returned error-code: %1").arg(ffmpeg.exitCode()));
  }
  if (exitCode == -2) {
    DVGui::warning(
        QObject::tr("FFmpeg timed out.\n"
                    "Please check the file for errors.\n"
                    "If the file doesn't play or is incomplete, \n"
                    "Please try raising the FFmpeg timeout in Preferences."));
  }
  return false;
}

void Ffmpeg::saveSoundTrack(TSoundTrack *st) {
  m_sampleRate    = st->getSampleRate();
  m_channelCount  = st->getChannelCount();
  m_bitsPerSample = st->getBitPerSample();
  // LONG count = st->getSampleCount();
  int bufSize         = st->getSampleCount() * st->getSampleSize();
  const UCHAR *buffer = st->getRawData();

  m_audioPath = getFfmpegCache().getQString() + "//" +
                QString::fromStdString(m_path.getName()) + "tempOut.raw";
  m_audioFormat = ((st->getSampleType() == TSound::FLOAT) ? "f" : "s") +
                  QString::number(m_bitsPerSample);
  if (m_bitsPerSample > 8) m_audioFormat = m_audioFormat + "le";
  std::string strPath                    = m_audioPath.toStdString();

  QByteArray data;
  data.insert(0, (char *)buffer, bufSize);

  QFile file(m_audioPath);
  file.open(QIODevice::WriteOnly);
  file.write(data);
  file.close();
  m_hasSoundTrack = true;

  m_audioArgs << "-f";
  m_audioArgs << m_audioFormat;
  m_audioArgs << "-ar";
  m_audioArgs << QString::number(m_sampleRate);
  m_audioArgs << "-ac";
  m_audioArgs << QString::number(m_channelCount);
  m_audioArgs << "-i";
  m_audioArgs << m_audioPath;

  // add file to framesWritten for cleanup
  m_cleanUpList.push_back(m_audioPath);
}

bool Ffmpeg::checkFilesExist() {
  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath = ffmpegCachePath + "//" + cleanPathSymbols() + "In0001." +
                     m_intermediateFormat;
  if (TSystem::doesExistFileOrLevel(TFilePath(tempPath))) {
    return true;
  } else
    return false;
}

ffmpegFileInfo Ffmpeg::getInfo() {
  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath = ffmpegCachePath + "//" + cleanPathSymbols() + ".txt";
  if (QFile::exists(tempPath)) {
    QFile infoText(tempPath);
    infoText.open(QIODevice::ReadOnly);
    QByteArray ba = infoText.readAll();
    infoText.close();
    QString text = QString::fromStdString(ba.toStdString());
    QStringList textlist = text.split(" ");
    if (textlist.count() >= 4) {
      m_lx         = textlist[0].toInt();
      m_ly         = textlist[1].toInt();
      m_frameRate  = textlist[2].toDouble();
      m_frameCount = textlist[3].toInt();
    }
  } else {
    QFile infoText(tempPath);
    getSize();
    getFrameCount();
    getFrameRate();
    infoText.open(QIODevice::WriteOnly);
    std::string infoToWrite =
        std::to_string(m_lx) + " " + std::to_string(m_ly) + " " +
        std::to_string(m_frameRate) + " " + std::to_string(m_frameCount);
    int infoLength = infoToWrite.length();
    infoText.write(infoToWrite.c_str(), infoLength);
    infoText.close();
  }
  ffmpegFileInfo info;
  info.m_lx         = m_lx;
  info.m_ly         = m_ly;
  info.m_frameRate  = m_frameRate;
  info.m_frameCount = m_frameCount;
  return info;
}
TRasterImageP Ffmpeg::getImage(int frameIndex) {
  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath        = ffmpegCachePath + "//" + cleanPathSymbols();
  std::string tmpPath     = tempPath.toStdString();
  // QString tempPath= m_path.getQString();
  QString number   = QString("%1").arg(frameIndex, 4, 10, QChar('0'));
  QString tempName = "In" + number + ".png";
  tempName         = tempPath + tempName;

  // for debugging
  std::string strPath = tempName.toStdString();
  if (TSystem::doesExistFileOrLevel(TFilePath(tempName))) {
    QImage *temp = new QImage(tempName, "PNG");
    if (temp) {
      QImage tempToo = temp->convertToFormat(QImage::Format_ARGB32);
      delete temp;
      const UCHAR *bits = tempToo.bits();

      TRasterPT<TPixelRGBM32> ret;
      ret.create(m_lx, m_ly);
      ret->lock();
      memcpy(ret->getRawData(), bits, m_lx * m_ly * 4);
      ret->unlock();
      ret->yMirror();
      return TRasterImageP(ret);
    }
  }
  return TRasterImageP();
}

double Ffmpeg::getFrameRate() {
  QStringList fpsArgs;
  int fpsNum = 0, fpsDen = 0;
  fpsArgs << "-v";
  fpsArgs << "error";
  fpsArgs << "-select_streams";
  fpsArgs << "v:0";
  fpsArgs << "-show_entries";
  fpsArgs << "stream=r_frame_rate";
  fpsArgs << "-of";
  fpsArgs << "default=noprint_wrappers=1:nokey=1";
  fpsArgs << m_path.getQString();
  QString fpsResults = runFfprobe(fpsArgs);

  QStringList fpsResultsList = fpsResults.split("/");
  if (fpsResultsList.size() > 1) {
    fpsNum = fpsResultsList[0].toInt();
    fpsDen = fpsResultsList[1].toInt();
  }

  // if for some reason we don't have enough info to calculate it. Use the
  // avg_frame_rate
  if (!fpsDen) {
    fpsArgs.clear();
    fpsArgs << "-v";
    fpsArgs << "error";
    fpsArgs << "-select_streams";
    fpsArgs << "v:0";
    fpsArgs << "-show_entries";
    fpsArgs << "stream=avg_frame_rate";
    fpsArgs << "-of";
    fpsArgs << "default=noprint_wrappers=1:nokey=1";
    fpsArgs << m_path.getQString();
    QString fpsResults = runFfprobe(fpsArgs);

    fpsResultsList = fpsResults.split("/");
    if (fpsResultsList.size() > 1) {
      fpsNum = fpsResultsList[0].toInt();
      fpsDen = fpsResultsList[1].toInt();
    }
  }

  if (fpsDen > 0) {
    m_frameRate = (double)fpsNum / (double)fpsDen;
  }
  return m_frameRate;
}

TDimension Ffmpeg::getSize() {
  QStringList sizeArgs;
  sizeArgs << "-v";
  sizeArgs << "error";
  sizeArgs << "-of";
  sizeArgs << "flat=s=_";
  sizeArgs << "-select_streams";
  sizeArgs << "v:0";
  sizeArgs << "-show_entries";
  sizeArgs << "stream=height,width";
  sizeArgs << m_path.getQString();

  QString sizeResults = runFfprobe(sizeArgs);
  QStringList split   = sizeResults.split("\n");
  m_lx                = split[0].split("=")[1].toInt();
  m_ly                = split[1].split("=")[1].toInt();
  return TDimension(m_lx, m_ly);
}

int Ffmpeg::getFrameCount() {
  // nb_read_frames from files may not be accurate. Let's calculate it based on
  // r_frame_rate * duration
  QStringList frameCountArgs;
  frameCountArgs << "-v";
  frameCountArgs << "error";
  frameCountArgs << "-count_frames";
  frameCountArgs << "-select_streams";
  frameCountArgs << "v:0";
  frameCountArgs << "-show_entries";
  frameCountArgs << "stream=duration";
  frameCountArgs << "-of";
  frameCountArgs << "default=nokey=1:noprint_wrappers=1";
  frameCountArgs << m_path.getQString();

  QString frameResults = runFfprobe(frameCountArgs);
  m_frameCount         = frameResults.toDouble() * getFrameRate();

  // if for some reason we don't have enough info to calculate it. Use the
  // nb_read_frames
  if (!m_frameCount) {
    frameCountArgs.clear();
    frameCountArgs << "-v";
    frameCountArgs << "error";
    frameCountArgs << "-count_frames";
    frameCountArgs << "-select_streams";
    frameCountArgs << "v:0";
    frameCountArgs << "-show_entries";
    frameCountArgs << "stream=nb_read_frames";
    frameCountArgs << "-of";
    frameCountArgs << "default=nokey=1:noprint_wrappers=1";
    frameCountArgs << m_path.getQString();

    frameResults = runFfprobe(frameCountArgs);
    m_frameCount = frameResults.toInt();
  }

  return m_frameCount;
}

void Ffmpeg::getFramesFromMovie(int frame) {
  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath        = ffmpegCachePath + "//" + cleanPathSymbols();
  std::string tmpPath     = tempPath.toStdString();
  QString tempName        = "In%04d." + m_intermediateFormat;
  tempName                = tempPath + tempName;
  QString tempStart;
  if (frame == -1) {
    tempStart = "In0001." + m_intermediateFormat;
    tempStart = tempPath + tempStart;
  } else {
    QString number = QString("%1").arg(frame, 4, 10, QChar('0'));
    tempStart      = tempPath + "In" + number + "." + m_intermediateFormat;
  }
  QString tempBase = tempPath + "In";
  QString addToDelete;
  if (!TSystem::doesExistFileOrLevel(TFilePath(tempStart))) {
    // for debugging
    std::string strPath = tempName.toStdString();

    QStringList preIFrameArgs;
    QStringList postIFrameArgs;
    // frameArgs << "-accurate_seek";
    // frameArgs << "-ss";
    // frameArgs << "0" + QString::number(frameIndex / m_info->m_frameRate);

    // Detect codec from the input file
    QStringList probeArgs;
    probeArgs << "-v" << "error"
              << "-select_streams" << "v:0"
              << "-show_entries" << "stream=codec_name"
              << "-of" << "default=noprint_wrappers=1:nokey=1"
              << m_path.getQString();

    QString codecName = runFfprobe(probeArgs).trimmed();

    // Set decoder based on detected codec
    if (codecName.contains("vp9", Qt::CaseInsensitive)) {
      preIFrameArgs << "-vcodec" << "libvpx-vp9";
    } else if (codecName.contains("vp8", Qt::CaseInsensitive)) {
      preIFrameArgs << "-vcodec" << "libvpx";
    } else if (codecName.contains("av1", Qt::CaseInsensitive)) {
      preIFrameArgs << "-vcodec" << "libaom-av1";
    }

    // Common args
    preIFrameArgs << "-threads" << "auto";
    preIFrameArgs << "-i" << m_path.getQString();
    postIFrameArgs << "-y";
    postIFrameArgs << "-f";
    postIFrameArgs << "image2";

    postIFrameArgs << tempName;

    runFfmpeg(preIFrameArgs, postIFrameArgs, true, true, true, false);

    QString addToDelete;
    QString tempBase = tempPath + "In";
    for (int i = 1; i <= m_frameCount; i++) {
      QString number = QString("%1").arg(i, 4, 10, QChar('0'));
      addToDelete    = tempBase + number + "." + m_intermediateFormat;
      // addToCleanUp(addToDelete);
    }
  }
}

QString Ffmpeg::cleanPathSymbols() {
  return m_path.getQString().remove(QRegExp(
      QString::fromUtf8("[-`~!@#$%^&*()_+=|:;<>«»,.?/{}\'\"\\[\\]\\\\]")));
}

int Ffmpeg::getGifFrameCount() {
  int frame               = 1;
  QString ffmpegCachePath = getFfmpegCache().getQString();
  QString tempPath        = ffmpegCachePath + "//" + cleanPathSymbols();
  std::string tmpPath     = tempPath.toStdString();
  QString tempName        = "In%04d." + m_intermediateFormat;
  tempName                = tempPath + tempName;
  QString tempStart;
  tempStart = "In0001." + m_intermediateFormat;
  tempStart = tempPath + tempStart;
  while (TSystem::doesExistFileOrLevel(TFilePath(tempStart))) {
    frame++;
    QString number = QString("%1").arg(frame, 4, 10, QChar('0'));
    tempStart      = tempPath + "In" + number + "." + m_intermediateFormat;
  }
  return frame - 1;
}

void Ffmpeg::addToCleanUp(QString path) {
  if (TSystem::doesExistFileOrLevel(TFilePath(path))) {
    m_cleanUpList.push_back(path);
  }
}

void Ffmpeg::cleanUpFiles() {
  for (QString path : m_cleanUpList) {
    if (TSystem::doesExistFileOrLevel(TFilePath(path))) {
      TSystem::deleteFile(TFilePath(path));
    }
  }
}

void Ffmpeg::disablePrecompute() {
  Preferences::instance()->setPrecompute(false);
}

//===========================================================
//
//  TImageReaderFFmpeg
//
//===========================================================

class TImageReaderFFmpeg final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderFFmpeg(const TFilePath &path, int index, TLevelReaderFFmpeg *lra,
                     TImageInfo *info)
      : TImageReader(path), m_lra(lra), m_frameIndex(index), m_info(info) {
    m_lra->addRef();
  }
  ~TImageReaderFFmpeg() { m_lra->release(); }

  TImageP load() override { return m_lra->load(m_frameIndex); }
  TDimension getSize() const { return m_lra->getSize(); }
  TRect getBBox() const { return TRect(); }
  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  TLevelReaderFFmpeg *m_lra;
  TImageInfo *m_info;

  // not implemented
  TImageReaderFFmpeg(const TImageReaderFFmpeg &);
  TImageReaderFFmpeg &operator=(const TImageReaderFFmpeg &src);
};

//===========================================================
//
//  TLevelReaderFFmpeg
//
//===========================================================

TLevelReaderFFmpeg::TLevelReaderFFmpeg(const TFilePath &path)
    : TLevelReader(path) {
  ffmpegReader = new Ffmpeg();
  ffmpegReader->setPath(m_path);
  ffmpegReader->disablePrecompute();
  ffmpegFileInfo tempInfo = ffmpegReader->getInfo();
  double fps              = tempInfo.m_frameRate;
  m_frameCount            = tempInfo.m_frameCount;
  m_size                  = TDimension(tempInfo.m_lx, tempInfo.m_ly);
  m_lx                    = m_size.lx;
  m_ly                    = m_size.ly;

  // set values
  m_info                   = new TImageInfo();
  m_info->m_frameRate      = fps;
  m_info->m_lx             = m_lx;
  m_info->m_ly             = m_ly;
  m_info->m_bitsPerSample  = 8;
  m_info->m_samplePerPixel = 4;
  m_info->m_dpix           = Stage::standardDpi;
  m_info->m_dpiy           = Stage::standardDpi;
}
//-----------------------------------------------------------

TLevelReaderFFmpeg::~TLevelReaderFFmpeg() {}

//-----------------------------------------------------------

TLevelP TLevelReaderFFmpeg::loadInfo() {
  if (m_frameCount == -1) return TLevelP();
  TLevelP level;
  for (int i = 1; i <= m_frameCount; i++) level->setFrame(i, TImageP());
  return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderFFmpeg::getFrameReader(TFrameId fid) {
  if (!fid.getLetter().isEmpty()) return TImageReaderP(0);
  int index = fid.getNumber();

  TImageReaderFFmpeg *irm = new TImageReaderFFmpeg(m_path, index, this, m_info);
  return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderFFmpeg::getSize() { return m_size; }

//------------------------------------------------

TImageP TLevelReaderFFmpeg::load(int frameIndex) {
  if (!ffmpegFramesCreated) {
    ffmpegReader->getFramesFromMovie();
    ffmpegFramesCreated = true;
  }
  return ffmpegReader->getImage(frameIndex);
}