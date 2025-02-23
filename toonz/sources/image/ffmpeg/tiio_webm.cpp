
#include "tiio_webm.h"
#include "tsystem.h"
#include "trasterimage.h"
#include "tsound.h"
#include "timageinfo.h"
#include "toonz/stage.h"
#include <QStringList>
#include <QDebug>

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

  // Calculate output dimensions (ensure even)
  int outLx = m_lx;
  int outLy = m_ly;
  if (m_scale != 0) {
    outLx = m_lx * m_scale / 100;
    outLy = m_ly * m_scale / 100;
  }
  outLx += (outLx % 2);  // Add 1 if odd
  outLy += (outLy % 2);

  // Input settings
  preIArgs << "-framerate" << QString::number(m_frameRate);

  // Codec selection
  postIArgs << "-c:v";
  if (m_codec == "AV1")
    postIArgs << "libaom-av1";  // AV1 codec
  else
    postIArgs << "libvpx-vp9";  // VP9 codec (default)

  // Common encoder settings
  postIArgs << "-threads" << "auto"      // Auto-detect CPU threads
            << "-row-mt" << "1"          // Row-based multi-threading
            << "-static-thresh" << "0";  // Optimize for sharp animation content

  // Quality settings
  if (m_crf == 0) {  // Lossless mode
    postIArgs << "-lossless" << "1";
    if (m_codec == "VP9")
      postIArgs << "-tune" << "ssim";  // SSIM tuning for VP9 lossless
  } else {                             // Lossy mode
    postIArgs << "-crf" << QString::number(m_crf);  // Constant rate factor
    if (m_codec == "VP9")
      postIArgs << "-tune" << "psnr";  // PSNR tuning for VP9 lossy
  }

  // AV1-specific optimizations
  if (m_codec == "AV1") {
    postIArgs << "-tune" << "ssim"  // SSIM tuning for animation
              << "-frame-parallel"
              << "1"  // Enable frame parallelism (compatibility TBD)
              << "-denoise-noise-level" << "10";  // Preserve fine noise details
    int tiles = (outLx >= 1920 || outLy >= 1080)
                    ? 4
                    : 2;  // 4x4 tiles for HD+, 2x2 otherwise
    postIArgs << "-tiles" << QString("%1x%1").arg(tiles) << "-tile-columns"
              << QString::number(tiles) << "-tile-rows"
              << QString::number(tiles);
  }

  // VP9-specific optimizations
  if (m_codec == "VP9") {
    postIArgs << "-auto-alt-ref"
              << "0";  // Disable alt refs for image sequences
  }

  // Keyframe interval
  postIArgs << "-g" << QString::number(m_kf);  // Keyframe every # frames

  // Encoding speed
  if (m_codec == "AV1") {
    postIArgs << "-speed";
    int speed = (m_speed == "Slow"       ? 0
                 : m_speed == "Balanced" ? 4
                                         : 8);  // 0=slow, 8=fast
    postIArgs << QString::number(speed);
  } else {  // VP9
    postIArgs << "-cpu-used";
    int cpuUsed = (m_speed == "Slow"       ? 0
                   : m_speed == "Balanced" ? 2
                                           : 4);  // 0=slow, 4=fast
    postIArgs << QString::number(cpuUsed);
  }

  // Color space (BT.709 for standard video)
  postIArgs << "-color_primaries" << "bt709"
            << "-color_trc" << "bt709"
            << "-colorspace" << "bt709";

  // Pixel format
  QString pixFmt = "yuv420p";                 // Default for compatibility
  if (m_codec == "VP9") pixFmt = "yuva420p";  // VP9 supports alpha
  postIArgs << "-pix_fmt" << pixFmt;

  // Output resolution
  postIArgs << "-s" << QString("%1x%2").arg(outLx).arg(outLy);

  // Debug arguments
  qDebug() << "preIArgs:" << preIArgs << "postIArgs:" << postIArgs;

  // Execute FFmpeg and clean up
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
      m_properties->getProperty("Encoding Speed")->getValueAsString());
  m_crf = QString::fromStdString(
              m_properties->getProperty("Quality (CRF)")->getValueAsString())
              .toInt();
  m_kf = QString::fromStdString(
             m_properties->getProperty("Key Frame Every (frames)")
                 ->getValueAsString())
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
    , m_crf("Quality (CRF)", 0, 63, 28)  // 0=lossless, 63=worst
    , m_codec("Codec")
    , m_speed("Encoding Speed")
    , m_kf("Key Frame Every (frames)", 1, 1000, 24, false) {
  bind(m_scale);
  bind(m_crf);
  bind(m_codec);
  bind(m_speed);
  bind(m_kf);

  // Codecs
  m_codec.addValue(L"VP9");
  m_codec.addValue(L"AV1");

  // Encoding Speeds
  // VP9 speed 0, AV1 speed 0
  m_speed.addValueWithUIName(L"Slow", tr("Slow (Best Compression)"));
  // VP9 speed 2, AV1 speed 4
  m_speed.addValueWithUIName(L"Balanced", tr("Balanced (Recommended)"));
  // VP9 speed 4, AV1 speed 8
  m_speed.addValueWithUIName(L"Fast", tr("Fast (Larger File)"));
}

void Tiio::WebmWriterProperties::updateTranslation() {
  m_scale.setQStringName(tr("Scale"));
  m_codec.setQStringName(tr("Codec"));
  m_speed.setQStringName(tr("Encoding Speed"));
  m_crf.setQStringName(tr("Quality (CRF)"));
  m_kf.setQStringName(tr("Key Frame Every (frames)"));
}

// Tiio::Reader* Tiio::makeWebmReader(){ return nullptr; }
// Tiio::Writer* Tiio::makeWebmWriter(){ return nullptr; }