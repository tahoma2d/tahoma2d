#pragma once

#ifndef TTIO_WEBM_INCLUDED
#define TTIO_WEBM_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "tiio_ffmpeg.h"
#include <QCoreApplication>

//===========================================================
//
//  TLevelWriterWebm
//
//===========================================================

class TLevelWriterWebm : public TLevelWriter {
public:
  TLevelWriterWebm(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterWebm();
  void setFrameRate(double fps) override;

  TImageWriterP getFrameWriter(TFrameId fid) override;
  void save(const TImageP &image, int frameIndex);

  void saveSoundTrack(TSoundTrack *st) override;

  static TLevelWriter *create(const TFilePath &path, TPropertyGroup *winfo) {
    return new TLevelWriterWebm(path, winfo);
  }

private:
  Ffmpeg *ffmpegWriter;
  int m_lx, m_ly;
  int m_scale;
  QString m_codec;
  QString m_speed;
  QString m_pixelFormat;
  int m_crf;
  // void *m_buffer;
};

//===========================================================
//
//  TLevelReaderWebm
//
//===========================================================

class TLevelReaderWebm final : public TLevelReader {
public:
  TLevelReaderWebm(const TFilePath &path);
  ~TLevelReaderWebm();
  TImageReaderP getFrameReader(TFrameId fid) override;

  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderWebm(f);
  }

  TLevelP loadInfo() override;
  TImageP load(int frameIndex);
  TDimension getSize();
  // TThread::Mutex m_mutex;
  // void *m_decompressedBuffer;
private:
  Ffmpeg *ffmpegReader;
  bool ffmpegFramesCreated = false;
  TDimension m_size;
  int m_frameCount, m_lx, m_ly;
};

//===========================================================================

namespace Tiio {

//===========================================================================

class WebmWriterProperties : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(WebmWriterProperties)
public:
  TIntProperty m_scale;
  TEnumProperty m_codec;
  TEnumProperty m_speed;
  TEnumProperty m_pixelFormat;
  TIntProperty m_crf;

  WebmWriterProperties();
  void updateTranslation() override;
};

//===========================================================================

// Tiio::Reader *makeWebmReader();
// Tiio::Writer *makeWebmWriter();

}  // namespace

#endif
