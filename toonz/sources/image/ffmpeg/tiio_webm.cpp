
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
    : TLevelWriter(path, winfo), m_lx(0), m_ly(0) {
  if (!m_properties) m_properties = new Tiio::WebmWriterProperties();

  parseProperties();

  ffmpegWriter = new Ffmpeg();
  ffmpegWriter->setPath(m_path);
  if (TSystem::doesExistFileOrLevel(m_path)) TSystem::deleteFile(m_path);
}

//-----------------------------------------------------------

TLevelWriterWebm::~TLevelWriterWebm() {
  QStringList preIArgs;
  QStringList postIArgs;

  // Calculate output dimensions
  int outLx = m_lx;
  int outLy = m_ly;
  if (m_scale != 0) {
    outLx = m_lx * m_scale / 100;
    outLy = m_ly * m_scale / 100;
  }
  outLx += (outLx % 2);  // Ensure even dimensions
  outLy += (outLy % 2);

  // Set input args
  preIArgs << "-framerate" << QString::number(m_frameRate);

  // Set codec
  postIArgs << "-c:v";
  postIArgs << (m_codec == "VP8" ? "libvpx" : "libvpx-vp9");

  // Set encoding speed
  postIArgs << "-speed";
  const int speed = [this]() {
    if (m_speed == "Best Quality") return 0;
    if (m_speed == "Good Quality") return 1;
    if (m_speed == "Balanced") return 2;
    if (m_speed == "Fast") return 3;
    if (m_speed == "Faster") return 4;
    if (m_speed == "Fastest") return 5;
    return 2;  // Default to Balanced
  }();
  postIArgs << QString::number(speed);

  // Set color space
  postIArgs << "-color_primaries" << "bt709"
            << "-color_trc" << "bt709"
            << "-colorspace" << "bt709";

  // Format settings
  postIArgs << "-pix_fmt" << m_pixelFormat.toLower();

  // Quality settings
  postIArgs << "-crf" << QString::number(m_crf) << "-auto-alt-ref" << "1"
            << "-b:v" << "0";

  // Resolution settings
  postIArgs << "-s" << QString("%1x%2").arg(outLx).arg(outLy);

  ffmpegWriter->runFfmpeg(preIArgs, postIArgs, false, false, true);
  ffmpegWriter->cleanUpFiles();
}

//-----------------------------------------------------------

void TLevelWriterWebm::parseProperties() {
  m_scale = QString::fromStdString(
                m_properties->getProperty("Scale")->getValueAsString())
                .toInt();
  m_codec = QString::fromStdString(
      m_properties->getProperty("Codec")->getValueAsString());
  m_speed = QString::fromStdString(
      m_properties->getProperty("Speed")->getValueAsString());
  m_pixelFormat = QString::fromStdString(
      m_properties->getProperty("Pixel Format")->getValueAsString());
  m_crf = QString::fromStdString(
              m_properties->getProperty("CRF")->getValueAsString())
              .toInt();
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
  m_codec.addValue(L"VP8");
  m_codec.addValue(L"VP9");
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
  m_pixelFormat.addValue(L"YUV420P");
  m_pixelFormat.addValue(L"YUV422P");
  m_pixelFormat.addValue(L"YUV444P");
  m_pixelFormat.addValue(L"YUV420P10LE");
  m_pixelFormat.addValue(L"YUV422P10LE");
  m_pixelFormat.addValue(L"YUV444P10LE");
  m_pixelFormat.setValue(L"YUV420P");  // Default
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