#pragma once

#ifndef TTIO_MOV_INCLUDED
#define TTIO_MOV_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "tiio_ffmpeg.h"

#include <QCoreApplication>

//===========================================================
//
//  TLevelWriterMov
//
//===========================================================

class TLevelWriterMov : public TLevelWriter {
public:
  TLevelWriterMov(const TFilePath &path, TPropertyGroup *winfo);
  ~TLevelWriterMov();
  void setFrameRate(double fps) override;

  TImageWriterP getFrameWriter(TFrameId fid) override;
  void save(const TImageP &image, int frameIndex);

  void saveSoundTrack(TSoundTrack *st) override;

  static TLevelWriter *create(const TFilePath &path, TPropertyGroup *winfo) {
    return new TLevelWriterMov(path, winfo);
  }

private:
  Ffmpeg *ffmpegWriter;
  int m_lx, m_ly;
  int m_scale;
  int m_vidQuality;
  // void *m_buffer;
};

//===========================================================
//
//  TLevelReaderMov
//
//===========================================================

class TLevelReaderMov final : public TLevelReader {
public:
  TLevelReaderMov(const TFilePath &path);
  ~TLevelReaderMov();
  TImageReaderP getFrameReader(TFrameId fid) override;

  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderMov(f);
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

class MovWriterProperties : public TPropertyGroup {
  Q_DECLARE_TR_FUNCTIONS(MovWriterProperties)
public:
  // TEnumProperty m_pixelSize;
  // TBoolProperty m_matte;
  TIntProperty m_vidQuality;
  TIntProperty m_scale;
  MovWriterProperties();
  void updateTranslation() override;
};

//===========================================================================

// Tiio::Reader *makeMovReader();
// Tiio::Writer *makeMovWriter();

}  // namespace

#endif
