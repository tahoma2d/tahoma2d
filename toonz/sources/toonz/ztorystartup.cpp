#include "ztorystartup.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

// ─── Config ────────────────────────────────────────────────────────────────

ZtoryStartupDialog::Config::Config()
  : width(1920), height(1080), fps(24), totalFrames(0)
  , workflow(Storyboard)
  , numberingStyle(Simple)
  , step(10), padding(3), seqPadding(2), startNumber(10), initialShotCount(1)
  , seqPrefix("sq"), shotPrefix("sh")
{}

QString ZtoryStartupDialog::Config::shotName(int sq, int idx) const {
  int number = startNumber + idx * step;
  if (numberingStyle == Sequence) {
    return QString("%1%2_%3%4")
        .arg(seqPrefix)
        .arg(sq, seqPadding, 10, QChar('0'))
        .arg(shotPrefix)
        .arg(number, padding, 10, QChar('0'));
  } else {
    return QString("%1%2")
        .arg(shotPrefix)
        .arg(number, padding, 10, QChar('0'));
  }
}

// ─── Format presets ────────────────────────────────────────────────────────

struct FormatPreset { const char *name; int w, h, fps; };
static const FormatPreset kPresets[] = {
  { "HDTV 1080p (1920×1080) 24fps",   1920, 1080, 24 },
  { "HDTV 1080p (1920×1080) 25fps",   1920, 1080, 25 },
  { "HDTV 1080p (1920×1080) 30fps",   1920, 1080, 30 },
  { "HDTV 720p  (1280×720)  24fps",   1280,  720, 24 },
  { "2K DCI     (2048×1080) 24fps",   2048, 1080, 24 },
  { "4K UHD     (3840×2160) 24fps",   3840, 2160, 24 },
  { "PAL SD     (768×576)   25fps",    768,  576, 25 },
  { "NTSC SD    (720×486)   30fps",    720,  486, 30 },
  { "Custom",                             0,    0,  0 },
};
static const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

// ─── Constructor ───────────────────────────────────────────────────────────

ZtoryStartupDialog::ZtoryStartupDialog(QWidget *parent)
  : QDialog(parent) {
  setWindowTitle(tr("New Ztoryc Project"));
  setMinimumWidth(520);
  buildUi();
  loadSettings();
}

// ─── buildUi ───────────────────────────────────────────────────────────────

void ZtoryStartupDialog::buildUi() {
  auto *mainLay = new QVBoxLayout(this);
  mainLay->setSpacing(12);
  mainLay->setContentsMargins(16, 16, 16, 12);

  // ── Section 1: Project ─────────────────────────────────────────────────
  auto *projGroup = new QGroupBox(tr("Project"), this);
  auto *projGrid  = new QGridLayout(projGroup);
  projGrid->setColumnStretch(1, 1);

  m_projectName = new QLineEdit(this);
  m_projectName->setPlaceholderText(tr("my_storyboard"));
  // Allow only filesystem-safe characters
  m_projectName->setValidator(
      new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9_-]+"), this));
  projGrid->addWidget(new QLabel(tr("Project name:")), 0, 0);
  projGrid->addWidget(m_projectName, 0, 1, 1, 2);

  m_projectPath = new QLineEdit(this);
  m_projectPath->setText(
      QDir::toNativeSeparators(
          QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
          + "/Ztoryc Projects"));
  m_browseBtn = new QPushButton(tr("Browse…"), this);
  m_browseBtn->setMaximumWidth(80);
  projGrid->addWidget(new QLabel(tr("Location:")), 1, 0);
  projGrid->addWidget(m_projectPath, 1, 1);
  projGrid->addWidget(m_browseBtn, 1, 2);

  connect(m_browseBtn, &QPushButton::clicked, this, [this]() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Choose project location"),
        m_projectPath->text());
    if (!dir.isEmpty())
      m_projectPath->setText(QDir::toNativeSeparators(dir));
  });

  mainLay->addWidget(projGroup);

  // ── Section 2: Camera / Resolution ────────────────────────────────────
  auto *camGroup = new QGroupBox(tr("Camera / Resolution"), this);
  auto *camGrid  = new QGridLayout(camGroup);
  camGrid->setColumnStretch(1, 1);

  m_formatPreset = new QComboBox(this);
  for (int i = 0; i < kPresetCount; i++)
    m_formatPreset->addItem(tr(kPresets[i].name));
  camGrid->addWidget(new QLabel(tr("Format:")), 0, 0);
  camGrid->addWidget(m_formatPreset, 0, 1, 1, 3);

  m_width  = new QSpinBox(this);  m_width->setRange(1, 8192);  m_width->setValue(1920);
  m_height = new QSpinBox(this);  m_height->setRange(1, 8192); m_height->setValue(1080);
  m_fps    = new QSpinBox(this);  m_fps->setRange(1, 120);     m_fps->setValue(24);
  camGrid->addWidget(new QLabel(tr("Width:")),  1, 0);
  camGrid->addWidget(m_width,                   1, 1);
  camGrid->addWidget(new QLabel(tr("Height:")), 1, 2);
  camGrid->addWidget(m_height,                  1, 3);
  camGrid->addWidget(new QLabel(tr("FPS:")),    2, 0);
  camGrid->addWidget(m_fps,                     2, 1);

  m_totalFrames = new QSpinBox(this);
  m_totalFrames->setRange(0, 99999);
  m_totalFrames->setValue(0);
  m_totalFrames->setSpecialValueText(tr("Undefined"));
  camGrid->addWidget(new QLabel(tr("Duration (frames):")), 2, 2);
  camGrid->addWidget(m_totalFrames, 2, 3);

  connect(m_formatPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ZtoryStartupDialog::onFormatPresetChanged);

  mainLay->addWidget(camGroup);

  // ── Section 3: Workflow ────────────────────────────────────────────────
  auto *wfGroup = new QGroupBox(tr("Workflow"), this);
  auto *wfLay   = new QHBoxLayout(wfGroup);

  m_workflowGroup = new QButtonGroup(this);
  auto *rb0 = new QRadioButton(tr("STORYBOARD"), this);
  auto *rb1 = new QRadioButton(tr("ANIMATIC"),   this);
  auto *rb2 = new QRadioButton(tr("STOPMOTION"), this);
  rb0->setChecked(true);
  m_workflowGroup->addButton(rb0, 0);
  m_workflowGroup->addButton(rb1, 1);
  m_workflowGroup->addButton(rb2, 2);
  wfLay->addWidget(rb0);
  wfLay->addWidget(rb1);
  wfLay->addWidget(rb2);
  wfLay->addStretch(1);

  mainLay->addWidget(wfGroup);

  // ── Section 4: Shot Numbering ──────────────────────────────────────────
  auto *numGroup = new QGroupBox(tr("Shot Numbering"), this);
  auto *numGrid  = new QGridLayout(numGroup);
  numGrid->setColumnStretch(1, 1);

  m_numberingStyle = new QComboBox(this);
  m_numberingStyle->addItem(tr("Simple  — sh010, sh020, sh030…"));
  m_numberingStyle->addItem(tr("Sequence — sq01_sh010, sq01_sh020…"));
  numGrid->addWidget(new QLabel(tr("Style:")), 0, 0);
  numGrid->addWidget(m_numberingStyle, 0, 1, 1, 3);

  m_seqPrefixLabel = new QLabel(tr("Sequence prefix:"), this);
  m_seqPrefix = new QLineEdit("sq", this);
  m_seqPrefix->setMaximumWidth(60);
  numGrid->addWidget(m_seqPrefixLabel, 1, 0);
  numGrid->addWidget(m_seqPrefix,      1, 1);

  m_shotPrefix = new QLineEdit("sh", this);
  m_shotPrefix->setMaximumWidth(60);
  numGrid->addWidget(new QLabel(tr("Shot prefix:")), 1, 2);
  numGrid->addWidget(m_shotPrefix, 1, 3);

  m_numberingStep = new QSpinBox(this);
  m_numberingStep->setRange(1, 1000);
  m_numberingStep->setValue(10);
  numGrid->addWidget(new QLabel(tr("Step:")), 2, 0);
  numGrid->addWidget(m_numberingStep, 2, 1);

  m_numPadding = new QSpinBox(this);
  m_numPadding->setRange(1, 6);
  m_numPadding->setValue(3);
  numGrid->addWidget(new QLabel(tr("Padding:")), 2, 2);
  numGrid->addWidget(m_numPadding, 2, 3);

  m_startNumber = new QSpinBox(this);
  m_startNumber->setRange(1, 9999);
  m_startNumber->setValue(10);
  numGrid->addWidget(new QLabel(tr("Start number:")), 3, 0);
  numGrid->addWidget(m_startNumber, 3, 1);

  m_initialShotCount = new QSpinBox(this);
  m_initialShotCount->setRange(0, 100);
  m_initialShotCount->setValue(1);
  numGrid->addWidget(new QLabel(tr("Initial shots:")), 3, 2);
  numGrid->addWidget(m_initialShotCount, 3, 3);

  connect(m_numberingStyle, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &ZtoryStartupDialog::onNumberingStyleChanged);

  // Hide sequence prefix initially (Simple mode by default)
  m_seqPrefixLabel->hide();
  m_seqPrefix->hide();

  mainLay->addWidget(numGroup);

  // ── Buttons ────────────────────────────────────────────────────────────
  auto *bbox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  bbox->button(QDialogButtonBox::Ok)->setText(tr("Create Project"));
  connect(bbox, &QDialogButtonBox::accepted, this, &ZtoryStartupDialog::onAccepted);
  connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
  mainLay->addWidget(bbox);
}

// ─── Slots ─────────────────────────────────────────────────────────────────

void ZtoryStartupDialog::onNumberingStyleChanged(int idx) {
  bool isSeq = (idx == 1);
  m_seqPrefixLabel->setVisible(isSeq);
  m_seqPrefix->setVisible(isSeq);
}

void ZtoryStartupDialog::onFormatPresetChanged(int idx) {
  if (idx < 0 || idx >= kPresetCount) return;
  const FormatPreset &p = kPresets[idx];
  if (p.w == 0) return;  // Custom — don't override
  m_width->setValue(p.w);
  m_height->setValue(p.h);
  m_fps->setValue(p.fps);
}

void ZtoryStartupDialog::onAccepted() {
  // Basic validation
  if (m_projectName->text().trimmed().isEmpty()) {
    m_projectName->setFocus();
    return;
  }
  saveSettings();
  accept();
}

// ─── Config extraction ─────────────────────────────────────────────────────

ZtoryStartupDialog::Config ZtoryStartupDialog::config() const {
  Config c;
  c.projectName  = m_projectName->text().trimmed();
  c.projectPath  = QDir::fromNativeSeparators(m_projectPath->text().trimmed());
  c.width        = m_width->value();
  c.height       = m_height->value();
  c.fps          = m_fps->value();
  c.totalFrames  = m_totalFrames->value();

  int wfId = m_workflowGroup->checkedId();
  c.workflow = (wfId == 1) ? Config::Animatic
             : (wfId == 2) ? Config::StopMotion
                           : Config::Storyboard;

  c.numberingStyle  = (m_numberingStyle->currentIndex() == 1)
                      ? Config::Sequence : Config::Simple;
  c.step            = m_numberingStep->value();
  c.padding         = m_numPadding->value();
  c.seqPadding      = 2;
  c.startNumber     = m_startNumber->value();
  c.initialShotCount = m_initialShotCount->value();
  c.seqPrefix       = m_seqPrefix->text();
  c.shotPrefix      = m_shotPrefix->text();
  return c;
}

// ─── Settings persistence ──────────────────────────────────────────────────

void ZtoryStartupDialog::saveSettings() {
  QSettings s("Ztoryc", "StartupDefaults");
  s.setValue("projectPath",      m_projectPath->text());
  s.setValue("formatPreset",     m_formatPreset->currentIndex());
  s.setValue("width",            m_width->value());
  s.setValue("height",           m_height->value());
  s.setValue("fps",              m_fps->value());
  s.setValue("workflow",         m_workflowGroup->checkedId());
  s.setValue("numberingStyle",   m_numberingStyle->currentIndex());
  s.setValue("step",             m_numberingStep->value());
  s.setValue("padding",          m_numPadding->value());
  s.setValue("startNumber",      m_startNumber->value());
  s.setValue("initialShotCount", m_initialShotCount->value());
  s.setValue("seqPrefix",        m_seqPrefix->text());
  s.setValue("shotPrefix",       m_shotPrefix->text());
}

void ZtoryStartupDialog::loadSettings() {
  QSettings s("Ztoryc", "StartupDefaults");
  if (!s.contains("projectPath")) return;  // first run — keep defaults

  m_projectPath->setText(s.value("projectPath").toString());
  m_formatPreset->setCurrentIndex(s.value("formatPreset", 0).toInt());
  m_width->setValue(s.value("width",  1920).toInt());
  m_height->setValue(s.value("height", 1080).toInt());
  m_fps->setValue(s.value("fps", 24).toInt());

  int wfId = s.value("workflow", 0).toInt();
  if (auto *btn = m_workflowGroup->button(wfId))
    btn->setChecked(true);

  m_numberingStyle->setCurrentIndex(s.value("numberingStyle", 0).toInt());
  m_numberingStep->setValue(s.value("step", 10).toInt());
  m_numPadding->setValue(s.value("padding", 3).toInt());
  m_startNumber->setValue(s.value("startNumber", 10).toInt());
  m_initialShotCount->setValue(s.value("initialShotCount", 1).toInt());
  m_seqPrefix->setText(s.value("seqPrefix", "sq").toString());
  m_shotPrefix->setText(s.value("shotPrefix", "sh").toString());
}
