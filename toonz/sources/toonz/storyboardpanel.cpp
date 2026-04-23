#include "storyboardpanel.h"

#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcell.h"
#include "toonz/childstack.h"
#include "toonz/txshchildlevel.h"
#include "toonz/tstageobjecttree.h"
#include "columncommand.h"
#include "toonz/tstageobject.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshlevelcolumn.h"
#include "toonzqt/stageobjectsdata.h"
#include "tfxattributes.h"
#include "toonz/fxdag.h"
#include "expressionreferencemanager.h"
#include "toonz/tframehandle.h"
#include "toonzqt/menubarcommand.h"
#include "toonz/tstageobjectid.h"
#include "toonz/tstageobject.h"
#include "mainwindow.h"
#include "toonzqt/gutil.h"

#include <QVBoxLayout>
#include <QKeyEvent>
#include <QShortcut>
#include <QRadioButton>
#include "iocommand.h"
#include "subscenecommand.h"
#include "columnselection.h"
#include "toonz/tproject.h"
#include "tsystem.h"
#include "tsystem.h"
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <set>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QSpinBox>
#include <QFrame>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QMessageBox>
#include <QFileDialog>
#include <QPainter>
#include <QPdfWriter>
#include <QPageLayout>
#include <QSizePolicy>
#include <QResizeEvent>
#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QTimer>
#include <QComboBox>
#include <QStackedWidget>
#include <QApplication>

// Strip leading alphabetic characters from a label; optionally capture the prefix.
// E.g. "SH010" → "010" (prefix="SH"),  "010" → "010" (prefix=""),  "SQ001" → "001"
static QString stripAlphaPrefix(const QString &s, QString *prefix = nullptr) {
  int i = 0;
  while (i < s.length() && s[i].isLetter()) i++;
  if (prefix) *prefix = s.left(i);
  return s.mid(i);
}

// Merge helpers defined in ztoryanimatic.cpp (non-static so they can be shared)
void materializeCells(TXshChildLevel *cl, int duration);
void trimChildXsheetTo(TXshChildLevel *cl, int keepFrames);
void mergeChildXsheetContent(TXshChildLevel *dstCl, TXshChildLevel *srcCl,
                              int dstOffset, int srcDuration);

PanelWidget::PanelWidget(QWidget *parent)
    : QFrame(parent)
    , m_shotIndex(0)
    , m_panelIndex(0)
    , m_panelCount(1)
    , m_fps(24)
    , m_selected(false)
{
  setMinimumWidth(200);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setAcceptDrops(true);
  updateBorderStyle();

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setSpacing(2);
  layout->setContentsMargins(4, 4, 4, 4);

  QWidget *header = new QWidget();
  header->setStyleSheet("background-color:#3a3a3a; border-radius:2px;");
  QHBoxLayout *hl = new QHBoxLayout(header);
  hl->setContentsMargins(6, 3, 6, 3);
  hl->setSpacing(4);

  auto lbl = [](const QString &t) {
    QLabel *l = new QLabel(t);
    l->setStyleSheet("color:#aaa; font-size:10px;");
    return l;
  };
  auto val = [](const QString &t) {
    QLabel *l = new QLabel(t);
    l->setStyleSheet("color:#fff; font-size:10px; font-weight:bold;");
    return l;
  };

  // SQ field — editable sequence label; shows number only (prefix stored in m_storedSeqPrefix)
  m_seqField = new QLineEdit();
  m_seqField->setFixedWidth(42);
  m_seqField->setPlaceholderText("—");
  m_seqField->setStyleSheet(
    "QLineEdit{color:#88aaff;background:#3a3a3a;border:none;font-size:10px;font-weight:bold;padding:0 2px;}"
    "QLineEdit:focus{background:#444;border:1px solid #88aaff;}");
  m_storedSeqPrefix = "SQ";  // default prefix

  // SH field — editable shot label; shows number only (prefix stored in m_storedShotPrefix)
  m_shotLabel = new QLineEdit();
  m_shotLabel->setFixedWidth(42);
  m_storedShotPrefix = "SH";  // default prefix
  m_shotLabel->setStyleSheet(
    "QLineEdit{color:#fff;background:#3a3a3a;border:none;font-size:10px;font-weight:bold;padding:0 2px;}"
    "QLineEdit:focus{background:#555;border:1px solid #888;}");
  m_panelLabel = val("1/1");

  // D: durata parziale panel — read-only, derivata dalla subscene
  m_durationSpin = new QSpinBox();
  m_durationSpin->setRange(1, 99999);
  m_durationSpin->setValue(24);
  m_durationSpin->setFixedWidth(52);
  m_durationSpin->setReadOnly(true);
  m_durationSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  m_durationSpin->setStyleSheet(
    "QSpinBox{background:#333;color:#aaa;border:1px solid #444;font-size:10px;padding:1px;}");

  m_durationLabel = new QLabel("00:00:00");
  m_durationLabel->setStyleSheet("color:#88aaff; font-size:10px;");

  m_totalLabel = new QLabel("T:00:00");
  m_totalLabel->setStyleSheet("color:#aaffaa; font-size:10px;");

  // T: durata totale shot — editabile solo nel panel 0
  m_totalSpin = new QSpinBox();
  m_totalSpin->setRange(1, 99999);
  m_totalSpin->setValue(24);
  m_totalSpin->setFixedWidth(52);
  m_totalSpin->setStyleSheet(
    "QSpinBox{background:#222;color:#aaffaa;border:1px solid #555;font-size:10px;padding:1px;}");

  m_matchButton = new QPushButton("\u21d4");  // ⇔ match timeline to sub-scene
  m_matchButton->setFixedSize(22, 18);
  m_matchButton->setToolTip("Match timeline duration to sub-scene actual duration");
  m_matchButton->setStyleSheet(
    "QPushButton{background:#444;color:#ffcc55;border-radius:3px;font-size:10px;}"
    "QPushButton:hover{background:#666;}");

  hl->addWidget(lbl("SQ:"));
  hl->addWidget(m_seqField);
  hl->addWidget(lbl("SH:"));
  hl->addWidget(m_shotLabel);
  hl->addWidget(lbl("P:"));
  hl->addWidget(m_panelLabel);
  hl->addWidget(lbl("D:"));
  hl->addWidget(m_durationSpin);
  hl->addWidget(lbl("T:"));
  hl->addWidget(m_totalSpin);
  hl->addWidget(m_matchButton);
  hl->addStretch();
  layout->addWidget(header);

  m_previewLabel = new QLabel();
  m_previewLabel->setAlignment(Qt::AlignCenter);
  m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_previewLabel->setMinimumHeight(100);
  m_previewLabel->setStyleSheet("QLabel{background:#f0f0eb;border:none;color:#bbb;}");
  layout->addWidget(m_previewLabel);

  layout->addWidget(makeFieldLabel("Dialog"));
  m_dialogField = new QTextEdit();
  m_dialogField->setPlaceholderText("Enter dialogue...");
  m_dialogField->setFixedHeight(68);
  m_dialogField->setStyleSheet(
    "QTextEdit{background:#2a2a2a;color:#eee;border:1px solid #444;font-size:11px;padding:2px;}");
  layout->addWidget(m_dialogField);

  layout->addWidget(makeFieldLabel("Action Notes"));
  m_actionField = new QTextEdit();
  m_actionField->setPlaceholderText("Enter action notes...");
  m_actionField->setFixedHeight(68);
  m_actionField->setStyleSheet(
    "QTextEdit{background:#2a2a2a;color:#eee;border:1px solid #444;font-size:11px;padding:2px;}");
  layout->addWidget(m_actionField);

  layout->addWidget(makeFieldLabel("Notes"));
  m_notesField = new QTextEdit();
  m_notesField->setPlaceholderText("Enter notes...");
  m_notesField->setFixedHeight(68);
  m_notesField->setStyleSheet(
    "QTextEdit{background:#2a2a2a;color:#eee;border:1px solid #444;font-size:11px;padding:2px;}");
  layout->addWidget(m_notesField);

  connect(m_matchButton, &QPushButton::clicked, this,
          [this](){ emit matchDurationRequested(m_shotIndex); });
  connect(m_shotLabel, &QLineEdit::editingFinished, [this](){
    emit dataChanged(m_shotIndex, m_panelIndex);
  });
  connect(m_seqField, &QLineEdit::editingFinished, [this](){
    QString entered = m_seqField->text().trimmed();
    if (entered.isEmpty()) return;
    // Reconstruct full sequence label: if user typed digits only, prepend stored prefix;
    // if they typed something like "SQ020", use the alpha part as the new prefix.
    QString fullLabel;
    if (!entered.isEmpty() && entered[0].isLetter()) {
      QString pfx;
      stripAlphaPrefix(entered, &pfx);
      if (!pfx.isEmpty()) m_storedSeqPrefix = pfx;
      fullLabel = entered;  // already has prefix
    } else {
      fullLabel = m_storedSeqPrefix + entered;
    }
    emit seqLabelEdited(m_shotIndex, fullLabel);
  });
  connect(m_totalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [this](int frames){ emit totalDurationChanged(frames); });
  connect(m_durationSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &PanelWidget::onDurationSpinChanged);
  connect(m_dialogField, &QTextEdit::textChanged,
          [this](){ emit dataChanged(m_shotIndex, m_panelIndex); });
  connect(m_actionField, &QTextEdit::textChanged,
          [this](){ emit dataChanged(m_shotIndex, m_panelIndex); });
  connect(m_notesField, &QTextEdit::textChanged,
          [this](){ emit dataChanged(m_shotIndex, m_panelIndex); });
  setDuration(24);
}

QLabel* PanelWidget::makeFieldLabel(const QString &text) {
  QLabel *l = new QLabel(text);
  l->setStyleSheet(
    "color:#aaa;font-size:10px;font-weight:bold;"
    "background:#333;padding:1px 4px;border-top:1px solid #555;");
  return l;
}

QString PanelWidget::framesToTimecode(int frames) const {
  int ff = frames % m_fps;
  int ts = frames / m_fps;
  int ss = ts % 60;
  int mm = ts / 60;
  return QString("%1:%2:%3")
    .arg(mm, 2, 10, QChar(48))
    .arg(ss, 2, 10, QChar(48))
    .arg(ff, 2, 10, QChar(48));
}

void PanelWidget::updateBorderStyle() {
  if (m_selected)
    setStyleSheet("PanelWidget{background:#2b2b2b;border:1px solid #e05a00;border-radius:3px;box-shadow:0 0 0 2px #e05a00;}");
  else
    setStyleSheet("PanelWidget{background:#2b2b2b;border:1px solid #555;border-radius:3px;}"
                  "PanelWidget:hover{border:1px solid #888;}");
}

void PanelWidget::rescalePreview() {
  int w = width() - 8;
  if (w <= 0) w = 200;
  int h = w * 9 / 16;
  m_previewLabel->setFixedHeight(h);
  if (not m_previewPixmap.isNull())
    m_previewLabel->setPixmap(
      m_previewPixmap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void PanelWidget::setShotIndex(int si) { m_shotIndex = si; }

void PanelWidget::setPanelIndex(int pi, int total) {
  m_panelIndex = pi;
  m_panelCount = total;
  m_panelLabel->setText(QString("%1/%2").arg(pi + 1).arg(total));
  // T: editabile solo nel primo panel
  m_totalSpin->setReadOnly(pi != 0);
  m_totalSpin->setButtonSymbols(pi == 0 ? QAbstractSpinBox::UpDownArrows : QAbstractSpinBox::NoButtons);
  m_totalSpin->setStyleSheet(pi == 0
    ? "QSpinBox{background:#222;color:#aaffaa;border:1px solid #555;font-size:10px;padding:1px;}"
    : "QSpinBox{background:#333;color:#666;border:1px solid #444;font-size:10px;padding:1px;}");
}

void PanelWidget::setShotNumber(const QString &label) {
  // Split "SQ001_SH010" → sqPart="SQ001", shPart="SH010"
  // or    "SH010"       → sqPart="",      shPart="SH010"
  QString sqPart, shPart;
  int sep = label.indexOf('_');
  if (sep >= 0) {
    sqPart = label.left(sep);
    shPart = label.mid(sep + 1);
  } else {
    shPart = label;
  }

  // SH field: strip alpha prefix, store it, show only the numeric part
  QString shNum = stripAlphaPrefix(shPart, &m_storedShotPrefix);
  m_shotLabel->blockSignals(true);
  m_shotLabel->setText(shNum.isEmpty() ? shPart : shNum);
  m_shotLabel->blockSignals(false);

  // SQ field: if the label carried a SQ part, update it too
  if (!sqPart.isEmpty()) {
    QString seqNum = stripAlphaPrefix(sqPart, &m_storedSeqPrefix);
    m_seqField->blockSignals(true);
    m_seqField->setText(seqNum.isEmpty() ? sqPart : seqNum);
    m_seqField->blockSignals(false);
  }
}

QString PanelWidget::shotNumber() const {
  // Reconstruct full shot label with its stored prefix (e.g. "SH" + "010" → "SH010")
  return m_storedShotPrefix + m_shotLabel->text();
}

void PanelWidget::setFps(int fps) {
  m_fps = fps;
  m_durationLabel->setText(framesToTimecode(m_durationSpin->value()));
}

void PanelWidget::setDuration(int frames) {
  m_durationSpin->blockSignals(true);
  m_durationSpin->setValue(frames);
  m_durationSpin->blockSignals(false);
  m_durationLabel->setText(framesToTimecode(frames));
}

void PanelWidget::setTotalDuration(int frames) {
  m_totalLabel->setText("T:" + framesToTimecode(frames));
  m_totalSpin->blockSignals(true);
  m_totalSpin->setValue(frames);
  m_totalSpin->blockSignals(false);
}

void PanelWidget::setPreviewPixmap(const QPixmap &px) {
  m_previewPixmap = px;
  rescalePreview();
}

void PanelWidget::setSelected(bool sel) {
  m_selected = sel;
  updateBorderStyle();
}

void PanelWidget::setDialog(const QString &t) {
  m_dialogField->blockSignals(true);
  m_dialogField->setPlainText(t);
  m_dialogField->blockSignals(false);
}

void PanelWidget::setAction(const QString &t) {
  m_actionField->blockSignals(true);
  m_actionField->setPlainText(t);
  m_actionField->blockSignals(false);
}

void PanelWidget::setNotes(const QString &t) {
  m_notesField->blockSignals(true);
  m_notesField->setPlainText(t);
  m_notesField->blockSignals(false);
}

int     PanelWidget::duration() const { return m_durationSpin->value(); }
QString PanelWidget::dialog()   const { return m_dialogField->toPlainText(); }
QString PanelWidget::action()   const { return m_actionField->toPlainText(); }
QString PanelWidget::notes()    const { return m_notesField->toPlainText(); }

void PanelWidget::mouseDoubleClickEvent(QMouseEvent *e) {
  // Double-click on preview area or header (text fields consume their own
  // double-clicks and don't propagate here) — enter the shot's sub-scene.
  // Accept the event so it does NOT bubble up to StoryboardPanel::mouseDoubleClickEvent,
  // which would immediately close the sub-scene again.
  emit editRequested(m_shotIndex);
  e->accept();
}

void PanelWidget::setSeqLabel(const QString &seq) {
  // Strip alpha prefix and show only the numeric part; store the prefix for reconstruction.
  m_seqField->blockSignals(true);
  if (seq.isEmpty()) {
    m_seqField->clear();
  } else {
    QString num = stripAlphaPrefix(seq, &m_storedSeqPrefix);
    m_seqField->setText(num.isEmpty() ? seq : num);
  }
  m_seqField->blockSignals(false);
}

void PanelWidget::onDurationSpinChanged(int value) {
  m_durationLabel->setText(framesToTimecode(value));
  emit durationChanged(m_shotIndex, m_panelIndex, value);
}

void PanelWidget::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    emit clicked(m_shotIndex, m_panelIndex, e->modifiers());
    // Avvia drag solo senza modifier
    if (e->modifiers() == Qt::NoModifier) {
      QDrag *drag = new QDrag(this);
      QMimeData *mime = new QMimeData();
      mime->setData("application/x-ztoryc-shotindex",
                    QByteArray::number(m_shotIndex));
      drag->setMimeData(mime);
      QPixmap pm(size());
      pm.fill(Qt::transparent);
      render(&pm);
      drag->setPixmap(pm.scaled(160, 90, Qt::KeepAspectRatio, Qt::SmoothTransformation));
      drag->setHotSpot(QPoint(80, 45));
      drag->exec(Qt::MoveAction);
    }
  }
  QFrame::mousePressEvent(e);
}

void PanelWidget::resizeEvent(QResizeEvent *e) {
  QFrame::resizeEvent(e);
  rescalePreview();
}

void PanelWidget::dragEnterEvent(QDragEnterEvent *e) {
  if (e->mimeData()->hasFormat("application/x-ztoryc-shotindex"))
    e->acceptProposedAction();
}

void PanelWidget::dragMoveEvent(QDragMoveEvent *e) {
  if (e->mimeData()->hasFormat("application/x-ztoryc-shotindex"))
    e->acceptProposedAction();
}

void PanelWidget::dropEvent(QDropEvent *e) {
  if (not e->mimeData()->hasFormat("application/x-ztoryc-shotindex")) return;
  int fromShot = e->mimeData()->data("application/x-ztoryc-shotindex").toInt();
  if (fromShot != m_shotIndex)
    emit dropReceived(fromShot, m_shotIndex);
  e->acceptProposedAction();
}

StoryboardPanel::StoryboardPanel(QWidget *parent)
    : TPanel(parent)
    , m_columnsPerRow(3)
    , m_selectedShotIndex(-1)
    , m_fps(24)
    , m_autoRenumber(true)
    , m_comboViewer(nullptr)
{
  setObjectName("StoryboardPanel");
  setFocusPolicy(Qt::StrongFocus);

  // Keyboard shortcuts: intercept via qApp event filter so they fire regardless
  // of which child widget (card, text field, button) currently holds keyboard focus.
  // QShortcut + WidgetWithChildrenShortcut was unreliable in Tahoma's dock system;
  // qApp filter is the only approach that works consistently.
  qApp->installEventFilter(this);

  setWindowTitle(tr("Storyboard"));

  QWidget *main = new QWidget(this);
  QVBoxLayout *mainLayout = new QVBoxLayout(main);
  mainLayout->setSpacing(4);
  mainLayout->setContentsMargins(6, 6, 6, 6);

  QHBoxLayout *tb = new QHBoxLayout();

    m_addShotButton = new QToolButton();
  m_addShotButton->setIcon(createQIcon("ztoryc_add_shot"));
  m_addShotButton->setIconSize(QSize(20, 20));
  m_addShotButton->setFixedSize(28, 28);
  m_addShotButton->setToolTip(tr("Add Shot"));
  m_addShotButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

  QLabel *colLabel = new QLabel("Columns:");
  colLabel->setStyleSheet("color:#ccc;font-size:11px;");
  m_columnsPerRowSpin = new QSpinBox();
  m_columnsPerRowSpin->setRange(1, 8);
  m_columnsPerRowSpin->setValue(m_columnsPerRow);
  m_columnsPerRowSpin->setFixedWidth(45);
  m_columnsPerRowSpin->setStyleSheet("background:#333;color:#ddd;border:1px solid #555;");

  m_numberingCombo = new QComboBox();
  m_numberingCombo->addItem("Auto #");
  m_numberingCombo->addItem("Keep #");
  m_numberingCombo->addItem("Renumber All");
  m_numberingCombo->setFixedWidth(110);
  m_numberingCombo->setStyleSheet(
    "QComboBox{background:#444;color:#ddd;border:1px solid #555;border-radius:3px;padding:2px 6px;}"
    "QComboBox:hover{background:#555;}"
    "QComboBox QAbstractItemView{background:#333;color:#ddd;selection-background-color:#555;}");

    m_deleteButton = new QToolButton();
  m_deleteButton->setIcon(createQIcon("ztoryc_delete_shot"));
  m_deleteButton->setIconSize(QSize(20, 20));
  m_deleteButton->setFixedSize(28, 28);
  m_deleteButton->setToolTip(tr("Delete Shot"));
  m_deleteButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_mergeButton = new QToolButton();
  m_mergeButton->setIcon(createQIcon("ztoryc_merge"));
  m_mergeButton->setIconSize(QSize(20, 20));
  m_mergeButton->setFixedSize(28, 28);
  m_mergeButton->setToolTip(tr("Merge Shots"));
  m_mergeButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");
  m_copyButton = new QToolButton();
  m_copyButton->setIcon(createQIcon("ztoryc_copy"));
  m_copyButton->setIconSize(QSize(20, 20));
  m_copyButton->setFixedSize(28, 28);
  m_copyButton->setToolTip(tr("Copy"));
  m_copyButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_cloneButton = new QToolButton();
  m_cloneButton->setIcon(createQIcon("ztoryc_clone"));
  m_cloneButton->setIconSize(QSize(20, 20));
  m_cloneButton->setFixedSize(28, 28);
  m_cloneButton->setToolTip(tr("Clone Shot"));
  m_cloneButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_pasteButton = new QToolButton();
  m_pasteButton->setIcon(createQIcon("ztoryc_paste"));
  m_pasteButton->setIconSize(QSize(20, 20));
  m_pasteButton->setFixedSize(28, 28);
  m_pasteButton->setToolTip(tr("Paste"));
  m_pasteButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

  m_numberingBtn = new QToolButton();
  m_numberingBtn->setIcon(createQIcon("ztoryc_numbering"));
  m_numberingBtn->setIconSize(QSize(20, 20));
  m_numberingBtn->setFixedSize(28, 28);
  m_numberingBtn->setToolTip(tr("Numbering options"));
  m_numberingBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");


    m_exportPdfButton = new QToolButton();
  m_exportPdfButton->setIcon(createQIcon("ztoryc_export_pdf"));
  m_exportPdfButton->setIconSize(QSize(20, 20));
  m_exportPdfButton->setFixedSize(28, 28);
  m_exportPdfButton->setToolTip(tr("Export PDF"));
  m_exportPdfButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_exportShotsButton = new QToolButton();
  m_exportShotsButton->setIcon(createQIcon("ztoryc_export_shots"));
  m_exportShotsButton->setIconSize(QSize(20, 20));
  m_exportShotsButton->setFixedSize(28, 28);
  m_exportShotsButton->setToolTip(tr("Export Shots"));
  m_exportShotsButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

    m_exportAnimaticButton = new QToolButton();
  m_exportAnimaticButton->setIcon(createQIcon("ztoryc_export_animatic"));
  m_exportAnimaticButton->setIconSize(QSize(20, 20));
  m_exportAnimaticButton->setFixedSize(28, 28);
  m_exportAnimaticButton->setToolTip(tr("Export Animatic"));
  m_exportAnimaticButton->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}""QToolButton:hover{background:#555;}");

  tb->addWidget(m_addShotButton);
  tb->addWidget(m_deleteButton);
  tb->addWidget(m_mergeButton);
  tb->addWidget(m_copyButton);
  tb->addWidget(m_cloneButton);
  tb->addWidget(m_pasteButton);
  tb->addSpacing(8);
  tb->addWidget(m_numberingCombo);
  tb->addWidget(m_numberingBtn);
  tb->addSpacing(8);
  tb->addWidget(colLabel);
  tb->addWidget(m_columnsPerRowSpin);
  tb->addStretch();
  tb->addWidget(m_exportPdfButton);
  tb->addWidget(m_exportShotsButton);
  tb->addWidget(m_exportAnimaticButton);
  mainLayout->addLayout(tb);

  QFrame *sep = new QFrame();
  sep->setFrameShape(QFrame::HLine);
  sep->setStyleSheet("color:#444;");
  mainLayout->addWidget(sep);

  m_scrollArea = new QScrollArea();
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setStyleSheet("QScrollArea{background:#1e1e1e;border:none;}");
  // StrongFocus so that setFocus() on the scroll area works: the QShortcuts
  // installed on StoryboardPanel (WidgetWithChildrenShortcut) fire when any
  // child has focus — and m_scrollArea IS a child of StoryboardPanel.
  m_scrollArea->setFocusPolicy(Qt::StrongFocus);

  m_container = new QWidget();
  m_container->setStyleSheet("background:#1e1e1e;");
  m_grid = new QGridLayout(m_container);
  m_grid->setSpacing(8);
  m_grid->setContentsMargins(8, 8, 8, 8);
  m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_scrollArea->setWidget(m_container);

  // ---- EDIT PAGE (lazy - comboViewer created on first use) ----
  QWidget *editPage = new QWidget();
  editPage->setStyleSheet("background:#1a1a2a;");
  QVBoxLayout *editLayout = new QVBoxLayout(editPage);
  editLayout->setSpacing(8);
  editLayout->setContentsMargins(8, 8, 8, 8);

  QLabel *editHint = new QLabel("Shot open in viewer - draw, then click Back.");
  editHint->setStyleSheet("color:#888;font-size:11px;");
  editHint->setAlignment(Qt::AlignCenter);

  editLayout->addStretch();
  editLayout->addWidget(editHint);
  editLayout->addStretch();

  // ---- STACK ----
  m_stack = new QStackedWidget();
  m_stack->addWidget(m_scrollArea);  // 0 = board
  m_stack->addWidget(editPage);      // 1 = edit
  mainLayout->addWidget(m_stack);
  setWidget(main);

  connect(m_addShotButton, &QToolButton::clicked, this, &StoryboardPanel::onAddShot);
  m_panelDetectTimer = new QTimer(this);
  m_panelDetectTimer->setSingleShot(true);
  m_panelDetectTimer->setInterval(1000);
  connect(m_panelDetectTimer, &QTimer::timeout, this, [this](){
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene || scene->getChildStack()->getAncestorCount() == 0) return;
    AncestorNode *node = scene->getChildStack()->getAncestorInfo(0);
    if (!node) return;
    int col = node->m_col;
    for (int si = 0; si < (int)m_shots.size(); si++) {
      if (m_shots[si].data.xsheetColumn == col) {
        detectAndUpdatePanels(si);
        for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++)
          updatePreview(si, pi);
        break;
      }
    }
  });
  connect(m_deleteButton, &QToolButton::clicked, this, &StoryboardPanel::onDeleteShot);
  connect(m_mergeButton, &QToolButton::clicked, this, &StoryboardPanel::onMergeShots);
  connect(m_copyButton,   &QToolButton::clicked, this, &StoryboardPanel::onCopyShot);
  connect(m_cloneButton,  &QToolButton::clicked, this, &StoryboardPanel::onCloneShot);
  connect(m_pasteButton,  &QToolButton::clicked, this, &StoryboardPanel::onPasteShot);
  connect(m_exportPdfButton, &QToolButton::clicked, this, &StoryboardPanel::onExportPdf);
  connect(m_exportShotsButton, &QToolButton::clicked, this, &StoryboardPanel::onExportShots);
  connect(m_exportAnimaticButton, &QToolButton::clicked, this, &StoryboardPanel::onExportAnimatic);
  connect(m_columnsPerRowSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, &StoryboardPanel::onColumnsChanged);
  connect(m_numberingCombo, QOverload<int>::of(&QComboBox::activated),
          this, &StoryboardPanel::onNumberingChanged);
  connect(m_numberingBtn, &QToolButton::clicked,
          this, &StoryboardPanel::onNumberingConfig);
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &StoryboardPanel::refreshFromScene);
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, &StoryboardPanel::onXsheetChanged);
  // Sync durations when ZtoryModel resequences (works even inside sub-scenes)
  connect(ZtoryModel::instance(), &ZtoryModel::modelReset,
          this, &StoryboardPanel::onModelResequenced);
  connect(ZtoryModel::instance(), &ZtoryModel::shotAdded,
          this, &StoryboardPanel::onShotInserted);
  connect(ZtoryModel::instance(), &ZtoryModel::shotRemovedAt,
          this, &StoryboardPanel::onShotRemovedAt);
  // Debounce timer per refresh thumbnail
  QTimer *refreshTimer = new QTimer(this);
  refreshTimer->setSingleShot(true);
  refreshTimer->setInterval(800);
  connect(refreshTimer, &QTimer::timeout, this, [this](){
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene || scene->getChildStack()->getAncestorCount() == 0) return;
    AncestorNode *node = scene->getChildStack()->getAncestorInfo(0);
    if (!node) return;
    int col = node->m_col;
    for (int si = 0; si < (int)m_shots.size(); si++) {
      if (m_shots[si].data.xsheetColumn == col) {
        for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++)
          updatePreview(si, pi);
        break;
      }
    }
  });
  connect(TApp::instance()->getCurrentFrame(), &TFrameHandle::frameSwitched,
          this, [this](){ m_panelDetectTimer->start(); });
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetSwitched,
          this, [this](){
    disconnect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
               this, nullptr);
    connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
            this, &StoryboardPanel::onXsheetChanged);
    // Refresh automatico thumbnail
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    int ancestorCount = scene->getChildStack()->getAncestorCount();
    if (ancestorCount == 0) {
      // Tornati al main — refresh completo
      QTimer::singleShot(200, this, &StoryboardPanel::onRefreshPreviews);
    } else {
      // Entrati in una sottoscena — refresh solo dello shot corrente
      AncestorNode *node = scene->getChildStack()->getAncestorInfo(0);
      if (node) {
        int col = node->m_col;
        for (int si = 0; si < (int)m_shots.size(); si++) {
          if (m_shots[si].data.xsheetColumn == col) {
            for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++)
              QTimer::singleShot(300, this, [this, si, pi](){ updatePreview(si, pi); });
            break;
          }
        }
      }
    }
  });
}

void StoryboardPanel::addPanelWidget(int shotIdx, int panelIdx) {
  Shot &shot = m_shots[shotIdx];
  PanelWidget *pw = new PanelWidget(m_container);
  pw->setFps(m_fps);
  pw->setShotIndex(shotIdx);
  pw->setPanelIndex(panelIdx, (int)shot.data.panels.size());
  pw->setShotNumber(shot.data.label());
  // Show sequence label if one is assigned; otherwise show "—"
  {
    QString seqLbl;
    if (!shot.data.sequenceId.isEmpty()) {
      const SequenceData *seq = ZtoryModel::instance()->findSequence(shot.data.sequenceId);
      if (seq) seqLbl = seq->label;
    }
    pw->setSeqLabel(seqLbl);
  }
  pw->setDuration(shot.data.panels[panelIdx].duration);
  pw->setTotalDuration(shot.data.totalDuration());
  pw->setDialog(shot.data.panels[panelIdx].dialog);
  pw->setAction(shot.data.panels[panelIdx].action);
  pw->setNotes(shot.data.panels[panelIdx].notes);
  connectPanelWidget(pw);
  shot.panels.push_back(pw);
}

void StoryboardPanel::connectPanelWidget(PanelWidget *pw) {
  connect(pw, &PanelWidget::editRequested, this, &StoryboardPanel::onEditShot);
  connect(pw, &PanelWidget::matchDurationRequested, this, &StoryboardPanel::onMatchDuration);
  connect(pw, &PanelWidget::durationChanged, this, &StoryboardPanel::onDurationChanged);
  connect(pw, &PanelWidget::totalDurationChanged, this, [this, pw](int frames){
    // Trova lo shot corrispondente e aggiorna la durata sul main xsheet
    for (int si = 0; si < (int)m_shots.size(); si++) {
      if (!m_shots[si].panels.empty() && m_shots[si].panels[0] == pw) {
        int col = m_shots[si].data.xsheetColumn;
        onDurationChanged(si, 0, frames);
        break;
      }
    }
  });
  connect(pw, &PanelWidget::clicked, this, &StoryboardPanel::onPanelClicked);
  connect(pw, &PanelWidget::dropReceived, this, &StoryboardPanel::onMoveShot);
  connect(pw, &PanelWidget::dataChanged, [this](int si, int pi){
    if (si >= 0 && si < (int)m_shots.size()) {
      // Update both shotLabel (primary) and shotNumber (legacy compat)
      QString edited = m_shots[si].panels[0]->shotNumber();
      m_shots[si].data.shotLabel  = edited;
      m_shots[si].data.shotNumber = edited;
      for (PanelWidget *p : m_shots[si].panels)
        p->setShotNumber(m_shots[si].data.label());
      updateColumnName(si);
    }
    saveZtoryc();
  });
  // Sequence field edited: cascade the sequence assignment forward until a
  // shot with a different (non-empty) sequenceId is encountered.
  connect(pw, &PanelWidget::seqLabelEdited, this,
          [this](int si, QString fullLabel) {
    if (si < 0 || si >= (int)m_shots.size()) return;
    ZtoryModel *model = ZtoryModel::instance();
    SequenceData *seq = model->findOrCreateSequence(fullLabel);
    if (!seq) return;
    // Remember what sequenceId the target shot had before the edit so we
    // know when to stop cascading (stop at the first shot that already
    // belongs to a *different* sequence).
    QString prevSeqId = m_shots[si].data.sequenceId;
    for (int i = si; i < (int)m_shots.size(); i++) {
      if (i > si && !m_shots[i].data.sequenceId.isEmpty() &&
          m_shots[i].data.sequenceId != prevSeqId)
        break;
      m_shots[i].data.sequenceId = seq->uuid;
      for (PanelWidget *pw2 : m_shots[i].panels)
        pw2->setSeqLabel(seq->label);
      if (i < model->shotCount())
        model->shot(i).sequenceId = seq->uuid;
    }
    model->save();
    saveZtoryc();
  });
}

void StoryboardPanel::assignBoardShotLabel(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  // Project Board shots to a plain ShotData vector so we can use the shared
  // static algorithm (which needs the full neighbour context).
  std::vector<ShotData> shotDatas;
  shotDatas.reserve(m_shots.size());
  for (const Shot &s : m_shots) shotDatas.push_back(s.data);
  ZtoryModel::assignShotLabel(shotDatas, si, ZtoryModel::instance()->numberingConfig());
  // Copy result back
  m_shots[si].data.shotLabel  = shotDatas[si].shotLabel;
  m_shots[si].data.orderIndex = shotDatas[si].orderIndex;
  m_shots[si].data.shotNumber = shotDatas[si].shotLabel;
}

void StoryboardPanel::renumberAll() {
  ZtoryModel *model             = ZtoryModel::instance();
  const NumberingConfig &cfg    = model->numberingConfig();
  const int scale               = 100;
  for (int i = 0; i < (int)m_shots.size(); i++) {
    Shot &shot = m_shots[i];
    if (m_autoRenumber) {
      // Auto mode: reassign ALL labels with clean sequential numbering.
      // This correctly handles inserts: the new shot takes the "next" position
      // and existing shots above it get renumbered (e.g. SH020→SH030).
      QString raw = cfg.shotName(i);   // may be "SQ001_SH010" in Sequence style
      int sep = raw.indexOf('_');
      if (sep >= 0) {
        // Sequence style: split SQ part and SH part
        QString sqPart = raw.left(sep);       // e.g. "SQ001"
        QString shPart = raw.mid(sep + 1);    // e.g. "SH010"
        SequenceData *seq = model->findOrCreateSequence(sqPart);
        shot.data.shotLabel  = shPart;
        shot.data.shotNumber = shPart;
        shot.data.sequenceId = seq ? seq->uuid : QString();
      } else {
        shot.data.shotLabel  = raw;
        shot.data.shotNumber = raw;
        shot.data.sequenceId = QString();
      }
      shot.data.orderIndex = (cfg.startNumber + i * cfg.step) * scale;
    } else if (shot.data.shotLabel.isEmpty()) {
      // Keep-# mode: only assign label to slots that have none yet.
      // Use the midpoint algorithm on the Board's own shot list.
      assignBoardShotLabel(i);
    }
    updateColumnName(i);
    // Resolve sequence label for display
    QString seqLabel;
    if (!shot.data.sequenceId.isEmpty()) {
      SequenceData *seq = model->findSequence(shot.data.sequenceId);
      if (seq) seqLabel = seq->label;
    }
    for (PanelWidget *pw : shot.panels) {
      pw->setShotIndex(i);
      pw->setShotNumber(shot.data.label());
      if (!seqLabel.isEmpty()) pw->setSeqLabel(seqLabel);
      pw->setPanelIndex(pw->panelIndex(), (int)shot.panels.size());
    }
  }
}

void StoryboardPanel::clearShots() {
  for (Shot &shot : m_shots)
    for (PanelWidget *pw : shot.panels) {
      m_grid->removeWidget(pw);
      delete pw;
    }
  m_shots.clear();
  m_selectedShotIndex = -1;
}

void StoryboardPanel::resequenceXsheet() {
  ZtoryModel::instance()->resequenceXsheet();
}

void StoryboardPanel::rebuildGrid() {
  for (Shot &shot : m_shots)
    for (PanelWidget *pw : shot.panels)
      m_grid->removeWidget(pw);
  int col = 0, row = 0;
  for (Shot &shot : m_shots) {
    for (PanelWidget *pw : shot.panels) {
      m_grid->addWidget(pw, row, col);
      pw->show();
      pw->updateGeometry();
      col++;
      if (col >= m_columnsPerRow) { col = 0; row++; }
    }
  }
  m_container->adjustSize();
  QTimer::singleShot(200, this, [this](){
    int available = m_scrollArea->viewport()->width() - 8 * (m_columnsPerRow + 1);
    int colW = qMax(200, available / m_columnsPerRow);
    for (Shot &shot : m_shots)
      for (PanelWidget *pw : shot.panels) {
        pw->setFixedWidth(colW);
        pw->rescalePreview();
      }
    m_container->adjustSize();
  });
}

void StoryboardPanel::selectShot(int shotIdx) {
  // Deseleziona tutti
  for (int i : m_selectedIndices)
    if (i >= 0 && i < (int)m_shots.size())
      for (PanelWidget *pw : m_shots[i].panels) pw->setSelected(false);
  m_selectedIndices.clear();
  if (m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
    for (PanelWidget *pw : m_shots[m_selectedShotIndex].panels)
      pw->setSelected(false);
  m_selectedShotIndex = shotIdx;
  if (m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
    for (PanelWidget *pw : m_shots[m_selectedShotIndex].panels)
      pw->setSelected(true);
}

void StoryboardPanel::updatePreview(int shotIdx, int panelIdx) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  Shot &shot = m_shots[shotIdx];
  if (panelIdx < 0 || panelIdx >= (int)shot.panels.size()) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (not scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (not xsh) return;
  TXshChildLevel *cl = nullptr;
  for (int r = 0; r <= xsh->getFrameCount(); r++) {
    TXshCell cell = xsh->getCell(r, shotIdx);
    if (not cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      cl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (not cl) return;
  TXsheet *subXsh = cl->getXsheet();
  if (not subXsh) return;
  int frame = shot.data.panels[panelIdx].startFrame;
  QPixmap px = IconGenerator::renderXsheetFrame(subXsh, frame, TDimension(320, 180));
  if (not px.isNull())
    shot.panels[panelIdx]->setPreviewPixmap(px);
}

QString StoryboardPanel::ztoryPath() const {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (not scene) return QString();
  TFilePath sp = scene->getScenePath();
  if (sp.isEmpty()) return QString();
  QString path = QString::fromStdWString(sp.getWideString());
  path.replace(QRegularExpression("\.tnz$"), ".ztoryc");
  return path;
}

void StoryboardPanel::updateColumnName(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getXsheet();
  if (!xsh) return;
  int col = si; // la colonna corrisponde all indice dello shot
  TStageObject *obj = xsh->getStageObjectTree()->getStageObject(TStageObjectId::ColumnId(col), false);
  if (obj) {
    obj->setName(m_shots[si].data.label().toStdString());
    app->getCurrentXsheet()->notifyXsheetChanged();
  }
}

void StoryboardPanel::saveZtoryc() {
  QString path = ztoryPath();
  if (path.isEmpty()) return;
  QFile file(path);
  if (not file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztoryc");
  xml.writeAttribute("version", "2");
  for (int si = 0; si < (int)m_shots.size(); si++) {
    const Shot &shot = m_shots[si];
    xml.writeStartElement("shot");
    xml.writeAttribute("index",  QString::number(si));
    xml.writeAttribute("number", shot.data.shotNumber);
    xml.writeAttribute("label",  shot.data.shotLabel);
    xml.writeAttribute("order",  QString::number(shot.data.orderIndex));
    for (int pi = 0; pi < (int)shot.data.panels.size(); pi++) {
      const PanelData &pd = shot.data.panels[pi];
      xml.writeStartElement("panel");
      xml.writeAttribute("index",      QString::number(pi));
      xml.writeAttribute("startFrame", QString::number(pd.startFrame));
      xml.writeAttribute("duration",   QString::number(pd.duration));
      xml.writeTextElement("dialog", pd.dialog);
      xml.writeTextElement("action", pd.action);
      xml.writeTextElement("notes",  pd.notes);
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }
  xml.writeEndElement();
  xml.writeEndDocument();
  file.close();
}

void StoryboardPanel::loadZtoryc() {
  QString path = ztoryPath();
  if (path.isEmpty()) return;
  QFile file(path);
  if (not file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
  QXmlStreamReader xml(&file);
  int si = -1, pi = -1;
  while (not xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      if (xml.name() == QLatin1String("shot")) {
        si = xml.attributes().value("index").toInt();
        if (si < (int)m_shots.size()) {
          m_shots[si].data.shotNumber = xml.attributes().value("number").toString();
          m_shots[si].data.shotLabel  = xml.attributes().value("label").toString();
          m_shots[si].data.orderIndex = xml.attributes().value("order").toInt();
          // Backward compat (v1-v2 files written by StoryboardPanel):
          // if shotLabel absent, use shotNumber
          if (m_shots[si].data.shotLabel.isEmpty())
            m_shots[si].data.shotLabel = m_shots[si].data.shotNumber;
        }
      }
      else if (xml.name() == QLatin1String("panel")) {
        pi = xml.attributes().value("index").toInt();
        if (si >= 0 && si < (int)m_shots.size() && pi >= 0) {
          // Aggiungi panel mancanti se necessario
          while (pi >= (int)m_shots[si].data.panels.size()) {
            PanelData pd;
            m_shots[si].data.panels.push_back(pd);
          }
          m_shots[si].data.panels[pi].startFrame =
            xml.attributes().value("startFrame").toInt();
          m_shots[si].data.panels[pi].duration =
            xml.attributes().value("duration").toInt();
        }
      }
      else if (xml.name() == QLatin1String("dialog")) {
        QString t = xml.readElementText();
        if (si >= 0 && si < (int)m_shots.size() &&
            pi >= 0 && pi < (int)m_shots[si].data.panels.size())
          m_shots[si].data.panels[pi].dialog = t;
      }
      else if (xml.name() == QLatin1String("action")) {
        QString t = xml.readElementText();
        if (si >= 0 && si < (int)m_shots.size() &&
            pi >= 0 && pi < (int)m_shots[si].data.panels.size())
          m_shots[si].data.panels[pi].action = t;
      }
      else if (xml.name() == QLatin1String("notes")) {
        QString t = xml.readElementText();
        if (si >= 0 && si < (int)m_shots.size() &&
            pi >= 0 && pi < (int)m_shots[si].data.panels.size())
          m_shots[si].data.panels[pi].notes = t;
      }
    }
  }
  file.close();
  for (int i = 0; i < (int)m_shots.size(); i++) {
    Shot &shot = m_shots[i];
    // Rimuovi tutti i widget esistenti e ricostruisci da data
    for (PanelWidget *pw : shot.panels) { m_grid->removeWidget(pw); delete pw; }
    shot.panels.clear();
    for (int j = 0; j < (int)shot.data.panels.size(); j++) {
      addPanelWidget(i, j);
      shot.panels[j]->setDuration(shot.data.panels[j].duration);
      shot.panels[j]->setDialog(shot.data.panels[j].dialog);
      shot.panels[j]->setAction(shot.data.panels[j].action);
      shot.panels[j]->setNotes(shot.data.panels[j].notes);
    }
  }
}

int StoryboardPanel::currentShotIndex() const {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (not scene) return -1;
  ChildStack *cs = scene->getChildStack();
  if (not cs) return -1;
  int depth = cs->getAncestorCount();
  if (depth == 0) return -1;
  AncestorNode *node = cs->getAncestorInfo(depth - 1);
  if (not node) return -1;
  return node->m_col;
}

void StoryboardPanel::detectAndUpdatePanels(int shotIdx) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  TApp *app = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  if (not xsh) return;
  int numCols = xsh->getColumnCount();
  int numFrames = xsh->getFrameCount();
  if (numFrames <= 0 || numCols <= 0) return;

  // Get timeline-visible duration from the main xsheet ancestor.
  // Sub-scene may have more frames than the column range visible on the timeline.
  int timelineDuration = numFrames;  // fallback: full sub-scene
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (scene && scene->getChildStack()->getAncestorCount() > 0) {
    int depth = scene->getChildStack()->getAncestorCount();
    // getAncestorInfo(depth-1) is the outermost ancestor (main xsheet level)
    AncestorNode *anc = scene->getChildStack()->getAncestorInfo(depth - 1);
    if (anc && anc->m_xsheet) {
      int mainCol = m_shots[shotIdx].data.xsheetColumn;
      TXshColumn *mc = anc->m_xsheet->getColumn(mainCol);
      if (mc) {
        int r0 = 0, r1 = 0;
        mc->getRange(r0, r1);
        if (r1 >= r0) timelineDuration = r1 - r0 + 1;
      }
    }
  }

  // Collect keyframe change rows from sub-scene
  std::vector<int> allPanelFrames;
  allPanelFrames.push_back(0);
  for (int r = 1; r < numFrames; r++) {
    bool changed = false;
    for (int c = 0; c < numCols && not changed; c++) {
      TXshCell prev = xsh->getCell(r - 1, c);
      TXshCell curr = xsh->getCell(r, c);
      if (prev.m_frameId != curr.m_frameId || prev.isEmpty() != curr.isEmpty())
        changed = true;
    }
    for (int c = 0; c < numCols && not changed; c++) {
      TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(c));
      if (obj && obj->isKeyframe(r)) changed = true;
    }
    if (not changed) {
      TStageObject *cam = xsh->getStageObject(TStageObjectId::CameraId(0));
      if (cam && cam->isKeyframe(r)) changed = true;
    }
    if (changed) allPanelFrames.push_back(r);
  }

  // Keep only panels whose start frame falls within the timeline-visible range.
  // Panels beyond timelineDuration are hidden from the Board.
  std::vector<int> panelFrames;
  for (int f : allPanelFrames)
    if (f < timelineDuration) panelFrames.push_back(f);
  if (panelFrames.empty()) panelFrames.push_back(0);

  Shot &shot = m_shots[shotIdx];
  int newPanelCount = (int)panelFrames.size();

  if (newPanelCount == (int)shot.data.panels.size()) {
    // Count unchanged — update durations only (timeline may have been resized)
    for (int i = 0; i < newPanelCount; i++) {
      shot.data.panels[i].startFrame = panelFrames[i];
      shot.data.panels[i].duration   = (i+1 < newPanelCount)
                                       ? panelFrames[i+1] - panelFrames[i]
                                       : timelineDuration - panelFrames[i];
      if (i < (int)shot.panels.size())
        shot.panels[i]->setDuration(shot.data.panels[i].duration);
    }
    for (PanelWidget *pw : shot.panels)
      pw->setTotalDuration(timelineDuration);
    saveZtoryc();
    return;
  }

  // Panel count changed — rebuild panel data and widgets
  while ((int)shot.data.panels.size() < newPanelCount) {
    PanelData pd;
    shot.data.panels.push_back(pd);
  }
  while ((int)shot.data.panels.size() > newPanelCount)
    shot.data.panels.pop_back();
  for (int i = 0; i < newPanelCount; i++) {
    shot.data.panels[i].startFrame = panelFrames[i];
    shot.data.panels[i].duration   = (i+1 < newPanelCount)
                                     ? panelFrames[i+1] - panelFrames[i]
                                     : timelineDuration - panelFrames[i];
  }
  for (PanelWidget *pw : shot.panels) { m_grid->removeWidget(pw); delete pw; }
  shot.panels.clear();
  for (int pi = 0; pi < (int)shot.data.panels.size(); pi++)
    addPanelWidget(shotIdx, pi);
  renumberAll();
  rebuildGrid();
  saveZtoryc();
}

void StoryboardPanel::assignKeepNumbers(int insertAt) {
  int total = (int)m_shots.size();
  // Assegna numeri fissi agli shot senza shotNumber basandosi sulla posizione originale
  // Gli shot prima di insertAt mantengono il loro numero, quelli dopo anche
  for (int j = 0; j < total; j++) {
    if (j != insertAt && m_shots[j].data.shotNumber.isEmpty())
      m_shots[j].data.shotNumber = QString("%1").arg(j + 1, 2, 10, QChar(48));
  }
  // Se in coda - stessa logica del caso "in mezzo"
  if (insertAt >= total - 1) {
    QString baseNum = m_shots[insertAt - 1].data.shotNumber;
    int i = baseNum.length() - 1;
    while (i >= 0 && baseNum[i].isLetter()) i--;
    QString numPart = baseNum.left(i + 1);
    // Controlla se il precedente ha gia lettere - in quel caso incrementa numero
    bool prevHasLetter = (i < baseNum.length() - 1);
    if (prevHasLetter) {
      // precedente e es. "05A" - nuovo e "05B"
      QChar letter = 'A';
      for (int j = 0; j < total - 1; j++) {
        QString n = m_shots[j].data.shotNumber;
        if (n.startsWith(numPart) && n.length() == numPart.length() + 1 && n[numPart.length()].isLetter()) {
          QChar c = n[numPart.length()];
          if (c.toLatin1() >= letter.toLatin1())
            letter = QChar(c.toLatin1() + 1);
        }
      }
      m_shots[insertAt].data.shotNumber = numPart + letter;
    } else {
      // precedente e es. "05" - nuovo e "05A"
      QChar letter = 'A';
      for (int j = 0; j < total - 1; j++) {
        QString n = m_shots[j].data.shotNumber;
        if (n.startsWith(numPart) && n.length() == numPart.length() + 1 && n[numPart.length()].isLetter()) {
          QChar c = n[numPart.length()];
          if (c.toLatin1() >= letter.toLatin1())
            letter = QChar(c.toLatin1() + 1);
        }
      }
      m_shots[insertAt].data.shotNumber = numPart + letter;
    }
    return;
  }
  // Se in testa
  if (insertAt == 0) {
    QString nextNum = m_shots[1].data.shotNumber;
    int i = nextNum.length() - 1;
    while (i >= 0 && nextNum[i].isLetter()) i--;
    m_shots[0].data.shotNumber = nextNum.left(i + 1) + "A";
    return;
  }
  // In mezzo - usa numero del precedente + lettera
  QString baseNum = m_shots[insertAt - 1].data.shotNumber;
  int i = baseNum.length() - 1;
  while (i >= 0 && baseNum[i].isLetter()) i--;
  QString numPart = baseNum.left(i + 1);
  QChar letter = 'A';
  for (int j = 0; j < total; j++) {
    if (j == insertAt) continue;
    QString n = m_shots[j].data.shotNumber;
    if (n.startsWith(numPart) && n.length() == numPart.length() + 1 && n[numPart.length()].isLetter()) {
      QChar c = n[numPart.length()];
      if (c.toLatin1() >= letter.toLatin1())
        letter = QChar(c.toLatin1() + 1);
    }
  }
  m_shots[insertAt].data.shotNumber = numPart + letter;
}

void StoryboardPanel::onModelResequenced() {
  // Same as onXsheetChanged but without the ancestor-count guard, so it works
  // even when the user is inside a sub-scene while the animatic panel resequences.
  // If shot count changed (e.g. Animatic deleted/merged shots), do a full rebuild.
  //
  // IMPORTANT: use the actual xsheet child-column count, NOT ZtoryModel::m_shots.size().
  // ZtoryModel::m_shots can be stale after copy/paste/clone sequences that bypass
  // ZtoryModel::addShot()/removeShot(). Using it as reference caused double-removal:
  // refreshFromScene() fired here AND shotRemovedAt() fired afterward → Board lost
  // one extra shot after a cross-panel merge.
  TXsheet *xsh = TApp::instance()->getCurrentScene()->getScene()
                   ? TApp::instance()->getCurrentScene()->getScene()->getChildStack()->getTopXsheet()
                   : nullptr;
  if (!xsh) return;
  int xshShotCount = 0;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        xshShotCount++;
        break;
      }
    }
  }
  if (xshShotCount != (int)m_shots.size()) {
    refreshFromScene();
    return;
  }
  for (int si = 0; si < (int)m_shots.size(); si++) {
    int col = m_shots[si].data.xsheetColumn;
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    int duration = r1 - r0 + 1;
    if (!m_shots[si].data.panels.empty()) {
      m_shots[si].data.panels[0].duration = duration;
      if (!m_shots[si].panels.empty())
        m_shots[si].panels[0]->setDuration(duration);
    }
  }
}

void StoryboardPanel::onMergeShots() {
  if (!ZtoryModel::assertMainXsheet(true)) return;

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Build the set of xsheet columns to merge.
  // Prefer own selection (>= 2 shots); fall back to shared selection from Animatic.
  std::vector<int> sortedCols;
  std::set<int> localIndices = m_selectedIndices;
  if (localIndices.size() < 2 && m_selectedShotIndex >= 0)
    localIndices.insert(m_selectedShotIndex);
  if (localIndices.size() >= 2) {
    for (int bi : localIndices)
      if (bi >= 0 && bi < (int)m_shots.size())
        sortedCols.push_back(m_shots[bi].data.xsheetColumn);
  } else {
    // Fall back to shared selection (last written by Animatic or Board).
    const std::set<int> &shared = ZtoryModel::instance()->sharedSelection();
    sortedCols.assign(shared.begin(), shared.end());
  }
  if (sortedCols.size() < 2) return;
  std::sort(sortedCols.begin(), sortedCols.end(), [&](int a, int b){
    int r0a = 0, r1a = 0, r0b = 0, r1b = 0;
    if (xsh->getColumn(a)) xsh->getColumn(a)->getRange(r0a, r1a);
    if (xsh->getColumn(b)) xsh->getColumn(b)->getRange(r0b, r1b);
    return r0a < r0b;
  });

  int dstCol = sortedCols[0];
  TXshColumn *dstColumn = xsh->getColumn(dstCol);
  if (!dstColumn) return;
  int dstR0 = 0, dstR1 = 0;
  dstColumn->getRange(dstR0, dstR1);

  TXshChildLevel *dstCl = nullptr;
  for (int r = dstR0; r <= dstR1; r++) {
    TXshCell cell = xsh->getCell(r, dstCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      dstCl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (!dstCl) return;

  int appendAt    = dstR1 + 1;
  int dstDuration = dstR1 - dstR0 + 1;
  int lastFrameNum = dstDuration;

  materializeCells(dstCl, dstDuration);
  trimChildXsheetTo(dstCl, dstDuration);

  for (int i = 1; i < (int)sortedCols.size(); i++) {
    int srcCol = sortedCols[i];
    TXshColumn *srcColumn = xsh->getColumn(srcCol);
    if (!srcColumn) continue;
    int r0 = 0, r1 = 0;
    srcColumn->getRange(r0, r1);
    int duration = r1 - r0 + 1;
    TXshChildLevel *srcCl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, srcCol);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        srcCl = cell.m_level->getChildLevel();
        break;
      }
    }
    mergeChildXsheetContent(dstCl, srcCl, lastFrameNum, duration);
    for (int r = 0; r < duration; r++)
      xsh->setCell(appendAt + r, dstCol, TXshCell(dstCl, TFrameId(++lastFrameNum)));
    appendAt += duration;
  }

  // Delete source columns in reverse order to keep lower indices stable
  for (int i = (int)sortedCols.size() - 1; i >= 1; i--)
    ColumnCmd::deleteColumn(sortedCols[i]);

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();

  m_selectedIndices.clear();
  m_selectedShotIndex = -1;

  // Board syncs via resequenceXsheet() → modelReset() → onModelResequenced() above.
  // m_updating=true prevents THIS Board instance from also processing the signal
  // via onShotRemovedAt() (which would cause a double-removal now that
  // onModelResequenced uses the reliable xsheet count and calls refreshFromScene).
  // Other Board instances (if any) still receive and process shotRemovedAt normally.
  m_updating = true;
  for (int i = (int)sortedCols.size() - 1; i >= 1; i--)
    emit ZtoryModel::instance()->shotRemovedAt(sortedCols[i]);
  m_updating = false;
}

void StoryboardPanel::onShotInserted(int col) {
  if (m_updating) return;  // skip if THIS Board emitted the signal (already updated)
  // Called when the animatic razor (or any external op) inserts a new shot
  // column at position 'col'.  We insert a corresponding Shot entry at that
  // position, then renumber and save — bypassing loadZtoryc() which would
  // map by stale index and assign wrong numbers to existing shots.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  if (col < 0 || col > (int)m_shots.size()) return;

  // Build new Shot from xsheet column
  Shot shot;
  shot.data.xsheetColumn = col;
  TXshColumn *column = xsh->getColumn(col);
  if (column) {
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    PanelData pd;
    pd.duration = (r1 >= r0) ? (r1 - r0 + 1) : 24;
    shot.data.panels.push_back(pd);
  } else {
    PanelData pd; pd.duration = 24;
    shot.data.panels.push_back(pd);
  }

  // Update xsheetColumn for all shots at col or later (they shifted right)
  for (int i = 0; i < (int)m_shots.size(); i++)
    if (m_shots[i].data.xsheetColumn >= col)
      m_shots[i].data.xsheetColumn++;

  m_shots.insert(m_shots.begin() + col, shot);
  // Copy shotLabel + orderIndex from ZtoryModel (generateShotLabel was already called there)
  ZtoryModel *model = ZtoryModel::instance();
  if (col < model->shotCount() && !model->shot(col).shotLabel.isEmpty()) {
    m_shots[col].data.shotLabel  = model->shot(col).shotLabel;
    m_shots[col].data.orderIndex = model->shot(col).orderIndex;
    m_shots[col].data.shotNumber = m_shots[col].data.shotLabel;
  }
  addPanelWidget(col, 0);

  renumberAll();
  rebuildGrid();
  saveZtoryc();
}

void StoryboardPanel::onShotRemovedAt(int col) {
  if (m_updating) return;  // skip if THIS Board emitted the signal (already updated)
  int si = -1;
  for (int i = 0; i < (int)m_shots.size(); i++) {
    if (m_shots[i].data.xsheetColumn == col) { si = i; break; }
  }
  if (si < 0) {
    // Shot not found — Board xsheetColumn tracking is desynced (e.g. after
    // a previous cut/merge left counts off by one). Rebuild from xsheet.
    refreshFromScene();
    return;
  }

  for (PanelWidget *pw : m_shots[si].panels) {
    m_grid->removeWidget(pw);
    delete pw;
  }
  m_shots.erase(m_shots.begin() + si);

  // Columns above 'col' shifted down by 1 when the column was deleted
  for (int i = 0; i < (int)m_shots.size(); i++)
    if (m_shots[i].data.xsheetColumn > col)
      m_shots[i].data.xsheetColumn--;

  renumberAll();
  rebuildGrid();
  saveZtoryc();
}

void StoryboardPanel::onXsheetChanged() {
  // Update T: (timeline duration) for all shots from main xsheet column range.
  // D: (partial) is only updated for single-panel shots; multi-panel partials
  // are owned by detectAndUpdatePanels and must not be overwritten here.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene || scene->getChildStack()->getAncestorCount() != 0) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;
  for (int si = 0; si < (int)m_shots.size(); si++) {
    int col = m_shots[si].data.xsheetColumn;
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    int duration = r1 - r0 + 1;
    // Always update T: display to reflect actual timeline duration
    for (PanelWidget *pw : m_shots[si].panels)
      pw->setTotalDuration(duration);
    // For single-panel shots: D: == T: (partial = total)
    if (m_shots[si].data.panels.size() == 1) {
      m_shots[si].data.panels[0].duration = duration;
      if (!m_shots[si].panels.empty())
        m_shots[si].panels[0]->setDuration(duration);
    }
    // For multi-panel shots: D: stays as computed by detectAndUpdatePanels
  }
}

void StoryboardPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  if (m_shots.empty()) refreshFromScene();
}

void StoryboardPanel::refreshFromScene() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (not scene) return;
  clearShots();
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (not xsh) return;
  int numCols = xsh->getColumnCount();
  for (int col = 0; col < numCols; col++) {
    TXshChildLevel *cl = nullptr;
    int duration = 0;
    for (int r = 0; r < xsh->getFrameCount(); r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (not cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        if (not cl) cl = cell.m_level->getChildLevel();
        duration++;
      } else if (duration > 0) break;
    }
    if (not cl) continue;
    Shot shot;
    shot.data.xsheetColumn = col;
    shot.data.shotNumber = QString("%1").arg((int)m_shots.size()+1, 2, 10, QChar(48));
    PanelData pd;
    pd.startFrame = 0;
    pd.duration = duration;
    shot.data.panels.push_back(pd);
    m_shots.push_back(shot);
    addPanelWidget((int)m_shots.size()-1, 0);
  }
  loadZtoryc();
  // Rebuild panel widgets to match the panel data loaded from .ztoryc.
  // refreshFromScene creates one placeholder widget per shot; loadZtoryc may
  // have added more panels to shot.data.panels, so we recreate all widgets.
  for (int si = 0; si < (int)m_shots.size(); si++) {
    for (PanelWidget *pw : m_shots[si].panels) {
      m_grid->removeWidget(pw);
      delete pw;
    }
    m_shots[si].panels.clear();
    for (int pi = 0; pi < (int)m_shots[si].data.panels.size(); pi++)
      addPanelWidget(si, pi);
  }
  renumberAll();
  rebuildGrid();
  QTimer::singleShot(500, this, [this](){
    for (int si = 0; si < (int)m_shots.size(); si++)
      for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++)
        updatePreview(si, pi);
  });
}

// ── qApp event filter: intercept keyboard shortcuts for the Board ────────────
// Two-phase interception:
//   1. QEvent::ShortcutOverride  — "claim" the key sequence so that
//      CommandManager's ApplicationShortcut QActions (MI_Copy etc.) never fire.
//      Returning true after accept() tells Qt to skip shortcut dispatch and
//      proceed to KeyPress on the focused widget.
//   2. QEvent::KeyPress — actually dispatch the shot operation.
// Without phase 1, Cmd+C/X/V/D are stolen by CommandManager before KeyPress
// arrives, so only Delete (which lacks a global binding in most contexts) worked.
bool StoryboardPanel::eventFilter(QObject *obj, QEvent *e) {
  const QEvent::Type t = e->type();
  if (t != QEvent::ShortcutOverride && t != QEvent::KeyPress) return false;

  // Check if focus is inside this panel's subtree
  QWidget *fw = QApplication::focusWidget();
  bool inPanel = false;
  for (QWidget *w = fw; w; w = w->parentWidget())
    if (w == this) { inPanel = true; break; }
  if (!inPanel) return false;

  // Require at least one shot selected
  if (m_selectedShotIndex < 0 && m_selectedIndices.empty()) return false;

  QKeyEvent *ke  = static_cast<QKeyEvent *>(e);
  const bool cmd   = ke->modifiers() & Qt::ControlModifier;
  const bool shift = ke->modifiers() & Qt::ShiftModifier;
  const bool noMod = ke->modifiers() == Qt::NoModifier;

  // Identify keys we own
  const bool isDelete = noMod && (ke->key() == Qt::Key_Delete ||
                                  ke->key() == Qt::Key_Backspace);
  const bool isCopy   = cmd && !shift && ke->key() == Qt::Key_C;
  const bool isCut    = cmd && !shift && ke->key() == Qt::Key_X;
  const bool isPaste  = cmd && !shift && ke->key() == Qt::Key_V;
  const bool isClone  = cmd && !shift && ke->key() == Qt::Key_D;

  if (!isDelete && !isCopy && !isCut && !isPaste && !isClone) return false;

  // Phase 1 — ShortcutOverride: claim the key so CommandManager doesn't fire.
  // We accept and return true; Qt will then skip shortcut dispatch and send
  // KeyPress to the focused widget, which our Phase 2 will intercept.
  if (t == QEvent::ShortcutOverride) {
    ke->accept();
    return true;
  }

  // Phase 2 — KeyPress: dispatch the shot operation.
  if (isCopy)   onCopyShot();
  else if (isCut)    onCutShot();
  else if (isPaste)  onPasteShot();
  else if (isClone)  onCloneShot();
  else if (isDelete) onDeleteShot();

  ke->accept();
  return true;
}

void StoryboardPanel::keyPressEvent(QKeyEvent *e) {
  TPanel::keyPressEvent(e);
}

void StoryboardPanel::mouseDoubleClickEvent(QMouseEvent *e) {
  // Double-click on the background (not on a shot card) closes the sub-scene.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  if (scene->getChildStack()->getAncestorCount() > 0) {
    CommandManager::instance()->execute("MI_CloseChild");
    return;
  }
  TPanel::mouseDoubleClickEvent(e);
}

void StoryboardPanel::onCopyShot() {
  // Auto-return to main xsheet before operating
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  m_clipboard.clear();
  std::set<int> indices = m_selectedIndices;
  if (indices.empty() && m_selectedShotIndex >= 0) indices.insert(m_selectedShotIndex);
  std::vector<int> sorted(indices.begin(), indices.end());
  std::sort(sorted.begin(), sorted.end());
  for (int idx : sorted) {
    if (idx < 0 || idx >= (int)m_shots.size()) continue;
    ClipboardEntry e;
    e.data      = m_shots[idx].data;
    e.isClone   = false;
    e.srcColumn = idx;
    m_clipboard.push_back(e);
  }
  m_pasteButton->setEnabled(!m_clipboard.empty());
  // Mirror to shared clipboard so Animatic can paste Board copies.
  {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    std::vector<ZtoryClipEntry> shared;
    for (int idx : sorted) {
      if (idx < 0 || idx >= (int)m_shots.size()) continue;
      ZtoryClipEntry ze;
      ze.srcCol   = m_shots[idx].data.xsheetColumn;
      ze.duration = m_shots[idx].data.panels.empty()
                    ? 24 : m_shots[idx].data.panels[0].duration;
      ze.isCut    = false;
      ze.isClone  = false;
      shared.push_back(ze);
    }
    ZtoryModel::instance()->setSharedClip(std::move(shared));
  }
}

void StoryboardPanel::onCutShot() {
  // Immediate cut: save metadata + level reference, then delete shots immediately.
  // cutLevel keeps the TXshChildLevel alive after ColumnCmd::deleteColumn so that
  // onPasteShot() can re-insert the same sub-scene (drawings preserved).
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  m_clipboard.clear();
  std::set<int> indices = m_selectedIndices;
  if (indices.empty() && m_selectedShotIndex >= 0) indices.insert(m_selectedShotIndex);
  std::vector<int> sorted(indices.begin(), indices.end());
  std::sort(sorted.begin(), sorted.end());
  for (int idx : sorted) {
    if (idx < 0 || idx >= (int)m_shots.size()) continue;
    ClipboardEntry e;
    e.data      = m_shots[idx].data;
    e.isClone   = false;
    e.srcColumn = -1;   // original will be deleted immediately
    e.isCut     = true;
    // Grab the TXshLevel before deletion to keep the sub-scene alive
    if (xsh) {
      int col = m_shots[idx].data.xsheetColumn;
      TXshColumn *xshCol = xsh->getColumn(col);
      TXshLevelColumn *lc = xshCol ? xshCol->getLevelColumn() : nullptr;
      if (lc) {
        int r0 = 0, r1 = 0;
        lc->getRange(r0, r1);
        TXshCell cell = lc->getCell(r0);
        if (!cell.isEmpty()) e.cutLevel = cell.m_level;
      }
    }
    m_clipboard.push_back(e);
  }
  m_pasteButton->setEnabled(!m_clipboard.empty());
  // Mirror to shared clipboard so Animatic can paste Board cuts.
  {
    std::vector<ZtoryClipEntry> shared;
    for (const ClipboardEntry &ce : m_clipboard) {
      ZtoryClipEntry ze;
      ze.srcCol   = -1;  // immediate cut: original will be gone
      ze.duration = ce.data.panels.empty() ? 24 : ce.data.panels[0].duration;
      ze.isCut    = true;
      ze.isClone  = false;
      ze.cutLevel = ce.cutLevel;
      shared.push_back(ze);
    }
    ZtoryModel::instance()->setSharedClip(std::move(shared));
  }
  onDeleteShot();  // immediately remove from board and xsheet
}

void StoryboardPanel::onCloneShot() {
  // Auto-return to main xsheet before operating
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  m_clipboard.clear();
  std::set<int> indices = m_selectedIndices;
  if (indices.empty() && m_selectedShotIndex >= 0) indices.insert(m_selectedShotIndex);
  std::vector<int> sorted(indices.begin(), indices.end());
  std::sort(sorted.begin(), sorted.end());
  for (int idx : sorted) {
    if (idx < 0 || idx >= (int)m_shots.size()) continue;
    ClipboardEntry e;
    e.data      = m_shots[idx].data;
    e.isClone   = true;
    e.srcColumn = idx;
    m_clipboard.push_back(e);
  }
  m_pasteButton->setEnabled(!m_clipboard.empty());
  // Mirror to shared clipboard so Animatic can paste Board clones.
  {
    std::vector<ZtoryClipEntry> shared;
    for (int idx : sorted) {
      if (idx < 0 || idx >= (int)m_shots.size()) continue;
      ZtoryClipEntry ze;
      ze.srcCol   = m_shots[idx].data.xsheetColumn;
      ze.duration = m_shots[idx].data.panels.empty()
                    ? 24 : m_shots[idx].data.panels[0].duration;
      ze.isCut    = false;
      ze.isClone  = true;
      shared.push_back(ze);
    }
    ZtoryModel::instance()->setSharedClip(std::move(shared));
  }
}

static void cloneChildToPosition(int srcCol, int dstCol) {
  TApp *app          = TApp::instance();
  ToonzScene *scene  = app->getCurrentScene()->getScene();
  TXsheet *xsh       = app->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(srcCol);
  if (!column) return;
  TXshLevelColumn *lcolumn = column->getLevelColumn();
  if (!lcolumn) return;
  int r0 = 0, r1 = -1;
  lcolumn->getRange(r0, r1);
  if (r0 > r1) return;
  TXshCell cell = lcolumn->getCell(r0);
  if (cell.isEmpty()) return;
  TXshChildLevel *childLevel = cell.m_level->getChildLevel();
  if (!childLevel) return;
  TXsheet *childXsh = childLevel->getXsheet();

  // Inserisci colonna vuota alla posizione target
  xsh->insertColumn(dstCol);

  // Crea nuovo child level clone
  ChildStack *childStack = scene->getChildStack();
  TXshChildLevel *newChildLevel = childStack->createChild(0, dstCol);
  TXsheet *newChildXsh = newChildLevel->getXsheet();

  // Copia contenuto
  std::set<int> indices;
  for (int i = 0; i < childXsh->getColumnCount(); i++) indices.insert(i);
  StageObjectsData *data = new StageObjectsData();
  data->storeColumns(indices, childXsh, 0);
  data->storeColumnFxs(indices, childXsh, 0);
  std::list<int> restoredSplineIds;
  QMap<TStageObjectId, TStageObjectId> idTable;
  QMap<TFx *, TFx *> fxTable;
  data->restoreObjects(indices, restoredSplineIds, newChildXsh,
                       StageObjectsData::eDoClone, idTable, fxTable);
  delete data;

  newChildXsh->getFxDag()->getXsheetFx()->getAttributes()->setDagNodePos(
      childXsh->getFxDag()->getXsheetFx()->getAttributes()->getDagNodePos());
  newChildXsh->updateFrameCount();

  // Rimuovi cella creata da createChild e copia celle originali
  xsh->removeCells(0, dstCol);
  for (int r = r0; r <= r1; r++) {
    TXshCell c = lcolumn->getCell(r);
    if (c.isEmpty()) continue;
    c.m_level = newChildLevel;
    xsh->setCell(r, dstCol, c);
  }

  xsh->updateFrameCount();
  app->getCurrentScene()->setDirtyFlag(true);
  app->getCurrentXsheet()->notifyXsheetChanged();
}

// Paste shared-clipboard entries (ZtoryClipEntry format) directly into xsheet.
// Uses the Board-local cloneChildToPosition() — same logic as Animatic pasteFromClip.
// After this call: xsh->updateFrameCount(), resequenceXsheet(), refreshFromScene().
static void pasteSharedClipToBoard(const std::vector<ZtoryClipEntry> &clip,
                                   int insertCol, TXsheet *xsh, ToonzScene *scene) {
  for (int ci = 0; ci < (int)clip.size(); ci++) {
    int pos = insertCol + ci;
    const ZtoryClipEntry &ce = clip[ci];
    if (ce.isCut && ce.srcCol == -1) {
      // Immediate cut: original already gone, re-insert via saved cutLevel.
      xsh->insertColumn(pos);
      if (ce.cutLevel) {
        for (int r = 0; r < ce.duration; r++)
          xsh->setCell(r, pos, TXshCell(ce.cutLevel, TFrameId(r + 1)));
      } else if (scene) {
        TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
        if (xl && xl->getChildLevel()) {
          TXshChildLevel *cl = xl->getChildLevel();
          for (int r = 0; r < ce.duration; r++)
            xsh->setCell(r, pos, TXshCell(cl, TFrameId(r + 1)));
        }
      }
    } else if (ce.isClone || ce.isCut) {
      // Clone or deferred cut: clone from source column.
      int srcCol = ce.srcCol;
      for (int cj = 0; cj < ci; cj++) if (insertCol + cj <= srcCol) srcCol++;
      cloneChildToPosition(srcCol, pos);
    } else {
      // Copy: share the same TXshChildLevel (shared sub-scene).
      int srcCol = ce.srcCol;
      for (int cj = 0; cj < ci; cj++) if (insertCol + cj <= srcCol) srcCol++;
      TXshColumn *srcColumn = xsh->getColumn(srcCol);
      if (srcColumn) {
        int r0 = 0, r1 = 0;
        srcColumn->getRange(r0, r1);
        xsh->insertColumn(pos);
        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, srcCol >= pos ? srcCol + 1 : srcCol);
          if (!cell.isEmpty()) xsh->setCell(r, pos, cell);
        }
      }
    }
  }
}

void StoryboardPanel::onPasteShot() {
  // Shared clipboard (written by both Board and Animatic) always has priority:
  // it reflects the most recent copy/cut/clone regardless of which panel did it.
  // Local m_clipboard is used only when shared is empty (e.g. first launch).
  const auto &shared = ZtoryModel::instance()->sharedClip();
  if (!shared.empty()) {
    // Use shared clipboard — covers both Board→Board and Animatic→Board paste.
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene)
      while (scene->getChildStack()->getAncestorCount() > 0)
        CommandManager::instance()->execute("MI_CloseChild");
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    // Compute insert column: after selected shot, or at end.
    int insertCol = m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size()
                    ? m_shots[m_selectedShotIndex].data.xsheetColumn + 1
                    : xsh->getColumnCount();
    pasteSharedClipToBoard(shared, insertCol, xsh, scene);
    xsh->updateFrameCount();
    // Clear one-shot entries (cut/clone) from shared clip; keep plain copies.
    {
      auto newShared = shared;
      newShared.erase(std::remove_if(newShared.begin(), newShared.end(),
                      [](const ZtoryClipEntry &e){ return e.isCut || e.isClone; }),
                      newShared.end());
      ZtoryModel::instance()->setSharedClip(std::move(newShared));
    }
    resequenceXsheet();
    refreshFromScene();
    return;
  }
  // Shared clip is empty — fall back to local m_clipboard (legacy path).
  if (m_clipboard.empty()) return;
  // Auto-return to main xsheet before pasting
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  // m_updating=true: prevents our own onShotInserted/onShotRemovedAt from
  // reacting to the ZtoryModel signals we emit below (for other Board instances).
  m_updating = true;
  // Blocca segnali xsheet durante il paste per evitare rebuild intermedi
  disconnect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this, &StoryboardPanel::onXsheetChanged);
  TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
  int insertAt = m_selectedShotIndex >= 0 ? m_selectedShotIndex + 1 : (int)m_shots.size();
  for (int ci = 0; ci < (int)m_clipboard.size(); ci++) {
    int pos     = insertAt + ci;
    int origSrc = m_clipboard[ci].srcColumn;
    // Calcola srcCol: per ogni inserimento precedente che precede origSrc, origSrc si e slittato di +1
    int srcCol  = origSrc;
    for (int cj = 0; cj < ci; cj++) {
      if ((insertAt + cj) <= srcCol) srcCol++;
    }
    // Immediate-cut (srcColumn==-1): original deleted but cutLevel kept alive.
    // Re-insert the same TXshLevel so sub-scene content (drawings) is preserved.
    if (m_clipboard[ci].isCut && origSrc == -1) {
      int duration = m_clipboard[ci].data.panels.empty()
                     ? 24
                     : m_clipboard[ci].data.panels[0].duration;
      xsh->insertColumn(pos);
      if (m_clipboard[ci].cutLevel) {
        // Re-use saved level: drawings intact
        for (int r = 0; r < duration; r++)
          xsh->setCell(r, pos, TXshCell(m_clipboard[ci].cutLevel, TFrameId(r + 1)));
      } else if (scene) {
        // Fallback: create empty subscene (drawings lost, shouldn't normally happen)
        TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
        if (xl && xl->getChildLevel()) {
          TXshChildLevel *cl = xl->getChildLevel();
          for (int r = 0; r < duration; r++)
            xsh->setCell(r, pos, TXshCell(cl, TFrameId(r + 1)));
        }
      }
    }
    // Clone or deferred-cut: clone from still-alive source column.
    // Plain copy: share the same TXshChildLevel instance.
    else if (m_clipboard[ci].isClone || m_clipboard[ci].isCut) {
      cloneChildToPosition(srcCol, pos);
    } else {
      // Copy: riusa lo stesso TXshChildLevel (shared instance)
      TXshColumn *srcColumn = xsh->getColumn(srcCol);
      if (srcColumn) {
        int r0 = 0, r1 = 0;
        srcColumn->getRange(r0, r1);
        xsh->insertColumn(pos);
        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, srcCol >= pos ? srcCol + 1 : srcCol);
          if (!cell.isEmpty()) xsh->setCell(r, pos, cell);
        }
      }
    }
    // Inserisci shot nel modello locale
    Shot newShot;
    newShot.data = m_clipboard[ci].data;
    newShot.data.shotNumber = "";
    newShot.data.xsheetColumn = pos;
    m_shots.insert(m_shots.begin() + pos, newShot);
    for (int pi = 0; pi < (int)newShot.data.panels.size(); pi++) {
      addPanelWidget(pos, pi);
      m_shots[pos].panels[pi]->setDialog(newShot.data.panels[pi].dialog);
      m_shots[pos].panels[pi]->setAction(newShot.data.panels[pi].action);
      m_shots[pos].panels[pi]->setNotes(newShot.data.panels[pi].notes);
      m_shots[pos].panels[pi]->setDuration(newShot.data.panels[pi].duration);
    }
    // Notify other Board instances (and Animatic)
    emit ZtoryModel::instance()->shotAdded(pos);
  }
  if (not m_autoRenumber) {
    for (int ci = 0; ci < (int)m_clipboard.size(); ci++)
      assignKeepNumbers(insertAt + ci);
  }
  renumberAll();

  // Deferred-cut (srcColumn>=0): delete originals now that clones exist.
  // Immediate-cut (srcColumn==-1): originals already deleted in onCutShot(), skip.
  bool anyCut = false;
  for (auto &ce : m_clipboard) if (ce.isCut && ce.srcColumn >= 0) { anyCut = true; break; }
  if (anyCut) {
    TXsheet *xsh2 = TApp::instance()->getCurrentXsheet()->getXsheet();
    std::vector<int> cutCols;
    for (int ci = 0; ci < (int)m_clipboard.size(); ci++) {
      if (!m_clipboard[ci].isCut || m_clipboard[ci].srcColumn < 0) continue;
      int col = m_clipboard[ci].srcColumn;
      // Adjust for each insertion that shifted this column right
      for (int cj = 0; cj < (int)m_clipboard.size(); cj++)
        if ((insertAt + cj) <= col) col++;
      cutCols.push_back(col);
    }
    std::sort(cutCols.rbegin(), cutCols.rend());
    for (int col : cutCols) {
      for (int i = 0; i < (int)m_shots.size(); i++) {
        if (m_shots[i].data.xsheetColumn == col) {
          for (PanelWidget *pw : m_shots[i].panels) { m_grid->removeWidget(pw); delete pw; }
          m_shots.erase(m_shots.begin() + i);
          for (int j = 0; j < (int)m_shots.size(); j++)
            if (m_shots[j].data.xsheetColumn > col) m_shots[j].data.xsheetColumn--;
          break;
        }
      }
      ColumnCmd::deleteColumn(col);
      emit ZtoryModel::instance()->shotRemovedAt(col);  // notify other Board instances
    }
    xsh2->updateFrameCount();
    m_clipboard.clear();
    m_pasteButton->setEnabled(false);
  }

  m_updating = false;
  // Riconnetti segnale xsheet
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this, &StoryboardPanel::onXsheetChanged);
  resequenceXsheet();
  rebuildGrid();
  saveZtoryc();
}

void StoryboardPanel::onDeleteShot() {
  // Raccogli indici da cancellare (selezione multipla o singola)
  std::vector<int> toDelete(m_selectedIndices.begin(), m_selectedIndices.end());
  if (toDelete.empty() && m_selectedShotIndex >= 0) toDelete.push_back(m_selectedShotIndex);
  if (toDelete.empty()) return;

  // Usa data.xsheetColumn (non l'indice Board) per identificare le colonne
  // da cancellare nell'xsheet. Se i due sono disallineati (dopo merge/cut),
  // usare l'indice Board cancellerebbe la colonna sbagliata.
  // Ordina per xsheet column decrescente: cancellare dall'alto mantiene
  // stabili gli indici delle colonne inferiori nelle iterazioni successive.
  std::vector<int> xshCols;
  for (int idx : toDelete) {
    if (idx >= 0 && idx < (int)m_shots.size())
      xshCols.push_back(m_shots[idx].data.xsheetColumn);
  }
  std::sort(xshCols.rbegin(), xshCols.rend());

  disconnect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this, &StoryboardPanel::onXsheetChanged);

  for (int col : xshCols) {
    // Cerca il board shot corrispondente a questa colonna xsheet.
    int si = -1;
    for (int i = 0; i < (int)m_shots.size(); i++)
      if (m_shots[i].data.xsheetColumn == col) { si = i; break; }
    if (si < 0) continue;
    for (PanelWidget *pw : m_shots[si].panels) {
      m_grid->removeWidget(pw);
      delete pw;
    }
    m_shots.erase(m_shots.begin() + si);
    // Aggiorna xsheetColumn degli shot rimasti che erano dopo col.
    for (int i = 0; i < (int)m_shots.size(); i++)
      if (m_shots[i].data.xsheetColumn > col)
        m_shots[i].data.xsheetColumn--;
    ColumnCmd::deleteColumn(col);
  }

  m_selectedShotIndex = -1;
  m_selectedIndices.clear();
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged, this, &StoryboardPanel::onXsheetChanged);
  renumberAll();
  ZtoryModel::instance()->resequenceXsheet();
  rebuildGrid();
  saveZtoryc();
}

void StoryboardPanel::onAddShot() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (scene && scene->getChildStack()->getAncestorCount() > 0)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  int duration = 24;
  int insertAt = (m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
                 ? m_selectedShotIndex + 1
                 : (int)m_shots.size();
  if (scene && xsh) {
    TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
    if (xl && xl->getChildLevel()) {
      TXshChildLevel *cl = xl->getChildLevel();
      xsh->insertColumn(insertAt);
      for (int r = 0; r < duration; r++)
        xsh->setCell(r, insertAt, TXshCell(cl, TFrameId(r+1)));
      xsh->updateFrameCount();

      // Inizializza camera della sottoscena copiando quella del main
      TXsheet *childXsh = cl->getXsheet();
      if (childXsh) {
        TStageObjectTree *parentTree = xsh->getStageObjectTree();
        TStageObjectTree *childTree  = childXsh->getStageObjectTree();
        int tmpCamId = 0;
        for (int cam = 0; cam < parentTree->getCameraCount();) {
          TStageObject *parentCamera = parentTree->getStageObject(
              TStageObjectId::CameraId(tmpCamId), false);
          if (!parentCamera) { tmpCamId++; continue; }
          if (parentCamera->getCamera()) {
            TCamera *childCamera = childTree->getStageObject(
                TStageObjectId::CameraId(tmpCamId))->getCamera();
            if (childCamera) {
              childCamera->setRes(parentCamera->getCamera()->getRes());
              childCamera->setSize(parentCamera->getCamera()->getSize());
            }
          }
          tmpCamId++; cam++;
        }
        childTree->setCurrentCameraId(parentTree->getCurrentCameraId());
      }

      app->getCurrentXsheet()->notifyXsheetChanged();
    }
  }
  Shot shot;
  shot.data.xsheetColumn = insertAt;
  PanelData pd;
  pd.startFrame = 0;
  pd.duration = duration;
  shot.data.panels.push_back(pd);
  m_shots.insert(m_shots.begin() + insertAt, shot);
  addPanelWidget(insertAt, 0);
  if (not m_autoRenumber) assignKeepNumbers(insertAt);
  renumberAll();
  resequenceXsheet();
  rebuildGrid();
  saveZtoryc();
  selectShot(insertAt);
}

void StoryboardPanel::onEditShot(int shotIdx) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;

  // Select this shot in the Board before entering edit mode
  selectShot(shotIdx);
  m_selectedIndices.clear();
  m_selectedIndices.insert(shotIdx);

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (not scene) return;
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  if (not xsh) return;
  int col = m_shots[shotIdx].data.xsheetColumn;
  int row = 0;
  for (int r = 0; r < xsh->getFrameCount(); r++) {
    TXshCell cell = xsh->getCell(r, col);
    if (not cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      row = r; break;
    }
  }
  app->getCurrentColumn()->setColumnIndex(col);
  app->getCurrentFrame()->setFrame(row);
  CommandManager::instance()->execute("MI_OpenChild");
}

void StoryboardPanel::onMatchDuration(int shotIdx) {
  // Resize the main xsheet column to match the sub-scene's actual frame count.
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  int col = m_shots[shotIdx].data.xsheetColumn;
  TXshColumn *column = mainXsh->getColumn(col);
  if (!column) return;

  // Find child level in this column
  TXshChildLevel *cl = nullptr;
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  for (int r = r0; r <= r1 && !cl; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
      cl = cell.m_level->getChildLevel();
  }
  if (!cl) return;

  int actualDuration = cl->getXsheet()->getFrameCount();
  if (actualDuration <= 0) return;
  if (actualDuration == (r1 - r0 + 1)) return;  // already matching

  // Resize column: clear and set new cell count (resequenceXsheet repositions)
  int maxFrames = mainXsh->getFrameCount() + actualDuration + 10;
  for (int r = 0; r <= maxFrames; r++) mainXsh->clearCells(r, col);
  for (int r = 0; r < actualDuration; r++)
    mainXsh->setCell(r, col, TXshCell(cl, TFrameId(r + 1)));

  mainXsh->updateFrameCount();
  ZtoryModel::instance()->resequenceXsheet();
  app->getCurrentXsheet()->notifyXsheetChanged();

  // Update Board T: display
  for (PanelWidget *pw : m_shots[shotIdx].panels)
    pw->setTotalDuration(actualDuration);
  if (m_shots[shotIdx].data.panels.size() == 1) {
    m_shots[shotIdx].data.panels[0].duration = actualDuration;
    if (!m_shots[shotIdx].panels.empty())
      m_shots[shotIdx].panels[0]->setDuration(actualDuration);
  }
  saveZtoryc();
}

void StoryboardPanel::onBackToBoard() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (scene)
    while (scene->getChildStack()->getAncestorCount() > 0)
      CommandManager::instance()->execute("MI_CloseChild");
  MainWindow *mw = dynamic_cast<MainWindow*>(TApp::instance()->getMainWindow());
  if (mw) mw->switchToRoom("BOARD");
}

void StoryboardPanel::onPanelClicked(int shotIdx, int panelIdx, Qt::KeyboardModifiers modifiers) {
  // Focus the scroll area (a child of StoryboardPanel) so that
  // WidgetWithChildrenShortcut QShortcuts on the panel fire correctly.
  m_scrollArea->setFocus(Qt::MouseFocusReason);
  if (modifiers & Qt::ControlModifier || modifiers & Qt::MetaModifier) {
    // Ctrl+click: aggiungi/rimuovi dalla selezione
    if (m_selectedIndices.count(shotIdx)) {
      m_selectedIndices.erase(shotIdx);
      for (PanelWidget *pw : m_shots[shotIdx].panels) pw->setSelected(false);
    } else {
      m_selectedIndices.insert(shotIdx);
      for (PanelWidget *pw : m_shots[shotIdx].panels) pw->setSelected(true);
    }
    m_selectedShotIndex = shotIdx;
  } else if (modifiers & Qt::ShiftModifier) {
    // Shift+click: seleziona range
    int from = qMin(m_selectedShotIndex, shotIdx);
    int to   = qMax(m_selectedShotIndex, shotIdx);
    if (from < 0) from = shotIdx;
    for (int i = 0; i < (int)m_shots.size(); i++) {
      bool sel = (i >= from && i <= to);
      for (PanelWidget *pw : m_shots[i].panels) pw->setSelected(sel);
      if (sel) m_selectedIndices.insert(i);
      else     m_selectedIndices.erase(i);
    }
  } else {
    // Click normale: deseleziona tutto e seleziona solo questo
    for (int i = 0; i < (int)m_shots.size(); i++)
      for (PanelWidget *pw : m_shots[i].panels) pw->setSelected(false);
    m_selectedIndices.clear();
    selectShot(shotIdx);
    m_selectedIndices.insert(shotIdx);
  }
  // Sync Board selection to ZtoryModel so Animatic's merge button can use it.
  {
    std::set<int> cols;
    for (int i : m_selectedIndices)
      if (i >= 0 && i < (int)m_shots.size())
        cols.insert(m_shots[i].data.xsheetColumn);
    if (cols.empty() && m_selectedShotIndex >= 0 && m_selectedShotIndex < (int)m_shots.size())
      cols.insert(m_shots[m_selectedShotIndex].data.xsheetColumn);
    ZtoryModel::instance()->setSharedSelection(std::move(cols));
  }
}

void StoryboardPanel::onDurationChanged(int shotIdx, int panelIdx, int frames) {
  if (shotIdx < 0 || shotIdx >= (int)m_shots.size()) return;
  if (panelIdx < 0 || panelIdx >= (int)m_shots[shotIdx].data.panels.size()) return;
  m_shots[shotIdx].data.panels[panelIdx].duration = frames;
  int tot = m_shots[shotIdx].data.totalDuration();
  for (PanelWidget *pw : m_shots[shotIdx].panels)
    pw->setTotalDuration(tot);
  resequenceXsheet();
  saveZtoryc();
}

void StoryboardPanel::onMoveShot(int fromShot, int toShot) {
  if (!ZtoryModel::assertMainXsheet(true)) return;   // warn: exit edit mode first
  if (fromShot == toShot) return;
  if (fromShot < 0 || fromShot >= (int)m_shots.size()) return;
  if (toShot < 0 || toShot >= (int)m_shots.size()) return;
  Shot s = m_shots[fromShot];
  m_shots.erase(m_shots.begin() + fromShot);
  m_shots.insert(m_shots.begin() + toShot, s);
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (scene) {
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    if (xsh) {
      int maxFrames = xsh->getFrameCount() + 200;
      int numCols = (int)m_shots.size();
      std::vector<std::vector<TXshCell>> cols(numCols);
      for (int c = 0; c < numCols; c++)
        for (int r = 0; r <= maxFrames; r++)
          cols[c].push_back(xsh->getCell(r, c));
      std::vector<TXshCell> tmp = cols[fromShot];
      cols.erase(cols.begin() + fromShot);
      cols.insert(cols.begin() + toShot, tmp);
      for (int c = 0; c < numCols; c++) {
        for (int r = 0; r <= maxFrames; r++) xsh->clearCells(r, c);
        for (int r = 0; r < (int)cols[c].size(); r++)
          if (not cols[c][r].isEmpty()) xsh->setCell(r, c, cols[c][r]);
      }
      xsh->updateFrameCount();
      app->getCurrentXsheet()->notifyXsheetChanged();
    }
  }
  renumberAll();
  resequenceXsheet();
  rebuildGrid();
  saveZtoryc();
  selectShot(toShot);
}

void StoryboardPanel::onColumnsChanged(int value) {
  m_columnsPerRow = value;
  rebuildGrid();
}

void StoryboardPanel::onNumberingChanged(int comboIndex) {
  if (comboIndex == 0) {
    m_autoRenumber = true;
    // Non rinumera subito - lo farà al prossimo addShot
  } else if (comboIndex == 1) {
    m_autoRenumber = false;
  } else if (comboIndex == 2) {
    m_autoRenumber = true;
    for (int i = 0; i < (int)m_shots.size(); i++)
      m_shots[i].data.shotNumber = QString("%1").arg(i+1, 2, 10, QChar(48));
    renumberAll();
    m_numberingCombo->blockSignals(true);
    m_numberingCombo->setCurrentIndex(0);
    m_numberingCombo->blockSignals(false);
  }
  saveZtoryc();
}

void StoryboardPanel::onNumberingConfig() {
  // Inline dialog for configuring the project's shot numbering scheme.
  NumberingConfig &cfg = ZtoryModel::instance()->numberingConfig();

  QDialog dlg(this);
  dlg.setWindowTitle(tr("Shot Numbering Config"));
  dlg.setMinimumWidth(340);
  auto *lay = new QGridLayout(&dlg);
  lay->setColumnStretch(1, 1);
  lay->setSpacing(6);
  lay->setContentsMargins(12, 12, 12, 12);

  // Style
  auto *styleCB = new QComboBox(&dlg);
  styleCB->addItem(tr("Simple   (sh010, sh020…)"));
  styleCB->addItem(tr("Sequence  (sq01_sh010…)"));
  styleCB->setCurrentIndex((int)cfg.style);
  lay->addWidget(new QLabel(tr("Style:"), &dlg),    0, 0);
  lay->addWidget(styleCB,                            0, 1, 1, 3);

  // Shot prefix
  auto *shotPxFld = new QLineEdit(cfg.shotPrefix, &dlg);
  shotPxFld->setMaximumWidth(60);
  lay->addWidget(new QLabel(tr("Shot prefix:"), &dlg), 1, 0);
  lay->addWidget(shotPxFld, 1, 1);

  // Seq prefix
  auto *seqPxLabel = new QLabel(tr("Seq prefix:"), &dlg);
  auto *seqPxFld   = new QLineEdit(cfg.seqPrefix, &dlg);
  seqPxFld->setMaximumWidth(60);
  lay->addWidget(seqPxLabel, 1, 2);
  lay->addWidget(seqPxFld,   1, 3);

  // Step
  auto *stepSB = new QSpinBox(&dlg);
  stepSB->setRange(1, 1000); stepSB->setValue(cfg.step);
  lay->addWidget(new QLabel(tr("Step:"), &dlg),    2, 0);
  lay->addWidget(stepSB,                            2, 1);

  // Padding
  auto *padSB = new QSpinBox(&dlg);
  padSB->setRange(1, 6); padSB->setValue(cfg.padding);
  lay->addWidget(new QLabel(tr("Padding:"), &dlg), 2, 2);
  lay->addWidget(padSB,                             2, 3);

  // Start number
  auto *startSB = new QSpinBox(&dlg);
  startSB->setRange(1, 9999); startSB->setValue(cfg.startNumber);
  lay->addWidget(new QLabel(tr("Start #:"), &dlg), 3, 0);
  lay->addWidget(startSB,                           3, 1);

  // Seq number
  auto *seqNumSB = new QSpinBox(&dlg);
  seqNumSB->setRange(1, 999); seqNumSB->setValue(cfg.seqNumber);
  auto *seqNumLabel = new QLabel(tr("Seq #:"), &dlg);
  lay->addWidget(seqNumLabel, 3, 2);
  lay->addWidget(seqNumSB,    3, 3);

  // Show/hide seq controls based on style
  auto syncSeqVisibility = [&](int idx) {
    bool isSeq = (idx == 1);
    seqPxLabel->setVisible(isSeq);
    seqPxFld->setVisible(isSeq);
    seqNumLabel->setVisible(isSeq);
    seqNumSB->setVisible(isSeq);
  };
  syncSeqVisibility(styleCB->currentIndex());
  connect(styleCB, QOverload<int>::of(&QComboBox::currentIndexChanged),
          &dlg, syncSeqVisibility);

  // Buttons
  auto *btns = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
  lay->addWidget(btns, 4, 0, 1, 4);
  connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted) return;

  // Apply changes
  cfg.style       = (NumberingConfig::Style)styleCB->currentIndex();
  cfg.shotPrefix  = shotPxFld->text().trimmed().isEmpty() ? "sh" : shotPxFld->text().trimmed();
  cfg.seqPrefix   = seqPxFld->text().trimmed().isEmpty()  ? "sq" : seqPxFld->text().trimmed();
  cfg.step        = stepSB->value();
  cfg.padding     = padSB->value();
  cfg.startNumber = startSB->value();
  cfg.seqNumber   = seqNumSB->value();

  // If in auto-renumber mode, renumber all shots immediately
  if (m_autoRenumber) {
    renumberAll();
    saveZtoryc();
  }
}

void StoryboardPanel::onRefreshPreviews() {
  for (int si = 0; si < (int)m_shots.size(); si++)
    for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++)
      updatePreview(si, pi);
}

// ── Audio export helper ───────────────────────────────────────────────────────
// Injects a temporary sound column into childXsh containing only the audio
// that falls within [shotR0, shotR1] of the main xsheet.
// Returns the list of column indices inserted (to be removed after save).
// One column per audio column in the main xsheet that overlaps the shot range.
static QList<int> injectAudioForShot(TXsheet *mainXsh, TXsheet *childXsh,
                                     int shotR0, int shotR1, double fps) {
  QList<int> injected;
  int mainCols = mainXsh->getColumnCount();
  for (int mc = 0; mc < mainCols; mc++) {
    TXshColumn *col = mainXsh->getColumn(mc);
    if (!col) continue;
    TXshSoundColumn *srcSc = col->getSoundColumn();
    if (!srcSc) continue;

    // Collect ColumnLevels that overlap [shotR0, shotR1]
    QList<ColumnLevel *> toInsert;
    for (int li = 0; li < srcSc->getColumnLevelCount(); li++) {
      ColumnLevel *cl = srcSc->getColumnLevel(li);
      if (!cl) continue;
      int vsf = cl->getVisibleStartFrame();
      int vef = cl->getVisibleEndFrame();
      if (vsf > shotR1 || vef < shotR0) continue;  // no overlap

      // Clip to shot boundary
      int clipStart     = std::max(vsf, shotR0);
      int clipEnd       = std::min(vef, shotR1);
      int addStartOff   = clipStart - vsf;
      int addEndOff     = vef - clipEnd;

      // Clone and adjust: startFrame relative to shot start (frame 0).
      // IMPORTANT: use cl->getStartFrame() (raw, before offset), NOT vsf
      // (= startFrame + startOffset). Using vsf would shift the audible
      // region by startOffset frames, making the clip play too late AND
      // extending its visible end past the shot boundary.
      ColumnLevel *newCl = new ColumnLevel(
          cl->getSoundLevel(),
          cl->getStartFrame() - shotR0,            // startFrame relative to shot
          cl->getStartOffset() + addStartOff,      // trimmed start
          cl->getEndOffset()   + addEndOff,        // trimmed end
          fps);
      toInsert.append(newCl);
    }
    if (toInsert.isEmpty()) continue;

    // Insert a new sound column at the end of the child xsheet
    int newCol = childXsh->getColumnCount();
    childXsh->insertColumn(newCol, TXshColumn::eSoundType);
    TXshSoundColumn *dstSc = childXsh->getColumn(newCol)->getSoundColumn();
    if (!dstSc) { for (auto *c : toInsert) delete c; continue; }

    dstSc->setFrameRate(fps);
    for (ColumnLevel *cl : toInsert)
      // adoptLevel() is the public counterpart of the protected insertColumnLevel():
      // it takes ownership of cl and places its visible start at targetFrame.
      // Passing cl->getVisibleStartFrame() keeps the position we set in the constructor.
      dstSc->adoptLevel(cl, cl->getVisibleStartFrame());

    // Mark column as reserved audio (visible in xsheet but not a drawing col)
    TStageObject *obj = childXsh->getStageObjectTree()
                          ->getStageObject(TStageObjectId::ColumnId(newCol), false);
    if (obj) obj->setName("_audio_main_");

    injected.append(newCol);
  }
  return injected;
}

// Remove injected audio columns in reverse order (to keep indices stable)
static void removeInjectedAudio(TXsheet *childXsh, QList<int> cols) {
  std::sort(cols.begin(), cols.end(), std::greater<int>());
  for (int c : cols)
    childXsh->removeColumn(c);
}

void StoryboardPanel::onExportShots() {
  if (m_shots.empty()) {
    QMessageBox::information(this, "Export Shots", "No shots to export.");
    return;
  }

  // Popup selezione range
  QDialog dlg(this);
  dlg.setWindowTitle("Export Shots as Scenes");
  QVBoxLayout *lay = new QVBoxLayout(&dlg);

  QHBoxLayout *rangeLayout = new QHBoxLayout();
  QRadioButton *allRadio = new QRadioButton("All shots");
  QRadioButton *rangeRadio = new QRadioButton("Range:");
  allRadio->setChecked(true);
  QSpinBox *fromSpin = new QSpinBox(); fromSpin->setMinimum(1); fromSpin->setMaximum((int)m_shots.size()); fromSpin->setValue(1);
  QSpinBox *toSpin = new QSpinBox(); toSpin->setMinimum(1); toSpin->setMaximum((int)m_shots.size()); toSpin->setValue((int)m_shots.size());
  QLabel *toLabel = new QLabel("to");
  fromSpin->setEnabled(false); toSpin->setEnabled(false); toLabel->setEnabled(false);
  rangeLayout->addWidget(allRadio);
  rangeLayout->addWidget(rangeRadio);
  rangeLayout->addWidget(fromSpin);
  rangeLayout->addWidget(toLabel);
  rangeLayout->addWidget(toSpin);
  rangeLayout->addStretch();
  lay->addLayout(rangeLayout);

  QObject::connect(rangeRadio, &QRadioButton::toggled, [&](bool checked){
    fromSpin->setEnabled(checked); toSpin->setEnabled(checked); toLabel->setEnabled(checked);
  });

  QDialogButtonBox *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  lay->addWidget(bbox);
  QObject::connect(bbox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(bbox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted) return;

  int from = allRadio->isChecked() ? 0 : fromSpin->value() - 1;
  int to   = allRadio->isChecked() ? (int)m_shots.size() - 1 : toSpin->value() - 1;

  // Export
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TFilePath scenesDir = scene->decodeFilePath(TFilePath("+scenes"));
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  // Use ZtoryModel fps (already synced from scene at load/new) to avoid pulling
  // in the TSceneProperties include just for this one call.
  double fps = (double)ZtoryModel::instance()->fps();

  int ok = 0, fail = 0;
  for (int i = from; i <= to; i++) {
    std::string shotNumStr = m_shots[i].data.shotNumber.toStdString();
    TFilePath outPath = scenesDir + TFilePath("sc" + shotNumStr + ".tnz");

    // Crea cartella scenes se non esiste
    if (!TFileStatus(outPath.getParentDir()).doesExist())
      TSystem::mkDir(outPath.getParentDir());

    // Determina range del main xsheet per questo shot
    int shotCol = m_shots[i].data.xsheetColumn;
    int shotR0 = 0, shotR1 = 0;
    if (mainXsh && mainXsh->getColumn(shotCol))
      mainXsh->getColumn(shotCol)->getRange(shotR0, shotR1);

    // Apri sottoscena
    TApp::instance()->getCurrentColumn()->setColumnIndex(shotCol);
    TColumnSelection *colSel = new TColumnSelection();
    colSel->selectColumn(shotCol, true);
    TSelection::setCurrent(colSel);
    ztoryOpenSubXsheet();

    if (scene->getChildStack()->getAncestorCount() == 0) { fail++; continue; }

    // Inietta audio principale nel child xsheet prima del salvataggio
    TXsheet *childXsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    QList<int> injectedCols;
    if (mainXsh && childXsh && shotR1 >= shotR0)
      injectedCols = injectAudioForShot(mainXsh, childXsh, shotR0, shotR1, fps);

    bool saved = IoCmd::saveScene(outPath, IoCmd::SAVE_SUBXSHEET);
    if (saved) ok++; else fail++;

    // Rimuovi colonne audio temporanee (non devono restare nella sottoscena)
    if (!injectedCols.isEmpty() && childXsh)
      removeInjectedAudio(childXsh, injectedCols);

    ztoryCloseSubXsheet(1);
  }

  QString msg = QString("Export completato: %1 shot esportati").arg(ok);
  if (fail > 0) msg += QString(", %1 falliti").arg(fail);
  QMessageBox::information(this, "Export Shots", msg);
}

void StoryboardPanel::onExportAnimatic() {
  QMessageBox::information(this, "Export Animatic", "To be connected to render pipeline.");
}

void StoryboardPanel::onExportPdf() {
  if (m_shots.empty()) {
    QMessageBox::information(this, "Export PDF", "No shots to export.");
    return;
  }
  QString path = QFileDialog::getSaveFileName(this, "Save Storyboard PDF", "", "PDF (*.pdf)");
  if (path.isEmpty()) return;
  QPdfWriter writer(path);
  writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4),
    QPageLayout::Landscape, QMarginsF(15,15,15,15)));
  writer.setResolution(150);
  QPainter painter(&writer);
  const int cols = 3, pageW = writer.width(), margin = 40;
  const int cellW = (pageW - margin*(cols+1))/cols;
  const int imgH = cellW*9/16;
  bool firstPage = true;
  int pos = 0;
  for (int si = 0; si < (int)m_shots.size(); si++) {
    for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++) {
      int col = pos % cols;
      if (col == 0 && not firstPage) writer.newPage();
      firstPage = false;
      PanelWidget *pw = m_shots[si].panels[pi];
      int x = margin + col*(cellW+margin), y = margin;
      painter.setFont(QFont("Arial", 8, QFont::Bold));
      painter.drawText(x, y-6, QString("S:%1 P:%2/%3")
        .arg(si+1,2,10,QChar(48)).arg(pi+1).arg((int)m_shots[si].panels.size()));
      painter.setPen(QPen(Qt::black, 2));
      painter.drawRect(x, y, cellW, imgH);
      int ty = y+imgH+14;
      painter.setFont(QFont("Arial", 7, QFont::Bold));
      painter.drawText(x, ty, "Dialog:");
      painter.setFont(QFont("Arial", 7));
      painter.drawText(x, ty+12, cellW, 40, Qt::AlignLeft|Qt::TextWordWrap, pw->dialog());
      ty += 56;
      painter.setFont(QFont("Arial", 7, QFont::Bold));
      painter.drawText(x, ty, "Action Notes:");
      painter.setFont(QFont("Arial", 7));
      painter.drawText(x, ty+12, cellW, 40, Qt::AlignLeft|Qt::TextWordWrap, pw->action());
      ty += 56;
      painter.setFont(QFont("Arial", 7, QFont::Bold));
      painter.drawText(x, ty, "Notes:");
      painter.setFont(QFont("Arial", 7));
      painter.drawText(x, ty+12, cellW, 40, Qt::AlignLeft|Qt::TextWordWrap, pw->notes());
      pos++;
    }
  }
  painter.end();
  QMessageBox::information(this, "Export PDF", "Exported to: " + path);
}

class StoryboardPanelFactory final : public TPanelFactory {
public:
  StoryboardPanelFactory() : TPanelFactory("Storyboard") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new StoryboardPanel(parent);
    panel->setObjectName(getPanelType());
    panel->setWindowTitle(QObject::tr("Storyboard"));
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} storyboardPanelFactory;
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
