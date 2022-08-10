#include "crashhandler.h"

#include <inttypes.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#include <winbase.h>
#include <dbghelp.h>
#else
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <err.h>
#include <regex>
#endif

#include "tgl.h"
#include "tapp.h"
#include "tenv.h"
#include "tconvert.h"
#include "tfilepath_io.h"
#include "toonz/toonzfolders.h"
#include "toonz/tproject.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"

#include <QOperatingSystemVersion>
#include <QDesktopServices>
#include <QApplication>
#include <QClipboard>
#include <QThread>
#include <QMainWindow>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDialog>
#include <QLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>

//-----------------------------------------------------------------------------

#ifndef _WIN32

static bool sh(std::string &out, const char *cmd) {
  char buffer[128];
  FILE *p = popen(cmd, "r");
  if (p == NULL) return false;
  while (fgets(buffer, 128, p)) out.append(buffer);
  pclose(p);
  return true;
}

static bool addr2line(std::string &out, const char *exepath, const char *addr) {
  char cmd[512];
#ifdef OSX
  sprintf(cmd, "atos -o \"%.400s\" %s 2>&1", exepath, addr);
#else
  sprintf(cmd, "addr2line -f -p -e \"%.400s\" %s 2>&1", exepath, addr);
#endif
  return sh(out, cmd);
}

#endif

static void print_backtrace(std::string &out) {
  int frameStack = 0;
  int frameSkip  = 2;

#ifdef _WIN32
  HANDLE hProcess = GetCurrentProcess();

  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES |
                SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

  SymInitialize(hProcess, NULL, TRUE);

  CONTEXT context;
  RtlCaptureContext(&context);

  char sourceSymMem[sizeof(IMAGEHLP_SYMBOL64) + 1025];
  PIMAGEHLP_SYMBOL64 sourceSym = (PIMAGEHLP_SYMBOL64)&sourceSymMem;
  memset(sourceSymMem, 0, sizeof(sourceSymMem));

  IMAGEHLP_LINE64 sourceInfo;
  memset(&sourceInfo, 0, sizeof(IMAGEHLP_LINE64));
  sourceInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

  STACKFRAME64 stackframe;
  memset(&stackframe, 0, sizeof(STACKFRAME64));

#ifdef _WIN64
  int machineType             = IMAGE_FILE_MACHINE_AMD64;
  stackframe.AddrPC.Offset    = context.Rip;
  stackframe.AddrPC.Mode      = AddrModeFlat;
  stackframe.AddrStack.Offset = context.Rsp;
  stackframe.AddrStack.Mode   = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.Rbp;
  stackframe.AddrFrame.Mode   = AddrModeFlat;
#else
  int machineType             = IMAGE_FILE_MACHINE_I386;
  stackframe.AddrPC.Offset    = context.Eip;
  stackframe.AddrPC.Mode      = AddrModeFlat;
  stackframe.AddrStack.Offset = context.Esp;
  stackframe.AddrStack.Mode   = AddrModeFlat;
  stackframe.AddrFrame.Offset = context.Ebp;
  stackframe.AddrFrame.Mode   = AddrModeFlat;
#endif

  HANDLE hThread = GetCurrentThread();
  while (StackWalk64(machineType, hProcess, hThread, &stackframe, &context,
                     NULL, SymFunctionTableAccess64, SymGetModuleBase64,
                     NULL)) {

    // Skip first frames since they point to this function
    if (frameStack++ < frameSkip) continue;
    char numStr[32];
    memset(numStr, 0, sizeof(numStr));
    sprintf(numStr, "%3i> ", frameStack - frameSkip);
    out.append(numStr);

    sourceSym->SizeOfStruct  = sizeof(IMAGEHLP_SYMBOL64);
    sourceSym->MaxNameLength = 1024;

    // Get symbol name
    DWORD64 displacement64;
    if (SymGetSymFromAddr64(hProcess, (ULONG64)stackframe.AddrPC.Offset,
                            &displacement64, sourceSym)) {
      out.append(sourceSym->Name);

      // Get source filename and line
      DWORD displacement32;
      if (SymGetLineFromAddr64(hProcess, stackframe.AddrPC.Offset,
                               &displacement32, &sourceInfo) != FALSE) {
        out.append(" {");
        out.append(sourceInfo.FileName);
        out.append(":");
        out.append(std::to_string(sourceInfo.LineNumber));
        out.append("}");
      } else {
        memset(numStr, 0, sizeof(numStr));
        sprintf(numStr, " [0x%" PRIx64 "]", stackframe.AddrPC.Offset);
        out.append(numStr);
      }
    }

    out.append("\n");
  }

  SymCleanup(hProcess);

#else

  const int size = 256;
  void *buffer[size];

  // Get executable path
  char exepath[512];
  memset(exepath, 0, 512);
  if (readlink("/proc/self/exe", exepath, 512) < 0) throw TException();

  // Back trace
  int nptrs  = backtrace(buffer, size);
  char **bts = backtrace_symbols(buffer, nptrs);
  std::regex re("\\[(.+)\\]");
  if (bts) {
    for (int i = 0; i < nptrs; ++i) {
      // Skip first frames since they point to this function
      if (frameStack++ < frameSkip) continue;
      char numStr[32];
      memset(numStr, 0, sizeof(numStr));
      sprintf(numStr, "%3i> ", frameStack - frameSkip);
      out.append(numStr);

      std::string sym = bts[i];
      std::string line;
      std::smatch ms;

      bool found = false;
      if (std::regex_search(sym, ms, re)) {
        std::string addr = ms[1];
        if (addr2line(line, exepath, addr.c_str())) {
          found = (line.rfind("??", 0) != 0);
        }
      }

      out.append(found ? line : (sym + "\n"));
    }
  }

  free(bts);

#endif

}

//-----------------------------------------------------------------------------

static void print_sysinfo(std::string &out) {
  out.append("Build ABI: " + QSysInfo::buildAbi().toStdString() + "\n");
  out.append("Operating System: " + QSysInfo::prettyProductName().toStdString() + "\n");
  out.append("OS Kernel: " + QSysInfo::kernelVersion().toStdString() + "\n");
  out.append("CPU Threads: " + std::to_string(QThread::idealThreadCount()) + "\n");
}

//-----------------------------------------------------------------------------

static void print_gpuinfo(std::string &out) {
  std::string gpuVendorName = (const char *)glGetString(GL_VENDOR);
  std::string gpuModelName  = (const char *)glGetString(GL_RENDERER);
  std::string gpuVersion    = (const char *)glGetString(GL_VERSION);
  out.append("GPU Vendor: " + gpuVendorName + "\n");
  out.append("GPU Model: " + gpuModelName + "\n");
  out.append("GPU Version: " + gpuVersion + "\n");
}

//-----------------------------------------------------------------------------

static void signal_handler(int sig) {
  QString reason = "Unknown";

  switch (sig) {
  case SIGABRT:
    reason = "(SIGABRT) Usually caused by an abort() or assert()";
    break;
  case SIGFPE:
    reason = "(SIGFPE) Arithmetic exception, such as divide by zero";
    break;
  case SIGILL:
    reason = "(SIGILL) Illegal instruction";
    break;
  case SIGINT:
    reason = "(SIGINT) Interactive attention signal, (usually ctrl+c)";
    break;
  case SIGSEGV:
    reason = "(SIGSEGV) Segmentation Fault";
    break;
  case SIGTERM:
    reason = "(SIGTERM) A termination request was sent to the program";
    break;
  }

  if (CrashHandler::trigger(reason, true)) _Exit(1);
}

//-----------------------------------------------------------------------------

CrashHandler::CrashHandler(QWidget *parent, TFilePath crashFile, QString crashReport)
    : QDialog(parent), m_crashFile(crashFile), m_crashReport(crashReport) {
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);

  QStringList sl;
  sl.append(tr("<b>Tahoma2D crashed unexpectedly.</b>"));
  sl.append("");
  sl.append(tr("A crash report has been generated."));
  sl.append(
      tr("To report, click 'Open Issue Webpage' to access Tahoma2D's Issues "
         "page on GitHub."));
  sl.append(tr("Click on the 'New issue' button and fill out the form."));
  sl.append("");
  sl.append(tr("System Configuration and Problem Details:"));
  
  QLabel *headtext = new QLabel(sl.join("<br>"));
  headtext->setTextFormat(Qt::RichText);

  QTextEdit *reportTxt = new QTextEdit();
  reportTxt->setText(crashReport);
  reportTxt->setReadOnly(true);
  reportTxt->setLineWrapMode(QTextEdit::LineWrapMode::NoWrap);
  reportTxt->setStyleSheet(
      "background:white;\ncolor:black;\nborder:1 solid black;");

  QVBoxLayout *mainLayout = new QVBoxLayout();
  QHBoxLayout *buttonsLay = new QHBoxLayout();

  QPushButton *copyBtn   = new QPushButton(tr("Copy to Clipboard"));
  QPushButton *webBtn    = new QPushButton(tr("Open Issue Webpage"));
  QPushButton *closeBtn  = new QPushButton(tr("Close Application"));
  buttonsLay->addWidget(copyBtn);
  buttonsLay->addWidget(webBtn);
  buttonsLay->addWidget(closeBtn);

  mainLayout->addWidget(headtext);
  mainLayout->addWidget(reportTxt);
  mainLayout->addLayout(buttonsLay);

  bool ret = connect(copyBtn, SIGNAL(clicked()), this, SLOT(copyClipboard()));
  ret      = ret && connect(webBtn, SIGNAL(clicked()), this, SLOT(openWebpage()));
  ret      = ret && connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
  if (!ret) throw TException();

  setWindowTitle(tr("Tahoma2D crashed!"));
  setLayout(mainLayout);
}

void CrashHandler::reject() {
  QStringList sl;
  sl.append(tr("Application is in unstable state and must be restarted."));
  sl.append(tr("Resuming is not recommended, it may lead to an unrecoverable crash."));
  sl.append(tr("Ignore advice and try to resume program?"));

  QMessageBox::StandardButton reply =
      QMessageBox::question(this, tr("Ignore crash?"), sl.join("\n"),
                            QMessageBox::Yes | QMessageBox::No);
  if (reply == QMessageBox::Yes) {
    QDialog::reject();
  }
}

//-----------------------------------------------------------------------------

void CrashHandler::copyClipboard() {
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(m_crashReport);
}

//-----------------------------------------------------------------------------

void CrashHandler::openWebpage() {
  QDesktopServices::openUrl(QUrl("https://github.com/tahoma2d/tahoma2d/issues"));
}  

//-----------------------------------------------------------------------------

void CrashHandler::install() {
  signal(SIGABRT, signal_handler);
  signal(SIGFPE, signal_handler);
  signal(SIGILL, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGSEGV, signal_handler);
  signal(SIGTERM, signal_handler);
}

//-----------------------------------------------------------------------------

bool CrashHandler::trigger(const QString reason, bool showDialog) {
  char fileName[1024];
  char dateName[256];
  std::string out;

  // Get time and build filename
  time_t acc_time;
  time(&acc_time);
  struct tm *tm = localtime(&acc_time);
  strftime(dateName, 128, "%Y-%m-%d %H:%M:%S", tm);
  strftime(fileName, 128, "Crash-%Y%m%d-%H%M%S.log", tm);
  TFilePath fp = ToonzFolder::getCrashReportFolder() + fileName;

  TProjectManager *pm      = TProjectManager::instance();
  TProjectP currentProject = pm->getCurrentProject();
  TFilePath projectPath    = currentProject->getProjectPath();
  ToonzScene *currentScene = TApp::instance()->getCurrentScene()->getScene();
  std::wstring sceneName   = currentScene->getSceneName();

  // Generate report
  try {
    out.append(TEnv::getApplicationFullName() + "  (Build " + __DATE__ ")\n");
    out.append("\nReport Date: ");
    out.append(dateName);
    out.append("\nCrash Reason: ");
    out.append(reason.toStdString());
    out.append("\n\n");
    print_sysinfo(out);
    out.append("\n");
    print_gpuinfo(out);
    out.append("\nCrash File: ");
    out.append(fp.getQString().toStdString());
    out.append("\nApplication Dir: ");
    out.append(QCoreApplication::applicationDirPath().toStdString());
    out.append("\nStuff Dir: ");
    out.append(TEnv::getStuffDir().getQString().toStdString());
    out.append("\n");
    out.append("\nProject Name: ");
    out.append(currentProject->getName().getQString().toStdString());
    out.append("\nScene Name: ");
    out.append(QString::fromStdWString(sceneName).toStdString());
    out.append("\nProject Path: ");
    out.append(projectPath.getQString().toStdString());
    out.append("\nScene Path: ");
    out.append(currentScene->getScenePath().getQString().toStdString());
    out.append("\n\n==== Backtrace ====\n");
    print_backtrace(out);
    out.append("==== End ====\n");
  } catch (...) {
  }

  // Save to crash information to file
  FILE *fw = fopen(fp, "w");
  if (fw != NULL) {
    fwrite(out.c_str(), 1, out.size(), fw);
    fclose(fw);
  }

  if (showDialog) {
    // Show crash handler dialog
    CrashHandler crashdialog(TApp::instance()->getMainWindow(), fp,
                             QString::fromStdString(out));
    return crashdialog.exec() != QDialog::Rejected;
  }

  return true;
}

//-----------------------------------------------------------------------------
