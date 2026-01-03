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
  int m_currentImageType;

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

  void updateStrokeListData();

protected:
  void showEvent(QShowEvent*) override;
  void hideEvent(QHideEvent*) override;

private slots:
  void filterRegExpChanged();
  void filterColumnChanged();
  void sortChanged();

private:
  MultiColumnSortProxyModel* m_proxyModel;
  QFrame* m_proxyGroupBox;
  QTreeView* m_proxyView;
  QStandardItemModel* m_sourceModel;
  QLabel* m_filterPatternLabel;
  QLabel* m_filterSyntaxLabel;
  QLabel* m_filterColumnLabel;
  QLineEdit* m_filterPatternLineEdit;
  QComboBox* m_filterSyntaxComboBox;
  QComboBox* m_filterColumnComboBox;
  bool m_initiatedByVectorInspector;
  bool m_initiatedBySelectTool;
  bool m_strokeOrderChangedInProgress;
  bool isSelected(int) const;
  bool m_selectingRowsForStroke;
  void changeWindowTitle();
};

#endif  // VECTORINSPECTOR_H
