#pragma once
#ifndef STORYBOARDPANEL_H
#define STORYBOARDPANEL_H

#include "pane.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshchildlevel.h"
#include "toonzqt/icongenerator.h"
#include "toonz/childstack.h"
#include "viewerpane.h"

#include <QWidget>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QFrame>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QResizeEvent>
#include <QPixmap>
#include <QLineEdit>
#include <QComboBox>
#include <QStackedWidget>

#include "ztorymodel.h"

class PanelWidget final : public QFrame {
  Q_OBJECT
  int  m_shotIndex;
  int  m_panelIndex;
  int  m_panelCount;
  int  m_fps;
  bool m_selected;
  QLineEdit   *m_shotLabel;
  QLabel      *m_panelLabel;
  QLabel      *m_durationLabel;
  QLabel      *m_totalLabel;
  QSpinBox    *m_totalSpin;
  QSpinBox    *m_durationSpin;
  QPushButton *m_editButton;
  QLabel      *m_previewLabel;
  QPixmap      m_previewPixmap;
  QTextEdit   *m_dialogField;
  QTextEdit   *m_actionField;
  QTextEdit   *m_notesField;
  QLabel* makeFieldLabel(const QString &text);
  QString framesToTimecode(int frames) const;
  void    updateBorderStyle();
public:
  explicit PanelWidget(QWidget *parent = nullptr);
  void setShotIndex(int si);
  void setPanelIndex(int pi, int total);
  void setFps(int fps);
  void setDuration(int frames);
  void setTotalDuration(int frames);
  void setPreviewPixmap(const QPixmap &px);
  void setSelected(bool sel);
  void setDialog(const QString &t);
  void setAction(const QString &t);
  void setNotes(const QString &t);
  void setShotNumber(const QString &n);
  void rescalePreview();
  int     shotIndex()  const { return m_shotIndex; }
  int     panelIndex() const { return m_panelIndex; }
  int     duration()   const;
  QString dialog()     const;
  QString action()     const;
  QString notes()      const;
  bool    isSelected() const { return m_selected; }
  QString shotNumber() const { return m_shotLabel->text(); }
signals:
  void totalDurationChanged(int frames);
  void editRequested(int shotIndex);
  void durationChanged(int shotIndex, int panelIndex, int frames);
  void dataChanged(int shotIndex, int panelIndex);
  void clicked(int shotIndex, int panelIndex, Qt::KeyboardModifiers modifiers);
  void dropReceived(int fromShot, int toShot);
protected:
  void mousePressEvent(QMouseEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dropEvent(QDropEvent *e) override;
private slots:
  void onEditClicked();
  void onDurationSpinChanged(int value);
};

class StoryboardPanel final : public TPanel {
  Q_OBJECT
  QScrollArea *m_scrollArea;
  QWidget     *m_container;
  QGridLayout *m_grid;
  QPushButton *m_addShotButton;
  QPushButton *m_deleteButton;
  QPushButton *m_copyButton;
  QPushButton *m_cloneButton;
  QPushButton *m_pasteButton;
  QPushButton *m_refreshButton;
  QPushButton *m_numberingBtn;   // ⚙ Numbering config button
  QTimer      *m_panelDetectTimer;
  QPushButton *m_exportPdfButton;
  QPushButton *m_exportShotsButton;
  QPushButton *m_exportAnimaticButton;
  QSpinBox    *m_columnsPerRowSpin;
  QComboBox   *m_numberingCombo;
  QStackedWidget   *m_stack;
  SceneViewerPanel *m_comboViewer;
  QPushButton      *m_backButton;
  struct Shot {
    ShotData               data;
    std::vector<PanelWidget*> panels;
    bool selected = false;
  };
  std::vector<Shot> m_shots;
  int  m_columnsPerRow;
  int              m_selectedShotIndex;
  std::set<int>     m_selectedIndices;

  struct ClipboardEntry {
    ShotData data;
    bool     isClone;
    int      srcColumn;
  };
  std::vector<ClipboardEntry> m_clipboard;
  int  m_fps;
  bool m_autoRenumber;
  void addPanelWidget(int shotIdx, int panelIdx);
  void connectPanelWidget(PanelWidget *pw);
  void renumberAll();
  void resequenceXsheet();
  void clearShots();
  void updatePreview(int shotIdx, int panelIdx);
  void rebuildGrid();
  void selectShot(int shotIdx);
  int  currentShotIndex() const;
  void detectAndUpdatePanels(int shotIdx);
  void assignKeepNumbers(int insertAt);
  QString ztoryPath() const;
  void    saveZtoryc();
  void    updateColumnName(int si);
  void    loadZtoryc();
public:
  explicit StoryboardPanel(QWidget *parent = nullptr);
  void refreshFromScene();
protected:
  void showEvent(QShowEvent *e) override;
private slots:
  void onAddShot();
  void onDeleteShot();
  void onCloneShot();
  void onCopyShot();
  void onPasteShot();
protected:
  void keyPressEvent(QKeyEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;
  void onEditShot(int shotIdx);
  void onPanelClicked(int shotIdx, int panelIdx, Qt::KeyboardModifiers modifiers);
  void onDurationChanged(int shotIdx, int panelIdx, int frames);
  void onMoveShot(int fromShot, int toShot);
  void onColumnsChanged(int value);
  void onNumberingChanged(int comboIndex);
  void onNumberingConfig();   // opens the numbering settings dialog
  void onRefreshPreviews();
  void onExportPdf();
  void onExportShots();
  void onExportAnimatic();
  void onXsheetChanged();
  void onModelResequenced(); // called when ZtoryModel::resequenceXsheet runs
  void onShotInserted(int col); // called when razor/external op inserts a shot at col
  void onBackToBoard();
};

#endif // STORYBOARDPANEL_H
