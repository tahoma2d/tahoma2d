

#include "tsound_t.h"
#include "texception.h"
#include "tthread.h"
#include "tthreadmessage.h"

#include <errno.h>
#include <unistd.h>
#include <queue>
#include <set>

#include <QByteArray>
#include <QAudioFormat>
#include <QBuffer>
#include <QAudioOutput>

using namespace std;

//==============================================================================
namespace {
TThread::Mutex MutexOut;
}

class TSoundOutputDeviceImp
    : public std::enable_shared_from_this<TSoundOutputDeviceImp> {
public:
  struct Output {
    QByteArray   *m_byteArray;
    QBuffer      *m_buffer;
    QAudioOutput *m_audioOutput;
    explicit Output(): m_byteArray(), m_buffer(), m_audioOutput() { }
  };

private:
  double m_volume;
  bool m_looping;
  QList<Output> m_outputs;

public:
  std::set<TSoundOutputDeviceListener *> m_listeners;

  TSoundOutputDeviceImp(): m_volume(0.5), m_looping(false) {};
  ~TSoundOutputDeviceImp() { stop(); }

  double getVolume() { return m_volume; }
  double isLooping() { return m_looping; }
  void prepareVolume(double x) { m_volume = x; }
  void setLooping(bool x) { m_looping = x; } // looping not implemented here yet

  void clearAudioOutputs(bool keepActive = false);
  QAudioOutput* createAudioOutput(QAudioFormat &format);
  bool isPlaying();
  void setVolume(double x);
  void stop();

  void play(const TSoundTrackP &st, TINT32 s0, TINT32 s1, bool loop, bool scrubbing);
};

//==============================================================================

TSoundOutputDevice::TSoundOutputDevice() : m_imp(new TSoundOutputDeviceImp) {
  try {
    supportsVolume();
  } catch (TSoundDeviceException &e) {
    throw TSoundDeviceException(e.getType(), e.getMessage());
  }
}

//------------------------------------------------------------------------------

TSoundOutputDevice::~TSoundOutputDevice() {
  stop();
  close();
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::installed() { return true; }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::open(const TSoundTrackP &st) {
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::close() {
  stop();
  return true;
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::prepareVolume(double volume) {
    m_imp->prepareVolume(volume);
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::play(const TSoundTrackP &st, TINT32 s0, TINT32 s1,
                              bool loop, bool scrubbing) {
  int lastSample = st->getSampleCount() - 1;
  notLessThan(0, s0);
  notLessThan(0, s1);

  notMoreThan(lastSample, s0);
  notMoreThan(lastSample, s1);

  if (s0 > s1) {
#ifdef DEBUG
    cout << "s0 > s1; reorder" << endl;
#endif
    swap(s0, s1);
  }

  m_imp->play(st, s0, s1, loop, scrubbing);
}

//------------------------------------------------------------------------------

void TSoundOutputDeviceImp::play(const TSoundTrackP &st, TINT32 s0, TINT32 s1, bool loop, bool scrubbing) {
  clearAudioOutputs(true);
  m_looping = loop;

  QAudioFormat format;
  format.setSampleSize(st->getBitPerSample());
  format.setCodec("audio/pcm");
  format.setChannelCount(st->getChannelCount());
  format.setByteOrder(QAudioFormat::LittleEndian);
  format.setSampleType(st->getFormat().m_signedSample
                           ? QAudioFormat::SignedInt
                           : QAudioFormat::UnSignedInt);
  format.setSampleRate(st->getSampleRate());

  QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
  if (!info.isFormatSupported((format)))
    format = info.nearestFormat(format);

  Output output;
  qint64 totalPacketCount = s1 - s0;
  qint64 fileByteCount    = (s1 - s0) * st->getSampleSize();
  char*  entireFileBuffer = new char[fileByteCount];
  memcpy(entireFileBuffer, st->getRawData() + s0 * st->getSampleSize(), fileByteCount);
  output.m_byteArray = new QByteArray(entireFileBuffer, fileByteCount);

  output.m_buffer = new QBuffer();
  output.m_buffer->setBuffer(output.m_byteArray);
  output.m_buffer->open(QIODevice::ReadOnly);
  output.m_buffer->seek(0);

  output.m_audioOutput = new QAudioOutput(format);
  output.m_audioOutput->start(output.m_buffer);
  output.m_audioOutput->setVolume(m_volume);

  m_outputs.push_back(output);
}

//------------------------------------------------------------------------------

void TSoundOutputDeviceImp::clearAudioOutputs(bool keepActive) {
  for(QList<Output>::iterator i = m_outputs.begin(); i != m_outputs.end(); ) {
    if (!keepActive || i->m_audioOutput->state() != QAudio::ActiveState) {
      delete i->m_audioOutput;
      delete i->m_buffer;
      delete i->m_byteArray;
      i = m_outputs.erase(i);
    } else ++i;
  }
}

//------------------------------------------------------------------------------

bool TSoundOutputDeviceImp::isPlaying() {
  clearAudioOutputs(true);
  return !m_outputs.empty();
}

//------------------------------------------------------------------------------

void TSoundOutputDeviceImp::setVolume(double x) {
  prepareVolume(x);
  clearAudioOutputs(true);
  for(QList<Output>::iterator i = m_outputs.begin(); i != m_outputs.end(); ++i)
    i->m_audioOutput->setVolume(x);
}

//------------------------------------------------------------------------------

void TSoundOutputDeviceImp::stop() {
  clearAudioOutputs();
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::stop() {
  m_imp->stop();
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::attach(TSoundOutputDeviceListener *listener) {
  m_imp->m_listeners.insert(listener);
}

//------------------------------------------------------------------------------

void TSoundOutputDevice::detach(TSoundOutputDeviceListener *listener) {
  m_imp->m_listeners.erase(listener);
}

//------------------------------------------------------------------------------

double TSoundOutputDevice::getVolume() {
  return m_imp->getVolume();
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::setVolume(double volume) {
  m_imp->setVolume(volume);
  return true;
}

//------------------------------------------------------------------------------

bool TSoundOutputDevice::supportsVolume() { return true; }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isPlaying() const { return m_imp->isPlaying(); }

//------------------------------------------------------------------------------

bool TSoundOutputDevice::isLooping() { return m_imp->isLooping(); }

//------------------------------------------------------------------------------

void TSoundOutputDevice::setLooping(bool loop) { m_imp->setLooping(loop); }

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                         int channelCount,
                                                         int bitPerSample) {
  TSoundTrackFormat fmt(sampleRate, bitPerSample, channelCount, true);
  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundOutputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
  return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                            format.m_bitPerSample);
}

//==============================================================================
//==============================================================================
//                REGISTRAZIONE
//==============================================================================
//==============================================================================

class TSoundInputDeviceImp {
public:
  // ALport m_port;
  bool m_stopped;
  bool m_isRecording;
  bool m_oneShotRecording;

  long m_recordedSampleCount;

  TSoundTrackFormat m_currentFormat;
  TSoundTrackP m_st;
  std::set<int> m_supportedRate;

  TThread::Executor m_executor;

  TSoundInputDeviceImp()
      : m_stopped(false)
      , m_isRecording(false)
      //   , m_port(NULL)
      , m_oneShotRecording(false)
      , m_recordedSampleCount(0)
      , m_st(0)
      , m_supportedRate(){};

  ~TSoundInputDeviceImp(){};

  bool doOpenDevice(const TSoundTrackFormat &format,
                    TSoundInputDevice::Source devType);
};

bool TSoundInputDeviceImp::doOpenDevice(const TSoundTrackFormat &format,
                                        TSoundInputDevice::Source devType) {
  return true;
}

//==============================================================================

class RecordTask : public TThread::Runnable {
public:
  TSoundInputDeviceImp *m_devImp;
  int m_ByteToSample;

  RecordTask(TSoundInputDeviceImp *devImp, int numByte)
      : TThread::Runnable(), m_devImp(devImp), m_ByteToSample(numByte){};

  ~RecordTask(){};

  void run();
};

void RecordTask::run() {}

//==============================================================================

TSoundInputDevice::TSoundInputDevice() : m_imp(new TSoundInputDeviceImp) {}

//------------------------------------------------------------------------------

TSoundInputDevice::~TSoundInputDevice() {}

//------------------------------------------------------------------------------

bool TSoundInputDevice::installed() {
  /*
  if (alQueryValues(AL_SYSTEM, AL_DEFAULT_INPUT, 0, 0, 0, 0) <=0)
return false;
*/
  return true;
}

//------------------------------------------------------------------------------

void TSoundInputDevice::record(const TSoundTrackFormat &format,
                               TSoundInputDevice::Source type) {}

//------------------------------------------------------------------------------

void TSoundInputDevice::record(const TSoundTrackP &st,
                               TSoundInputDevice::Source type) {}

//------------------------------------------------------------------------------

TSoundTrackP TSoundInputDevice::stop() {
  TSoundTrackP st;
  return st;
}

//------------------------------------------------------------------------------

double TSoundInputDevice::getVolume() { return 0.0; }

//------------------------------------------------------------------------------

bool TSoundInputDevice::setVolume(double volume) { return true; }

//------------------------------------------------------------------------------

bool TSoundInputDevice::supportsVolume() { return true; }

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundInputDevice::getPreferredFormat(TUINT32 sampleRate,
                                                        int channelCount,
                                                        int bitPerSample) {
  TSoundTrackFormat fmt;
  return fmt;
}

//------------------------------------------------------------------------------

TSoundTrackFormat TSoundInputDevice::getPreferredFormat(
    const TSoundTrackFormat &format) {
  /*
try {
*/
  return getPreferredFormat(format.m_sampleRate, format.m_channelCount,
                            format.m_bitPerSample);
  /*}

catch (TSoundDeviceException &e) {
throw TSoundDeviceException( e.getType(), e.getMessage());
}
*/
}

//------------------------------------------------------------------------------

bool TSoundInputDevice::isRecording() { return m_imp->m_isRecording; }
