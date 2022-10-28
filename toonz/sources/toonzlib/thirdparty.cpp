#include "thirdparty.h"

// TnzLib includes
#include "toonz/preferences.h"

// TnzCore includes
#include "tsystem.h"
#include "tmsgcore.h"

// QT includes
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

namespace ThirdParty {

//-----------------------------------------------------------------------------

void initialize() {
  // Auto detect FFmpeg
  if (!ThirdParty::checkFFmpeg()) {
    QString path = ThirdParty::autodetectFFmpeg();
    if (!path.isEmpty()) ThirdParty::setFFmpegDir(path);
  }
  // Auto detect Rhubarb
  if (!ThirdParty::checkRhubarb()) {
    QString path = ThirdParty::autodetectRhubarb();
    if (!path.isEmpty()) ThirdParty::setRhubarbDir(path);
  }
}

//=============================================================================
// FFmpeg interface
//-----------------------------------------------------------------------------

#ifdef _WIN32
#define FFMPEG_EXE "/ffmpeg.exe"
#define FFPROBE_EXE "/ffprobe.exe"
#else
#define FFMPEG_EXE "/ffmpeg"
#define FFPROBE_EXE "/ffprobe"
#endif

//-----------------------------------------------------------------------------

void getFFmpegVideoSupported(QStringList &exts) {
  exts.append("gif");
  exts.append("mp4");
  exts.append("webm");
}

//-----------------------------------------------------------------------------

void getFFmpegAudioSupported(QStringList &exts) {
  exts.append("mp3");
  exts.append("ogg");
  exts.append("flac");
}

//-----------------------------------------------------------------------------

bool findFFmpeg(QString dir) {
  // Relative path
  if (dir.isEmpty() || dir.at(0) == ".") {
    dir = QCoreApplication::applicationDirPath() + "/" + dir;
  }

  // Check if both executables exist
  return TSystem::doesExistFileOrLevel(TFilePath(dir + FFMPEG_EXE)) &&
         TSystem::doesExistFileOrLevel(TFilePath(dir + FFPROBE_EXE));
}

//-----------------------------------------------------------------------------

bool checkFFmpeg() {
  // Path in preferences
  return findFFmpeg(Preferences::instance()->getFfmpegPath());
}

//-----------------------------------------------------------------------------

QString autodetectFFmpeg() {
  QString dir = Preferences::instance()->getFfmpegPath();
  if (findFFmpeg(dir)) return dir;

  // Let's try and autodetect the exe included with release
  QStringList folderList;

  folderList.append(".");
  folderList.append("./ffmpeg");  // ffmpeg folder

#ifdef MACOSX
  // Look inside app
  folderList.append("./" +
                    QString::fromStdString(TEnv::getApplicationFileName()) +
                    ".app/ffmpeg");  // ffmpeg folder
#elif defined(LINUX) || defined(FREEBSD)
  // Need to account for symbolic links
  folderList.append(TEnv::getWorkingDirectory().getQString() +
                    "/ffmpeg");  // ffmpeg folder
#endif

#ifndef _WIN32
  folderList.append("/usr/local/bin");
  folderList.append("/usr/bin");
  folderList.append("/bin");
#endif

#ifdef _WIN32
  QString exePath = TSystem::findFileLocation(folderList, "ffmpeg.exe");
#else
  QString exePath = TSystem::findFileLocation(folderList, "ffmpeg");
#endif

  if (!exePath.isEmpty()) return exePath;

  // give up
  return "";
}

//-----------------------------------------------------------------------------

QString getFFmpegDir() {
  return Preferences::instance()->getStringValue(ffmpegPath);
}

//-----------------------------------------------------------------------------

void setFFmpegDir(const QString &dir) {
  QString path = Preferences::instance()->getFfmpegPath();
  if (path != dir) Preferences::instance()->setValue(ffmpegPath, dir);
}

//-----------------------------------------------------------------------------

int getFFmpegTimeout() {
  return Preferences::instance()->getIntValue(ffmpegTimeout);
}

//-----------------------------------------------------------------------------

void setFFmpegTimeout(int secs) {
  int timeout = Preferences::instance()->getIntValue(ffmpegTimeout);
  if (secs != timeout) Preferences::instance()->setValue(ffmpegTimeout, secs);
}

//-----------------------------------------------------------------------------

void runFFmpeg(QProcess &process, const QStringList &arguments) {
  QString dir = Preferences::instance()->getFfmpegPath();
  if (dir.at(0) == '.')  // Relative path
    dir = QCoreApplication::applicationDirPath() + "/" + dir;

  process.start(dir + FFMPEG_EXE, arguments);
}

//-----------------------------------------------------------------------------

void runFFprobe(QProcess &process, const QStringList &arguments) {
  QString dir = Preferences::instance()->getFfmpegPath();
  if (dir.at(0) == '.')  // Relative path
    dir = QCoreApplication::applicationDirPath() + "/" + dir;

  process.start(dir + FFPROBE_EXE, arguments);
}

//-----------------------------------------------------------------------------

void runFFmpegAudio(QProcess &process, QString srcPath, QString dstPath,
                    int samplerate, int bpp, int channels) {
  QStringList args;
  args << "-y";
  args << "-i";
  args << srcPath;
  args << "-f";
  switch (bpp) {
  case 8:  // unsigned
    args << "u8";
    break;
  case 16:
    args << "s16le";
    break;
  case 24:
    args << "s24le";
    break;
  case 32:  // floating-point
    args << "f32le";
    break;
  default:
    return;
  }
  args << "-ac";
  args << QString::number(channels);
  args << "-ar";
  args << QString::number(samplerate);
  args << dstPath;

  ThirdParty::runFFmpeg(process, args);
}

//-----------------------------------------------------------------------------

bool readFFmpegAudio(QProcess &process, QByteArray &rawData) {
  if (!process.waitForStarted()) return false;
  if (!process.waitForFinished(30000)) return false;
  bool success = (process.exitCode() == 0);

  if (success) rawData = process.readAllStandardOutput();
  process.close();

  return success;
}

//=============================================================================
// Rhubarb interface
//-----------------------------------------------------------------------------

#ifdef _WIN32
#define RHUBARB_EXE "/rhubarb.exe"
#else
#define RHUBARB_EXE "/rhubarb"
#endif

//-----------------------------------------------------------------------------

bool findRhubarb(QString dir) {
  // Rhubarb executable

  // Relative path
  if (dir.isEmpty() || dir.at(0) == ".") {
    dir = QCoreApplication::applicationDirPath() + "/" + dir;
  }

  // Check if executable exist
  return TSystem::doesExistFileOrLevel(TFilePath(dir + RHUBARB_EXE));
}

//-----------------------------------------------------------------------------

bool checkRhubarb() {
  // Path in preferences
  return findRhubarb(Preferences::instance()->getRhubarbPath());
}

//-----------------------------------------------------------------------------

QString autodetectRhubarb() {
  QString dir = Preferences::instance()->getRhubarbPath();
  if (findRhubarb(dir)) return dir;

  // Let's try and autodetect the exe included with release
  QStringList folderList;

  folderList.append(".");
  folderList.append("./rhubarb");  // rhubarb folder

#ifdef MACOSX
  // Look inside app
  folderList.append("./" +
                    QString::fromStdString(TEnv::getApplicationFileName()) +
                    ".app/rhubarb");  // rhubarb folder
#elif defined(LINUX) || defined(FREEBSD)
  // Need to account for symbolic links
  folderList.append(TEnv::getWorkingDirectory().getQString() +
                    "/rhubarb");  // rhubarb folder
#endif

#ifndef _WIN32
  folderList.append("/usr/local/bin");
  folderList.append(return "/usr/bin");
  folderList.append(return "/bin");
#endif

#ifdef _WIN32
  QString exePath = TSystem::findFileLocation(folderList, "rhubarb.exe");
#else
  QString exePath = TSystem::findFileLocation(folderList, "rhubarb");
#endif

  if (!exePath.isEmpty()) {
    Preferences::instance()->setValue(rhubarbPath, exePath);
    return exePath;
  }

  // give up
  return "";
}

//-----------------------------------------------------------------------------

void runRhubarb(QProcess &process, const QStringList &arguments) {
  QString dir = Preferences::instance()->getRhubarbPath();
  if (dir.at(0) == '.')  // Relative path
    dir = QCoreApplication::applicationDirPath() + "/" + dir;

  process.start(dir + RHUBARB_EXE, arguments);
}

//-----------------------------------------------------------------------------

QString getRhubarbDir() {
  return Preferences::instance()->getStringValue(rhubarbPath);
}

//-----------------------------------------------------------------------------

void setRhubarbDir(const QString &dir) {
  QString path = Preferences::instance()->getRhubarbPath();
  if (path != dir) Preferences::instance()->setValue(rhubarbPath, dir);
}

//-----------------------------------------------------------------------------

int getRhubarbTimeout() {
  return Preferences::instance()->getIntValue(rhubarbTimeout);
}

//-----------------------------------------------------------------------------

void setRhubarbTimeout(int secs) {
  int timeout = Preferences ::instance()->getIntValue(rhubarbTimeout);
  if (secs != timeout) Preferences::instance()->setValue(rhubarbTimeout, secs);
}

//-----------------------------------------------------------------------------

int waitAsyncProcess(const QProcess &process, int timeout) {
  QEventLoop eloop;
  QTimer timer;
  timer.connect(&timer, &QTimer::timeout, &eloop, [&eloop] { eloop.exit(-2); });
  QMetaObject::Connection con1 = process.connect(
      &process, &QProcess::errorOccurred, &eloop, [&eloop] { eloop.exit(-1); });
  QMetaObject::Connection con2 = process.connect(
      &process,
      static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
          &QProcess::finished),
      &eloop, &QEventLoop::quit);
  if (timeout >= 0) timer.start(timeout);

  int result = eloop.exec();
  process.disconnect(con1);
  process.disconnect(con2);

  return result;
}

//-----------------------------------------------------------------------------

}  // namespace ThirdParty