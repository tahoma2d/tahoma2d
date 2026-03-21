#pragma once
#include <QDialog>
#include <QString>

class QLineEdit;
class QComboBox;
class QSpinBox;
class QButtonGroup;
class QLabel;
class QGroupBox;

// ─── ZtoryStartupDialog ────────────────────────────────────────────────────
//
// Modal dialog shown when File > New Scene is called.
// Forces naming + configuration of the project before any content is created,
// avoiding the "untitled path" problem where levels get broken relative paths.

class ZtoryStartupDialog : public QDialog {
  Q_OBJECT

public:
  struct Config {
    // Section 1 — Project
    QString projectName;
    QString projectPath;

    // Section 2 — Camera
    int width, height, fps, totalFrames;

    // Section 3 — Workflow
    enum Workflow { Storyboard, Animatic, StopMotion } workflow;

    // Section 4 — Shot numbering
    enum NumberingStyle { Simple, Sequence } numberingStyle;
    int     step, padding, seqPadding, startNumber, initialShotCount;
    QString seqPrefix, shotPrefix;

    Config();

    // Generates shot name for sequence sq (1-based), shot index idx (0-based)
    QString shotName(int sq, int idx) const;
  };

  explicit ZtoryStartupDialog(QWidget *parent = nullptr);

  Config config() const;

private slots:
  void onNumberingStyleChanged(int idx);
  void onFormatPresetChanged(int idx);
  void onAccepted();

private:
  void buildUi();
  void saveSettings();
  void loadSettings();

  // Section 1
  QLineEdit *m_projectName;
  QLineEdit *m_projectPath;
  QPushButton *m_browseBtn;

  // Section 2
  QComboBox *m_formatPreset;
  QSpinBox  *m_width;
  QSpinBox  *m_height;
  QSpinBox  *m_fps;
  QSpinBox  *m_totalFrames;

  // Section 3
  QButtonGroup *m_workflowGroup;

  // Section 4
  QComboBox *m_numberingStyle;
  QSpinBox  *m_numberingStep;
  QLabel    *m_seqPrefixLabel;
  QLineEdit *m_seqPrefix;
  QLineEdit *m_shotPrefix;
  QSpinBox  *m_numPadding;
  QSpinBox  *m_startNumber;
  QSpinBox  *m_initialShotCount;
};
