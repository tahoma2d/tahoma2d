#pragma once

#ifndef TTIO_FFMPEG_INCLUDED
#define TTIO_FFMPEG_INCLUDED

#include "tproperty.h"
#include "tlevel_io.h"
#include "trasterimage.h"
#include <QVector>
#include <QStringList>
#include <QProcess>

struct ffmpegFileInfo {
  int m_lx, m_ly, m_frameCount;
  double m_frameRate;
};

class Ffmpeg {
public:
  Ffmpeg();
  ~Ffmpeg();
  void createIntermediateImage(const TImageP &image, int frameIndex);
  void runFfmpeg(QStringList preIArgs, QStringList postIArgs,
                 bool includesInPath, bool includesOutPath, bool overWriteFiles,
                 bool asyncProcess = true);
  void runFfmpeg(QStringList preIArgs, QStringList postIArgs, TFilePath path);
  QString runFfprobe(QStringList args);
  void cleanUpFiles();
  void addToCleanUp(QString);
  void setFrameRate(double fps);
  void setPath(TFilePath path);
  void saveSoundTrack(TSoundTrack *st);
  bool checkFilesExist();
  static bool checkFormat(std::string format);
  static bool checkCodecs(std::string format);
  double getFrameRate();
  TDimension getSize();
  int getFrameCount();
  void getFramesFromMovie(int frame = -1);
  TRasterImageP getImage(int frameIndex);
  TFilePath getFfmpegCache();
  ffmpegFileInfo getInfo();
  void disablePrecompute();
  int getGifFrameCount();

private:
  QString m_intermediateFormat, m_audioPath, m_audioFormat;
  int m_frameCount    = 0, m_lx, m_ly, m_bpp, m_bitsPerSample, m_channelCount,
      m_ffmpegTimeout = -1, m_startNumber = 2147483647;
  double m_frameRate   = 24.0;
  bool m_hasSoundTrack = false;
  TFilePath m_path;
  QVector<QString> m_cleanUpList;
  QStringList m_audioArgs;
  TUINT32 m_sampleRate;
  QString cleanPathSymbols();
  bool waitFfmpeg(QProcess &ffmpeg, bool asyncProcess);
};

//===========================================================
//
//  TLevelReaderFFmpeg
//
//===========================================================

class TLevelReaderFFmpeg final : public TLevelReader {
public:
  TLevelReaderFFmpeg(const TFilePath &path);
  ~TLevelReaderFFmpeg();
  TImageReaderP getFrameReader(TFrameId fid) override;

  static TLevelReader *create(const TFilePath &f) {
    return new TLevelReaderFFmpeg(f);
  }

  TLevelP loadInfo() override;
  TImageP load(int frameIndex);
  TDimension getSize();

private:
  Ffmpeg *ffmpegReader;
  bool ffmpegFramesCreated = false;
  TDimension m_size;
  int m_frameCount, m_lx, m_ly;
};

//===========================================================================

#endif
