#pragma once

#ifndef VECTORINSPECTOR_H
#define VECTORINSPECTOR_H

#include <QWidget>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QTreeView>
#include <QFrame>
#include <QAbstractItemModel>
#include <QItemSelection>
#include "multicolumnsortproxymodel.h"
#include "toonzqt/tselectionhandle.h"
#include "tvectorimage.h"
#include "toonz/tscenehandle.h"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QGroupBox;
class QSortFilterProxyModel;
class QHeaderView;
class QStandardItemModel;
QT_END_NAMESPACE

class VectorInspectorPanel final : public QWidget {
  Q_OBJECT

  QScrollArea* m_frameArea;
  TVectorImage* m_vectorImage;
  std::vector<int> m_selectedStrokeIndexes;

public:
  VectorInspectorPanel(QWidget* parent       = nullptr,
                       Qt::WindowFlags flags = Qt::WindowFlags());
  ~VectorInspectorPanel() override = default;

  void setHeaderToolTips();
  void setSourceModel(QAbstractItemModel* model);
  void showContextMenu(const QPoint& pos);
  void copySelectedItemsToClipboard();
  void contextMenuEvent(QContextMenuEvent* event) override;
  void getVectorLineData(std::ostream& os, int stroke,
                         std::vector<int> groupIds) const {}

public slots:
  void onLevelSwitched();
  void onSelectionSwitched(TSelection* selectionFrom, TSelection* selectionTo);
  void setRowHighlighting();
  void onSelectionChanged();
  void onEnteredGroup();
  void onExitedGroup();
  void onChangedStrokes();
  void onToolEditingFinished();
  void onSceneChanged();
  void onStrokeOrderChanged(int, int, int, bool);
  void onToolSwitched();
  void onSelectedAllStrokes();
  void onSelectedStroke(const QModelIndex&);
  void onVectorInspectorSelectionChanged(const QItemSelection&,
                                         const QItemSelection&);
  void updateSelectToolSelectedRows(const QItemSelection&,
                                    const QItemSelection&);

protected:
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;

private slots:
  void filterRegExpChanged();
  void filterColumnChanged();
  void sortChanged();

private:
  MultiColumnSortProxyModel* proxyModel;
  QFrame* proxyGroupBox;
  QTreeView* proxyView;
  QStandardItemModel* sourceModel;
  QLabel* filterPatternLabel;
  QLabel* filterSyntaxLabel;
  QLabel* filterColumnLabel;
  QLineEdit* filterPatternLineEdit;
  QComboBox* filterSyntaxComboBox;
  QComboBox* filterColumnComboBox;
  bool initiatedByVectorInspector;
  bool initiatedBySelectTool;
  bool strokeOrderChangedInProgress;
  bool isSelected(int) const;
  bool selectingRowsForStroke;
  void changeWindowTitle();
};

#endif  // VECTORINSPECTOR_H
