
#include "tiio_webm.h"
#include "tsystem.h"
#include "trasterimage.h"
#include "tsound.h"
#include "timageinfo.h"
#include "toonz/stage.h"
#include <QStringList>

//===========================================================
//
//  TImageWriterWebm
//
//===========================================================

class TImageWriterWebm : public TImageWriter {
public:
  int m_frameIndex;

  TImageWriterWebm(const TFilePath &path, int frameIndex, TLevelWriterWebm *lwg)
      : TImageWriter(path), m_frameIndex(frameIndex), m_lwg(lwg) {
    m_lwg->addRef();
  }
  ~TImageWriterWebm() { m_lwg->release(); }

  bool is64bitOutputSupported() override { return false; }
  void save(const TImageP &img) override { m_lwg->save(img, m_frameIndex); }

private:
  TLevelWriterWebm *m_lwg;
};

//===========================================================
//
//  TLevelWriterWebm;
//
//===========================================================

TLevelWriterWebm::TLevelWriterWebm(const TFilePath &path, TPropertyGroup *winfo)
    : TLevelWriter(path, winfo) {
  if (!m_properties) m_properties = new Tiio::WebmWriterProperties();

  std::string scale = m_properties->getProperty("Scale")->getValueAsString();
  m_scale           = QString::fromStdString(scale).toInt();
  std::string codec = m_properties->getProperty("Codec")->getValueAsString();
  m_codec           = QString::fromStdString(codec);
  std::string speed = m_properties->getProperty("Speed")->getValueAsString();
  m_speed           = QString::fromStdString(speed);
  std::string pixelFormat =
      m_properties->getProperty("Pixel Format")->getValueAsString();
  m_pixelFormat   = QString::fromStdString(pixelFormat);
  std::string crf = m_properties->getProperty("CRF")->getValueAsString();
  m_crf           = QString::fromStdString(crf).toInt();

  ffmpegWriter = new Ffmpeg();
  ffmpegWriter->setPath(m_path);
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterWebm::~TLevelWriterWebm() {
  QStringList preIArgs;
  QStringList postIArgs;

  int outLx = m_lx;
  int outLy = m_ly;

  // set scaling
  if (m_scale != 0) {
    outLx = m_lx * m_scale / 100;
    outLy = m_ly * m_scale / 100;
  }
  // ffmpeg doesn't like resolutions that aren't divisible by 2.
  if (outLx % 2 != 0) outLx++;
  if (outLy % 2 != 0) outLy++;

  postIArgs << "-c:v";
  // Select codec based on property
  if (m_codec == "VP8")
    postIArgs << "libvpx";
  else if (m_codec == "VP9")
    postIArgs << "libvpx-vp9";
  else
    postIArgs << "libvpx-vp9";  // Default

  // For VP8/VP9, convert named speeds to speed values
  postIArgs << "-speed";
  if (m_speed == "Best Quality")
    postIArgs << "0";
  else if (m_speed == "Good Quality")
    postIArgs << "1";
  else if (m_speed == "Balanced")
    postIArgs << "2";
  else if (m_speed == "Fast")
    postIArgs << "3";
  else if (m_speed == "Faster")
    postIArgs << "4";
  else if (m_speed == "Fastest")
    postIArgs << "5";
  else
    postIArgs << "2";  // Default to Balanced

  preIArgs << "-framerate" << QString::number(m_frameRate);
  postIArgs << "-pix_fmt" << m_pixelFormat;
  postIArgs << "-crf" << QString::number(m_crf);
  postIArgs << "-auto-alt-ref" << "1";
  postIArgs << "-b:v" << "0";
  postIArgs << "-s" << QString::number(outLx) + "x" + QString::number(outLy);

  ffmpegWriter->runFfmpeg(preIArgs, postIArgs, false, false, true);
  ffmpegWriter->cleanUpFiles();
}

//-----------------------------------------------------------

TImageWriterP TLevelWriterWebm::getFrameWriter(TFrameId fid) {
  // if (IOError != 0)
  //	throw TImageException(m_path, buildGifExceptionString(IOError));
  if (!fid.getLetter().isEmpty()) return TImageWriterP(0);
  int index             = fid.getNumber();
  TImageWriterWebm *iwg = new TImageWriterWebm(m_path, index, this);
  return TImageWriterP(iwg);
}

//-----------------------------------------------------------
void TLevelWriterWebm::setFrameRate(double fps) {
  m_frameRate = fps;
  ffmpegWriter->setFrameRate(fps);
}

void TLevelWriterWebm::saveSoundTrack(TSoundTrack *st) {
  ffmpegWriter->saveSoundTrack(st);
}

//-----------------------------------------------------------

void TLevelWriterWebm::save(const TImageP &img, int frameIndex) {
  TRasterImageP image(img);
  m_lx = image->getRaster()->getLx();
  m_ly = image->getRaster()->getLy();
  ffmpegWriter->createIntermediateImage(img, frameIndex);
}

//===========================================================
//
//  TImageReaderWebm
//
//===========================================================

class TImageReaderWebm final : public TImageReader {
public:
  int m_frameIndex;

  TImageReaderWebm(const TFilePath &path, int index, TLevelReaderWebm *lra,
                   TImageInfo *info)
      : TImageReader(path), m_lra(lra), m_frameIndex(index), m_info(info) {
    m_lra->addRef();
  }
  ~TImageReaderWebm() { m_lra->release(); }

  TImageP load() override { return m_lra->load(m_frameIndex); }
  TDimension getSize() const { return m_lra->getSize(); }
  TRect getBBox() const { return TRect(); }
  const TImageInfo *getImageInfo() const override { return m_info; }

private:
  TLevelReaderWebm *m_lra;
  TImageInfo *m_info;

  // not implemented
  TImageReaderWebm(const TImageReaderWebm &);
  TImageReaderWebm &operator=(const TImageReaderWebm &src);
};

//===========================================================
//
//  TLevelReaderWebm
//
//===========================================================

TLevelReaderWebm::TLevelReaderWebm(const TFilePath &path) : TLevelReader(path) {
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

TLevelReaderWebm::~TLevelReaderWebm() {
  // ffmpegReader->cleanUpFiles();
}

//-----------------------------------------------------------

TLevelP TLevelReaderWebm::loadInfo() {
  if (m_frameCount == -1) return TLevelP();
  TLevelP level;
  for (int i = 1; i <= m_frameCount; i++) level->setFrame(i, TImageP());
  return level;
}

//-----------------------------------------------------------

TImageReaderP TLevelReaderWebm::getFrameReader(TFrameId fid) {
  // if (IOError != 0)
  //	throw TImageException(m_path, buildAVIExceptionString(IOError));
  if (!fid.getLetter().isEmpty()) return TImageReaderP(0);
  int index = fid.getNumber();

  TImageReaderWebm *irm = new TImageReaderWebm(m_path, index, this, m_info);
  return TImageReaderP(irm);
}

//------------------------------------------------------------------------------

TDimension TLevelReaderWebm::getSize() { return m_size; }

//------------------------------------------------

TImageP TLevelReaderWebm::load(int frameIndex) {
  if (!ffmpegFramesCreated) {
    ffmpegReader->getFramesFromMovie();
    ffmpegFramesCreated = true;
  }
  return ffmpegReader->getImage(frameIndex);
}

Tiio::WebmWriterProperties::WebmWriterProperties()
    : m_scale("Scale", 1, 100, 100)
    , m_crf("CRF", 4, 63, 23)
    , m_codec("Codec")
    , m_speed("Speed")
    , m_pixelFormat("Pixel Format") {
  bind(m_scale);
  bind(m_crf);
  bind(m_codec);
  bind(m_speed);
  bind(m_pixelFormat);

  // Codec
  m_codec.addValue(L"VP9");
  m_codec.addValue(L"VP8");
  m_codec.setValue(L"VP9");  // Default
  // Speed
  m_speed.addValue(L"Best Quality");  // speed 0
  m_speed.addValue(L"Good Quality");  // speed 1
  m_speed.addValue(L"Balanced");      // speed 2
  m_speed.addValue(L"Fast");          // speed 3
  m_speed.addValue(L"Faster");        // speed 4
  m_speed.addValue(L"Fastest");       // speed 5
  m_speed.setValue(L"Balanced");      // Default
  // Pixel Format
  m_pixelFormat.addValue(L"yuv420p");
  m_pixelFormat.addValue(L"yuv422p");
  m_pixelFormat.addValue(L"yuv444p");
  m_pixelFormat.addValue(L"yuv420p10le");
  m_pixelFormat.addValue(L"yuv422p10le");
  m_pixelFormat.addValue(L"yuv444p10le");
  m_pixelFormat.setValue(L"yuv420p");  // Default
}

void Tiio::WebmWriterProperties::updateTranslation() {
  m_scale.setQStringName(tr("Scale"));
  m_codec.setQStringName(tr("Codec"));
  m_speed.setQStringName(tr("Speed"));
  m_pixelFormat.setQStringName(tr("Pixel Format"));
  m_crf.setQStringName(tr("CRF"));
}

// Tiio::Reader* Tiio::makeWebmReader(){ return nullptr; }
// Tiio::Writer* Tiio::makeWebmWriter(){ return nullptr; }