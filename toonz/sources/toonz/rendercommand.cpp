

// Tnz6 includes
#include "tapp.h"
#include "mainwindow.h"
#include "formatsettingspopups.h"
#include "fileviewerpopup.h"
#include "flipbook.h"
#include "filebrowsermodel.h"
#include "previewfxmanager.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/toonzscene.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheet.h"
#include "toonz/txsheethandle.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tcamera.h"
#include "toonz/sceneproperties.h"
#include "toonz/onionskinmask.h"
#include "toonz/observer.h"
#include "toonz/scenefx.h"
#include "toonz/movierenderer.h"
#include "toonz/multimediarenderer.h"
#include "toutputproperties.h"

#ifdef _WIN32
#include "avicodecrestrictions.h"
#endif

// TnzBase includes
#include "tenv.h"
#include "trenderer.h"
#include "trasterfx.h"
#include "iocommand.h"

// TnzCore includes
#include "tsystem.h"
#include "tiio.h"
#include "trop.h"
#include "tconvert.h"
#include "tfiletype.h"
#include "timagecache.h"
#include "tthreadmessage.h"

// Qt includes
#include <QObject>
#include <QDesktopServices>
#include <QUrl>

//---------------------------------------------------------

//=========================================================

class OnRenderCompleted final : public TThread::Message {
  TFilePath m_fp;
  bool m_error;

public:
  OnRenderCompleted(const TFilePath &fp, bool error)
      : m_fp(fp), m_error(error) {}

  TThread::Message *clone() const override {
    return new OnRenderCompleted(*this);
  }

  void onDeliver() override {
    if (m_error) {
      m_error = false;
      DVGui::error(
          QObject::tr("There was an error saving frames for the %1 level.")
              .arg(QString::fromStdWString(
                  m_fp.withoutParentDir().getWideString())));
    }

    bool isPreview = (m_fp.getType() == "noext");

    TImageCache::instance()->remove(::to_string(m_fp.getWideString() + L".0"));
    TNotifier::instance()->notify(TSceneNameChange());

    if (Preferences::instance()->isGeneratedMovieViewEnabled()) {
      if (!isPreview && (Preferences::instance()->isDefaultViewerEnabled()) &&
          (m_fp.getType() == "avi" ||
           m_fp.getType() == "mp4" ||
           m_fp.getType() == "gif" || m_fp.getType() == "webm" ||
           m_fp.getType() == "mov")) {
        QString name = QString::fromStdString(m_fp.getName());
        int index;
        if ((index = name.indexOf("#RENDERID")) != -1)  //! quite ugly I
                                                        //! know....
          m_fp = m_fp.withName(name.left(index).toStdWString());

        if (!TSystem::showDocument(m_fp)) {
          QString msg(QObject::tr("It is not possible to display the file %1: "
                                  "no player associated with its format")
                          .arg(QString::fromStdWString(
                              m_fp.withoutParentDir().getWideString())));
          DVGui::warning(msg);
        }

      }

      else {
        int r0, r1, step;
        TApp *app         = TApp::instance();
        ToonzScene *scene = app->getCurrentScene()->getScene();
        TOutputProperties &outputSettings =
            isPreview ? *scene->getProperties()->getPreviewProperties()
                      : *scene->getProperties()->getOutputProperties();
        outputSettings.getRange(r0, r1, step);
        const TRenderSettings rs    = outputSettings.getRenderSettings();
        if (r0 == 0 && r1 == -1) r0 = 0, r1 = scene->getFrameCount() - 1;

        double timeStretchFactor =
            isPreview
                ? 1.0
                : (double)outputSettings.getRenderSettings().m_timeStretchTo /
                      outputSettings.getRenderSettings().m_timeStretchFrom;

        r0 = tfloor(r0 * timeStretchFactor);
        r1 = tceil((r1 + 1) * timeStretchFactor) - 1;

        TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
        prop->m_frameRate              = outputSettings.getFrameRate();
        TSoundTrack *snd =
            app->getCurrentXsheet()->getXsheet()->makeSound(prop);
        // keeps ffmpeg files from being previewed until import is fixed
        if (m_fp.getType() != "mp4" && m_fp.getType() != "webm" &&
            m_fp.getType() != "gif" && m_fp.getType() != "spritesheet" &&
            m_fp.getType() != "mov") {
          if (outputSettings.getRenderSettings().m_stereoscopic) {
            assert(!isPreview);
            ::viewFile(m_fp.withName(m_fp.getName() + "_l"), r0 + 1, r1 + 1,
                       step, isPreview ? rs.m_shrinkX : 1, snd, 0, false, true);
            ::viewFile(m_fp.withName(m_fp.getName() + "_r"), r0 + 1, r1 + 1,
                       step, isPreview ? rs.m_shrinkX : 1, snd, 0, false, true);
          } else
            ::viewFile(m_fp, r0 + 1, r1 + 1, step, isPreview ? rs.m_shrinkX : 1,
                       snd, 0, false, true);
        }
      }
    }
  }
};

//=========================================================

class RenderCommand {
  int m_r0, m_r1, m_step;
  TFilePath m_fp;
  int m_numFrames;
  TPixel32 m_oldBgColor;
  static TPixel32 m_priorBgColor;
  TDimension m_oldCameraRes;
  double m_r, m_stepd;
  double m_timeStretchFactor;

  int m_multimediaRender;

public:
  RenderCommand()
      : m_r0(0)
      , m_r1(-1)
      , m_step(1)
      , m_numFrames(0)
      , m_r(0)
      , m_stepd(1)
      , m_oldCameraRes(0, 0)
      , m_timeStretchFactor(1)
      , m_multimediaRender(0) {
    setCommandHandler("MI_Render", this, &RenderCommand::onRender);
    setCommandHandler("MI_SaveAndRender", this, &RenderCommand::onSaveAndRender);
    setCommandHandler("MI_FastRender", this, &RenderCommand::onFastRender);
    setCommandHandler("MI_Preview", this, &RenderCommand::onPreview);
  }

  bool init(bool isPreview);  // true if r0<=r1
  void rasterRender(bool isPreview);
  void multimediaRender();
  void onRender();
  void onSaveAndRender();
  void onFastRender();
  void onPreview();
  static void resetBgColor();
  void doRender(bool isPreview);
};

RenderCommand renderCommand;

//---------------------------------------------------------

bool RenderCommand::init(bool isPreview) {
  ToonzScene *scene       = TApp::instance()->getCurrentScene()->getScene();
  TSceneProperties *sprop = scene->getProperties();

  // Get the right output settings depending on if it's a preview or not.
  TOutputProperties &outputSettings = isPreview ? *sprop->getPreviewProperties()
                                                : *sprop->getOutputProperties();
  outputSettings.getRange(m_r0, m_r1, m_step);
  /*-- Use the whole scene if m_r1 < 0 --*/
  if (m_r0 == 0 && m_r1 == -1) {
    m_r0 = 0;
    m_r1 = scene->getFrameCount() - 1;
  }
  if (m_r0 < 0) m_r0                       = 0;
  if (m_r1 >= scene->getFrameCount() &&
      !Preferences::instance()->isImplicitHoldEnabled())
    m_r1 = scene->getFrameCount() - 1;
  // nothing to render
  if (m_r1 < m_r0) {
    DVGui::warning(QObject::tr(
        "The command cannot be executed because the scene is empty."));
    return false;
  }

  // Initialize the preview case

  /*TRenderSettings rs = sprop->getPreviewProperties()->getRenderSettings();
TRenderSettings rso = sprop->getOutputProperties()->getRenderSettings();
rs.m_stereoscopic=true;
rs.m_stereoscopicShift=0.05;
rso.m_stereoscopic=true;
rso.m_stereoscopicShift=0.05;
sprop->getPreviewProperties()->setRenderSettings(rs);
sprop->getOutputProperties()->setRenderSettings(rso);*/

  if (isPreview) {
    /*--
     * Since time stretch is not considered in Preview, the frame value is stored as it is
     * --*/
    m_numFrames        = (int)(m_r1 - m_r0 + 1);
    m_r                = m_r0;
    m_stepd            = m_step;
    m_multimediaRender = 0;
    return true;
  }

  // Full render case

  // Read the output filepath
  TFilePath fp = outputSettings.getPath();

  // you cannot render an untitled scene to scene folder
  if (scene->isUntitled() && TFilePath("$scenefolder").isAncestorOf(fp)) {
    DVGui::warning(
        QObject::tr("The scene is not yet saved and the output destination is "
                    "set to $scenefolder.\nSave the scene first."));
    return false;
  }

  /*-- If no file name is specified, use the scene name as the output file name. --*/
  if (fp.getWideName() == L"")
    fp = fp.withName(scene->getScenePath().getName());
  /*-- For raster images, add the frame number to the filename --*/
  if (TFileType::getInfo(fp) == TFileType::RASTER_IMAGE) 
    fp = fp.withFrame(TFrameId::EMPTY_FRAME);
  fp   = scene->decodeFilePath(fp);

  // make sure there is a destination to write to.
  if (!TFileStatus(fp.getParentDir()).doesExist()) {
    try {
      TFilePath parent = fp.getParentDir();
      TSystem::mkDir(parent);
      DvDirModel::instance()->refreshFolder(parent.getParentDir());
    } catch (TException &e) {
      DVGui::warning(
          QObject::tr("It is not possible to create folder : %1")
              .arg(QString::fromStdString(::to_string(e.getMessage()))));
      return false;
    } catch (...) {
      DVGui::warning(QObject::tr("It is not possible to create a folder."));
      return false;
    }
  }
  m_fp = fp;

  // Retrieve camera infos
  const TCamera *camera =
      isPreview ? scene->getCurrentPreviewCamera() : scene->getCurrentCamera();
  TDimension cameraSize = camera->getRes();
  TPointD cameraDpi     = camera->getDpi();

  // Retrieve render interval/step/times
  double stretchTo = (double)outputSettings.getRenderSettings().m_timeStretchTo;
  double stretchFrom =
      (double)outputSettings.getRenderSettings().m_timeStretchFrom;

  m_timeStretchFactor = stretchTo / stretchFrom;
  m_stepd             = m_step / m_timeStretchFactor;

  int stretchedR0 = tfloor(m_r0 * m_timeStretchFactor);
  int stretchedR1 = tceil((m_r1 + 1) * m_timeStretchFactor) - 1;

  m_r         = stretchedR0 / m_timeStretchFactor;
  m_numFrames = (stretchedR1 - stretchedR0) / m_step + 1;

  // Update the multimedia render switch
  m_multimediaRender = outputSettings.getMultimediaRendering();

  return true;
}

//===================================================================

class RenderListener final : public DVGui::ProgressDialog,
                             public MovieRenderer::Listener {
  QString m_progressBarString;
  int m_frameCounter, m_totalFrames;
  TRenderer *m_renderer;
  bool m_error;

  class Message final : public TThread::Message {
    RenderListener *m_pb;
    int m_frame;
    QString m_labelText;

  public:
    Message(RenderListener *pb, int frame, const QString &labelText)
        : TThread::Message()
        , m_pb(pb)
        , m_frame(frame)
        , m_labelText(labelText) {}
    TThread::Message *clone() const override { return new Message(*this); }
    void onDeliver() override {
      if (m_frame == -2) {
        m_pb->setLabelText(
            QObject::tr("Finalizing render, please wait.", "RenderListener"));
        // Set busy indicator to progress bar
        m_pb->setMinimum(0);
        m_pb->setMaximum(0);
        m_pb->setValue(0);
      } else if (m_frame == -1)
        m_pb->hide();
      else {
        m_pb->setLabelText(
            QObject::tr("Rendering frame %1 / %2", "RenderListener")
                .arg(m_frame)
                .arg(m_labelText));
        m_pb->setValue(m_frame);
      }
    }
  };

public:
  RenderListener(TRenderer *renderer, const TFilePath &path, int steps,
                 bool isPreview)
      : DVGui::ProgressDialog(
            QObject::tr("Precomputing %1 Frames", "RenderListener").arg(steps) +
                ((isPreview) ? "" : QObject::tr(" of %1", "RenderListener")
                                        .arg(toQString(path))),
            QObject::tr("Cancel"), 0, steps, TApp::instance()->getMainWindow())
      , m_renderer(renderer)
      , m_frameCounter(0)
      , m_error(false) {
#ifdef MACOSX
    // Modal dialogs seem to be preventing the execution of
    // Qt::BlockingQueuedConnections on MAC...!
    setModal(false);
#else
    setWindowModality(Qt::ApplicationModal);
#endif
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    m_progressBarString =
        QString::number(steps) +
        ((isPreview)
             ? ""
             : QObject::tr(" of %1", "RenderListener").arg(toQString(path)));
    // setMinimumDuration (0);
    m_totalFrames = steps;
    show();
  }

  /*-- 以下３つの関数はMovieRenderer::Listenerの純粋仮想関数の実装 --*/
  bool onFrameCompleted(int frame) override {
    bool ret = wasCanceled();
    if (m_frameCounter + 1 < m_totalFrames)
      Message(this, ret ? -1 : ++m_frameCounter, m_progressBarString).send();
    else
      Message(this, -2, "").send();
    return !ret;
  }
  bool onFrameFailed(int frame, TException &) override {
    m_error = true;
    return onFrameCompleted(frame);
  }
  void onSequenceCompleted(const TFilePath &fp) override {
    Message(this, -1, "").send();
    OnRenderCompleted(fp, m_error).send();
    m_error = false;
    RenderCommand::resetBgColor();
  }

  void onCancel() override {
    m_isCanceled = true;
    setLabelText(QObject::tr("Aborting render...", "RenderListener"));
    reset();
    hide();
    RenderCommand::resetBgColor();
  }
};

//---------------------------------------------------------

void RenderCommand::rasterRender(bool isPreview) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  if (isPreview) {
    // Let the PreviewFxManager own the rest. Just pass him the current output
    // node.
    PreviewFxManager::instance()->showNewPreview(
        (TFx *)scene->getXsheet()->getFxDag()->getCurrentOutputFx());
    return;
  }

  std::string ext = m_fp.getType();

#ifdef _WIN32
  if (ext == "avi" && !isPreview) {
    TPropertyGroup *props =
        scene->getProperties()->getOutputProperties()->getFileFormatProperties(
            ext);
    std::string codecName = props->getProperty(0)->getValueAsString();
    TDimension res        = scene->getCurrentCamera()->getRes();
    if (!AviCodecRestrictions::canWriteMovie(::to_wstring(codecName), res)) {
      QString msg(
          QObject::tr("The resolution of the output camera does not fit with "
                      "the options chosen for the output file format."));
      DVGui::warning(msg);
      return;
    }
  }
#endif

  TPixel32 currBgColor = scene->getProperties()->getBgColor();
  m_priorBgColor       = currBgColor;
  // fixes background colors for non alpha-enabled movie types (eventually
  // transparent gif would be good)
  currBgColor.m = 255;
  // Mov may have alpha channel under some settings (Millions of Colors+ color
  // depth). I tried to make OT to detect the mov settings and adaptively switch
  // the behavior, but ended in vain :-(
  // So I just omitted every mov from applying solid background as a quick fix.
  if (isMovieTypeOpaque(ext)) {
    scene->getProperties()->setBgColor(currBgColor);
  }
  // for non alpha-enabled images (like jpg), background color will be inserted
  // in  TImageWriter::save() (see timage_io.cpp)
  else
    TImageWriter::setBackgroundColor(currBgColor);

  // Extract output properties
  TOutputProperties *prop = isPreview
                                ? scene->getProperties()->getPreviewProperties()
                                : scene->getProperties()->getOutputProperties();

  // Build thread count
  /*-- Dedicated CPUs (Single, Half, All) --*/
  int index = prop->getThreadIndex();

  const int procCount       = TSystem::getProcessorCount();
  const int threadCounts[3] = {1, procCount / 2, procCount};

  int threadCount = threadCounts[index];

  /*-- MovieRenderer --*/
  // construct a renderer
  MovieRenderer movieRenderer(scene, isPreview ? TFilePath() : m_fp,
                              threadCount, isPreview);

  TRenderSettings rs = prop->getRenderSettings();

  // Build raster granularity size
  index = prop->getMaxTileSizeIndex();

  const int maxTileSizes[4] = {
      (std::numeric_limits<int>::max)(), TOutputProperties::LargeVal,
      TOutputProperties::MediumVal, TOutputProperties::SmallVal};
  rs.m_maxTileSize = maxTileSizes[index];

  // Build

  /*-- RenderSettings --*/
  movieRenderer.setRenderSettings(rs);
  /*-- get and set the dpi --*/
  TPointD cameraDpi = isPreview ? scene->getCurrentPreviewCamera()->getDpi()
                                : scene->getCurrentCamera()->getDpi();
  movieRenderer.setDpi(cameraDpi.x, cameraDpi.y);
  // Precomputing causes a long delay for ffmpeg imported types
  if (!isPreview && !Preferences::instance()->getPrecompute())
    movieRenderer.enablePrecomputing(false);
  else
    movieRenderer.enablePrecomputing(true);

  /*-- Creating a progress dialog --*/
  RenderListener *listener =
      new RenderListener(movieRenderer.getTRenderer(), m_fp,
                         ((m_numFrames - 1) / m_step) + 1, isPreview);
  QObject::connect(listener, SIGNAL(canceled()), &movieRenderer,
                   SLOT(onCanceled()));
  movieRenderer.addListener(listener);


  QProgressBar *buildSceneProgressBar =
      new QProgressBar(TApp::instance()->getMainWindow());
  buildSceneProgressBar->setAttribute(Qt::WA_DeleteOnClose);
  buildSceneProgressBar->setWindowFlags(Qt::SubWindow | Qt::Dialog);
  buildSceneProgressBar->setMinimum(0);
  buildSceneProgressBar->setMaximum(m_numFrames - 1);
  buildSceneProgressBar->setValue(0);
  buildSceneProgressBar->move(600, 500);
  buildSceneProgressBar->setWindowTitle(
      QObject::tr("Building Schematic...", "RenderCommand"));
  buildSceneProgressBar->show();


  // Interlacing
  bool fieldRendering = rs.m_fieldPrevalence != TRenderSettings::NoField;


  for (int i = 0; i < m_numFrames; ++i, m_r += m_stepd) {
    buildSceneProgressBar->setValue(i);

    // shift camera if stereoscopic output
    if (rs.m_stereoscopic) scene->shiftCameraX(-rs.m_stereoscopicShift / 2);

    // get the fx for each frame
    // m_frameB is only used for stereoscopic or interlacing
    TFxPair fx;
    fx.m_frameA = buildSceneFx(scene, m_r, rs.m_shrinkX, isPreview);

    if (fieldRendering && !isPreview)
      fx.m_frameB = buildSceneFx(scene, m_r + 0.5 / m_timeStretchFactor,
                                 rs.m_shrinkX, isPreview);
    else if (rs.m_stereoscopic) {
      scene->shiftCameraX(rs.m_stereoscopicShift);
      fx.m_frameB = buildSceneFx(scene, m_r + 0.5 / m_timeStretchFactor,
                                 rs.m_shrinkX, isPreview);
      scene->shiftCameraX(-rs.m_stereoscopicShift / 2);
    } else
      fx.m_frameB = TRasterFxP();
    /*-- movieRenderer Register Fx for each frame --*/
    movieRenderer.addFrame(m_r, fx);
  }

  buildSceneProgressBar->close();

  // resetViewer(); //TODO delete the images of any previous render
  //FileViewerPopupPool::instance()->getCurrent()->onClose();

  movieRenderer.start();
}

TPixel32 RenderCommand::m_priorBgColor;

void RenderCommand::resetBgColor() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->getProperties()->setBgColor(m_priorBgColor);

  // revert background color settings
  TImageWriter::setBackgroundColor(
      Preferences::instance()->getRasterBackgroundColor());
}

//===================================================================

class MultimediaProgressBar final : public DVGui::ProgressDialog,
                                    public MultimediaRenderer::Listener {
  QString m_progressBarString;
  int m_frameCounter;
  int m_columnCounter;
  int m_pbCounter;
  MultimediaRenderer *m_renderer;

  //----------------------------------------------------------

  class Message final : public TThread::Message {
    MultimediaProgressBar *m_pb;
    int m_frame;
    int m_column;
    int m_pbValue;
    QString m_labelText;

  public:
    Message(MultimediaProgressBar *pb, int frame, int column, int pbValue,
            const QString &labelText)
        : TThread::Message()
        , m_pb(pb)
        , m_frame(frame)
        , m_column(column)
        , m_pbValue(pbValue)
        , m_labelText(labelText) {}

    TThread::Message *clone() const override { return new Message(*this); }

    void onDeliver() override {
      if (m_pbValue == -1)
        m_pb->hide();
      else {
        QString modeStr(
            m_pb->m_renderer->getMultimediaMode() == MultimediaRenderer::COLUMNS
                ? QObject::tr("column ",
                              "MultimediaProgressBar label (mode name)")
                : QObject::tr("layer ",
                              "MultimediaProgressBar label (mode name)"));
        m_pb->setLabelText(QObject::tr("Rendering %1%2, frame %3 / %4",
                                       "MultimediaProgressBar label")
                               .arg(modeStr)
                               .arg(m_column)
                               .arg(m_frame)
                               .arg(m_labelText));
        m_pb->setValue(m_pbValue);
      }
    }
  };

  //----------------------------------------------------------

public:
  MultimediaProgressBar(MultimediaRenderer *renderer)
      : ProgressDialog(
            QObject::tr("Rendering %1 frames of %2", "MultimediaProgressBar")
                .arg(renderer->getFrameCount())
                .arg(toQString(renderer->getFilePath())),
            QObject::tr("Cancel"), 0,
            renderer->getFrameCount() * renderer->getColumnsCount())
      , m_renderer(renderer)
      , m_frameCounter(0)
      , m_columnCounter(0)
      , m_pbCounter(0) {
#ifdef MACOSX
    // Modal dialogs seem to be preventing the execution of
    // Qt::BlockingQueuedConnections on MAC...!
    setModal(false);
#else
    setWindowModality(Qt::ApplicationModal);
#endif
    setWindowFlags(Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    m_progressBarString =
        QObject::tr("%1 of %2",
                    "MultimediaProgressBar - [totalframe] of [path]")
            .arg(m_renderer->getFrameCount())
            .arg(toQString(m_renderer->getFilePath()));
    show();
  }

  MultimediaRenderer *getRenderer() { return m_renderer; }

  bool onFrameCompleted(int frame, int column) override {
    bool ret = wasCanceled();
    Message(this, ++m_frameCounter, m_columnCounter, ret ? -1 : ++m_pbCounter,
            m_progressBarString)
        .send();
    return !ret;
  }

  bool onFrameFailed(int frame, int column, TException &) override {
    return onFrameCompleted(frame, column);
  }

  void onSequenceCompleted(int column) override {
    m_frameCounter = 0;
    Message(this, m_frameCounter, ++m_columnCounter, m_pbCounter,
            m_progressBarString)
        .send();
    // OnRenderCompleted(fp).send();   //No output should have been put in cache
    // - and no viewfile to be called...
  }

  void onRenderCompleted() override {
    Message(this, -1, -1, -1, "").send();
    // OnRenderCompleted(fp).send();
  }

  void onCancel() override {
    m_isCanceled = true;
    setLabelText(QObject::tr("Aborting render...", "MultimediaProgressBar"));
    TRenderer *trenderer(m_renderer->getTRenderer());
    if (m_renderer) trenderer->stopRendering(true);
    reset();
    hide();
  }
};

//---------------------------------------------------------

//! Specialized render invocation for multimedia rendering.
void RenderCommand::multimediaRender() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  std::string ext   = m_fp.getType();

#ifdef _WIN32
  if (ext == "avi") {
    TPropertyGroup *props =
        scene->getProperties()->getOutputProperties()->getFileFormatProperties(
            ext);
    std::string codecName = props->getProperty(0)->getValueAsString();
    TDimension res        = scene->getCurrentCamera()->getRes();
    if (!AviCodecRestrictions::canWriteMovie(::to_wstring(codecName), res)) {
      QString msg(
          QObject::tr("The resolution of the output camera does not fit with "
                      "the options chosen for the output file format."));
      DVGui::warning(msg);
      return;
    }
  }
#endif

  TOutputProperties *prop = scene->getProperties()->getOutputProperties();

  // Build thread count
  int index = prop->getThreadIndex();

  const int procCount       = TSystem::getProcessorCount();
  const int threadCounts[3] = {1, procCount / 2, procCount};

  int threadCount = threadCounts[index];

  // Build raster granularity size
  index = prop->getMaxTileSizeIndex();

  const int maxTileSizes[4] = {
      (std::numeric_limits<int>::max)(), TOutputProperties::LargeVal,
      TOutputProperties::MediumVal, TOutputProperties::SmallVal};

  TRenderSettings rs = prop->getRenderSettings();
  rs.m_maxTileSize   = maxTileSizes[index];

  MultimediaRenderer multimediaRenderer(
      scene, m_fp, prop->getMultimediaRendering(), threadCount);
  multimediaRenderer.setRenderSettings(rs);

  TPointD cameraDpi = scene->getCurrentCamera()->getDpi();
  multimediaRenderer.setDpi(cameraDpi.x, cameraDpi.y);
  multimediaRenderer.enablePrecomputing(true);

  for (int i = 0; i < m_numFrames; ++i, m_r += m_stepd)
    multimediaRenderer.addFrame(m_r);

  MultimediaProgressBar *listener =
      new MultimediaProgressBar(&multimediaRenderer);
  QObject::connect(listener, SIGNAL(canceled()), &multimediaRenderer,
                   SLOT(onCanceled()));
  multimediaRenderer.addListener(listener);

  multimediaRenderer.start();
}

//===================================================================

void RenderCommand::onRender() {
  doRender(false);
}

//===================================================================

void RenderCommand::onSaveAndRender() {
    bool saved = false;
    saved = IoCmd::saveAll();
    if (!saved) {
        return;
    }
    doRender(false);
}

//===================================================================

void RenderCommand::onFastRender() {
  TOutputProperties *prop = TApp::instance()
                                ->getCurrentScene()
                                ->getScene()
                                ->getProperties()
                                ->getOutputProperties();
  QString sceneName = QString::fromStdWString(
      TApp::instance()->getCurrentScene()->getScene()->getSceneName());
  QString location = Preferences::instance()->getFastRenderPath();
  if (location == "desktop" || location == "Desktop") {
    location =
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  }
  TFilePath path     = TFilePath(location) + TFilePath(sceneName + ".mp4");
  TFilePath currPath = prop->getPath();

  QStringList formats;
  TImageWriter::getSupportedFormats(formats, true);
  TLevelWriter::getSupportedFormats(formats, true);
  Tiio::Writer::getSupportedFormats(formats, true);
  if (!formats.contains("mp4")) {
    QString msg = QObject::tr(
        "FFmpeg not found, please set the location in the Preferences and "
        "restart.");
    DVGui::warning(msg);
    return;
  }
  prop->setPath(path);
  doRender(false);
  prop->setPath(currPath);
}

//---------------------------------------------------------

void RenderCommand::onPreview() { doRender(true); }

//---------------------------------------------------------

void RenderCommand::doRender(bool isPreview) {
  bool isWritable = true;
  bool isMultiFrame;
  /*--
   * Initialization process. Calculate the frame range, and in the case of Render, also create the save destination path from Output Settings
   * --*/
  if (!init(isPreview)) return;

  // file name stuff
  // and check ability to write.
  if (m_fp.getDots() == ".") {
    isMultiFrame = false;
    TFileStatus fs(m_fp);
    if (fs.doesExist()) isWritable = fs.isWritable();
  } else {
    isMultiFrame  = true;
    TFilePath dir = m_fp.getParentDir();
    QDir qDir(QString::fromStdWString(dir.getWideString()));
    QString levelName =
        QRegExp::escape(QString::fromStdWString(m_fp.getWideName()));
    QString levelType = QString::fromStdString(m_fp.getType());
    QString exp(levelName + ".[0-9]{1,4}." + levelType);
    QRegExp regExp(exp);
    QStringList list        = qDir.entryList(QDir::Files);
    QStringList levelFrames = list.filter(regExp);

    int i;
    for (i = 0; i < levelFrames.size() && isWritable; i++) {
      TFilePath frame = dir + TFilePath(levelFrames[i].toStdWString());
      if (frame.isEmpty() || !frame.isAbsolute()) continue;
      TFileStatus fs(frame);
      isWritable = fs.isWritable();
    }
  }
  if (!isWritable) {
    QString str = QObject::tr(
        "It is not possible to write the output:  the file", "RenderCommand");
    str += isMultiFrame ? QObject::tr("s are read only.", "RenderCommand")
                        : QObject::tr(" is read only.", "RenderCommand");
    DVGui::warning(str);
    return;
  }

  ToonzScene *scene = 0;
  TCamera *camera   = 0;

  // send it to the appropriate renderer
  try {
    if (m_multimediaRender)
      multimediaRender();

    else
      rasterRender(isPreview);
  } catch (TException &e) {
    DVGui::warning(QString::fromStdString(::to_string(e.getMessage())));
  } catch (...) {
    DVGui::warning(
        QObject::tr("It is not possible to complete the rendering."));
  }
}

//===================================================================
