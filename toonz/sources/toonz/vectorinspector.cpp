#include "vectorinspector.h"
#include <multicolumnsortproxymodel.h>

#include "tapp.h"

#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshcell.h"

#include "tvectorimage.h"
#include "toonz/tvectorimageutils.h"
#include "tstroke.h"
#include "toonz/tobjecthandle.h"
#include "toonzqt/selection.h"
#include "toonzqt/tselectionhandle.h"
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/strokeselection.h"
#include "../tnztools/vectorselectiontool.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>

#include <QtWidgets>

#include <QRegularExpression>

//------------------------------------------------------------------------------

VectorInspectorPanel::VectorInspectorPanel(QWidget* parent,
                                           Qt::WindowFlags flags)
    : QWidget(parent)
    , m_proxyModel(new MultiColumnSortProxyModel(this))
    , m_proxyView(new QTreeView(this))
    , m_sourceModel(new QStandardItemModel(0, 10, this))
    , m_filterPatternLineEdit(new QLineEdit(this))
    , m_filterPatternLabel(
          new QLabel(tr("&Filter pattern (example 0|1):"), this))
    , m_filterSyntaxComboBox(new QComboBox(this))
    , m_filterSyntaxLabel(new QLabel(tr("Filter &syntax:"), this))
    , m_filterColumnComboBox(new QComboBox(this))
    , m_filterColumnLabel(new QLabel(tr("Filter &column:"), this))
    , m_proxyGroupBox(new QFrame(this))
    , m_currentImageType(TImage::NONE)
    , m_initiatedByVectorInspector(false)
    , m_initiatedBySelectTool(false)
    , m_strokeOrderChangedInProgress(false)
    , m_selectingRowsForStroke(false)
    , m_vectorImage(0) {
  std::vector<int> m_selectedStrokeIndexes = {};

  // Initialize the source model
  m_sourceModel->setHeaderData(0, Qt::Horizontal, tr("Stroke Order"));
  m_sourceModel->setHeaderData(1, Qt::Horizontal, tr("Group Id"));
  m_sourceModel->setHeaderData(2, Qt::Horizontal, tr("Id"));
  m_sourceModel->setHeaderData(3, Qt::Horizontal, tr("StyleId"));
  m_sourceModel->setHeaderData(4, Qt::Horizontal, tr("Self Loop"));
  m_sourceModel->setHeaderData(5, Qt::Horizontal, tr("Quad"));
  m_sourceModel->setHeaderData(6, Qt::Horizontal, tr("P"));
  m_sourceModel->setHeaderData(7, Qt::Horizontal, tr("x"));
  m_sourceModel->setHeaderData(8, Qt::Horizontal, tr("y"));
  m_sourceModel->setHeaderData(9, Qt::Horizontal, tr("Thickness"));

  // Set the source model for the proxy model
  m_proxyModel->setSourceModel(m_sourceModel);

  m_proxyView->setRootIsDecorated(false);
  m_proxyView->setAlternatingRowColors(true);
  m_proxyView->setModel(m_proxyModel);
  m_proxyView->setSortingEnabled(true);

  m_proxyView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_proxyView->setSelectionBehavior(QAbstractItemView::SelectRows);

  m_proxyView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(m_proxyView, &QWidget::customContextMenuRequested, this,
          &VectorInspectorPanel::showContextMenu);

  m_filterPatternLabel->setBuddy(m_filterPatternLineEdit);
  m_filterPatternLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  m_filterSyntaxComboBox->addItem(tr("Regular expression"));
  m_filterSyntaxComboBox->addItem(tr("Wildcard"));
  m_filterSyntaxComboBox->addItem(tr("Fixed string"));
  m_filterSyntaxLabel->setBuddy(m_filterSyntaxComboBox);
  m_filterSyntaxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  m_filterColumnComboBox->addItem(tr("Stroke Order"));
  m_filterColumnComboBox->addItem(tr("Group Id"));
  m_filterColumnComboBox->addItem(tr("Id"));
  m_filterColumnComboBox->addItem(tr("StyleId"));
  m_filterColumnComboBox->addItem(tr("Self Loop"));
  m_filterColumnComboBox->addItem(tr("Quad"));
  m_filterColumnComboBox->addItem(tr("P"));
  m_filterColumnComboBox->addItem(tr("x"));
  m_filterColumnComboBox->addItem(tr("y"));
  m_filterColumnComboBox->addItem(tr("Thickness"));

  m_filterColumnLabel->setBuddy(m_filterColumnComboBox);
  m_filterColumnLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  setHeaderToolTips();

  connect(m_filterPatternLineEdit, &QLineEdit::textChanged, this,
          &VectorInspectorPanel::filterRegExpChanged);
  connect(m_filterSyntaxComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &VectorInspectorPanel::filterRegExpChanged);
  connect(m_filterColumnComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &VectorInspectorPanel::filterColumnChanged);

  connect(m_proxyView, &QTreeView::clicked, this,
          &VectorInspectorPanel::onSelectedStroke);

  connect(m_proxyView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &VectorInspectorPanel::onVectorInspectorSelectionChanged);

  m_proxyGroupBox->setContentsMargins(0, 0, 0, 0);

  QGridLayout* proxyLayout = new QGridLayout;

  // Set size policy for proxyView to be expanding
  m_proxyView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // Create a QScrollArea and set proxyView as its widget
  QScrollArea* scrollArea = new QScrollArea;
  scrollArea->setWidget(m_proxyView);
  scrollArea->setWidgetResizable(true);

  // Add the scroll area to the layout instead of the proxyView directly
  proxyLayout->addWidget(scrollArea, 0, 0, 1, 3);

  proxyLayout->addWidget(m_filterPatternLabel, 1, 0, Qt::AlignRight);
  proxyLayout->addWidget(m_filterPatternLineEdit, 1, 1, 1, 2);
  proxyLayout->addWidget(m_filterSyntaxLabel, 2, 0, Qt::AlignRight);
  proxyLayout->addWidget(m_filterSyntaxComboBox, 2, 1, 1, 2);
  proxyLayout->addWidget(m_filterColumnLabel, 3, 0, Qt::AlignRight);
  proxyLayout->addWidget(m_filterColumnComboBox, 3, 1, 1, 2);
  proxyLayout->setContentsMargins(0, 0, 0, 0);

  // Set margins and spacing to zero
  proxyLayout->setContentsMargins(0, 0, 0, 0);
  proxyLayout->setSpacing(0);

  m_proxyGroupBox->setLayout(proxyLayout);

  // Add the scroll area to your main layout or widget
  QVBoxLayout* mainLayout = new QVBoxLayout;

  // Set margins and spacing to zero
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Add the proxyGroupBox to the main layout
  mainLayout->addWidget(m_proxyGroupBox, 1);

  setLayout(mainLayout);

  m_proxyView->sortByColumn(0, Qt::AscendingOrder);
  m_filterColumnComboBox->setCurrentIndex(
      6);  // position 6 for the 'P' column of the Vector Inspector QTreeView
  m_filterPatternLineEdit->setText(
      "2");  // set default filter here, like: "0|1" to filter on value 0 or 1
}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::setHeaderToolTips() {
  // Get the source model and set the tooltips
  if (m_sourceModel) {
    m_sourceModel->setHeaderData(0, Qt::Horizontal,
                                 tr("Order of the stroke in the sequence"),
                                 Qt::ToolTipRole);
    m_sourceModel->setHeaderData(
        1, Qt::Horizontal, tr("Identifier for the group"), Qt::ToolTipRole);
    m_sourceModel->setHeaderData(2, Qt::Horizontal,
                                 tr("Unique identifier for the stroke"),
                                 Qt::ToolTipRole);
    m_sourceModel->setHeaderData(
        3, Qt::Horizontal, tr("The palette style index assigned to the stroke"),
        Qt::ToolTipRole);
    m_sourceModel->setHeaderData(
        4, Qt::Horizontal, tr("Indicates if the stroke is a closed shape"),
        Qt::ToolTipRole);
    m_sourceModel->setHeaderData(
        5, Qt::Horizontal,
        tr("Order of the quadratic equation in the sequence per stroke"),
        Qt::ToolTipRole);
    m_sourceModel->setHeaderData(
        6, Qt::Horizontal,
        tr("This point is parameter P0, P1, or P2 in the Quad equation"),
        Qt::ToolTipRole);
    m_sourceModel->setHeaderData(
        7, Qt::Horizontal, tr("X coordinate of the point"), Qt::ToolTipRole);
    m_sourceModel->setHeaderData(
        8, Qt::Horizontal, tr("Y coordinate of the point"), Qt::ToolTipRole);
    m_sourceModel->setHeaderData(9, Qt::Horizontal,
                                 tr("Thickness of the stroke at this point"),
                                 Qt::ToolTipRole);
  }
}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::onLevelSwitched() {
  m_currentImageType = TApp::instance()->getCurrentImageType();

  if (m_currentImageType == TImage::VECTOR) {
    TApp* app = TApp::instance();

    TXshLevelHandle* currentLevel = app->getCurrentLevel();

    TFrameHandle* currentFrame = app->getCurrentFrame();

    TColumnHandle* currentColumn = app->getCurrentColumn();

    int frameIndex = currentFrame->getFrameIndex();

    TXshSimpleLevel* currentSimpleLevel = currentLevel->getSimpleLevel();

    int row = currentFrame->getFrame();
    int col = currentColumn->getColumnIndex();

    TXsheet* xsheet = app->getCurrentXsheet()->getXsheet();

    const TXshCell cell = xsheet->getCell(row, col);

    TVectorImageP vectorImage = cell.getImage(false);
    m_vectorImage             = vectorImage.getPointer();

    UINT strokeCount = vectorImage->getStrokeCount();

    if (vectorImage) {
      QObject::connect(m_vectorImage, &TVectorImage::changedStrokes, this,
                       &VectorInspectorPanel::onChangedStrokes);

      QObject::connect(m_vectorImage, &TVectorImage::enteredGroup, this,
                       &VectorInspectorPanel::onEnteredGroup);

      QObject::connect(m_vectorImage, &TVectorImage::exitedGroup, this,
                       &VectorInspectorPanel::onExitedGroup);

      QObject::connect(m_vectorImage, &TVectorImage::changedStrokeOrder, this,
                       &VectorInspectorPanel::onStrokeOrderChanged);

      QObject::connect(m_vectorImage, &TVectorImage::selectedAllStrokes, this,
                       &VectorInspectorPanel::onSelectedAllStrokes);

      updateStrokeListData();

      // Autosizing columns after the model is populated
      for (int column = 1; column < m_proxyModel->columnCount(); ++column) {
        m_proxyView->resizeColumnToContents(column);
      }
      m_proxyView->setColumnWidth(0, 100);  // Stroke Order column width
    }
  } else {
    if (m_vectorImage) {
      QObject::disconnect(m_vectorImage, &TVectorImage::changedStrokes, this,
                          &VectorInspectorPanel::onChangedStrokes);

      QObject::disconnect(m_vectorImage, &TVectorImage::enteredGroup, this,
                          &VectorInspectorPanel::onEnteredGroup);

      QObject::disconnect(m_vectorImage, &TVectorImage::exitedGroup, this,
                          &VectorInspectorPanel::onExitedGroup);

      QObject::disconnect(m_vectorImage, &TVectorImage::changedStrokeOrder,
                          this, &VectorInspectorPanel::onStrokeOrderChanged);

      QObject::disconnect(m_vectorImage, &TVectorImage::selectedAllStrokes,
                          this, &VectorInspectorPanel::onSelectedAllStrokes);

      m_vectorImage = 0;
    }

    setSourceModel(new MultiColumnSortProxyModel);
  }
  update();
  changeWindowTitle();
}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::onSelectionSwitched(TSelection* selectionFrom,
                                               TSelection* selectionTo) {}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::setRowHighlighting() {
  m_proxyView->selectionModel()->blockSignals(true);

  QItemSelectionModel* selectionModel = m_proxyView->selectionModel();

  // Clear the previous selection
  selectionModel->clearSelection();
  m_proxyModel->invalidate();

  m_selectedStrokeIndexes = {};

  QModelIndex index =
      m_proxyModel->index(0, 0);  // the first column of the first row

  ToolHandle* currentTool = TApp::instance()->getCurrentTool();

  if ("T_Selection" == currentTool->getTool()->getName()) {
    StrokeSelection* strokeSelection =
        dynamic_cast<StrokeSelection*>(currentTool->getTool()->getSelection());

    if (strokeSelection) {
      if (strokeSelection) {
        // initialize the lookup value list.
        for (int i = 0; i < (int)m_proxyModel->rowCount(); i++) {
          if (strokeSelection->isSelected(i)) {
            index = m_proxyModel->index(i, 0);
            m_selectedStrokeIndexes.push_back(i);
          }
        }
        // now if selected rows then set them in the QTreeView
        if (m_selectedStrokeIndexes.size() > 0) {
          for (int i = 0; i < (int)m_proxyModel->rowCount(); i++) {
            if (isSelected(m_proxyModel->index(i, 0).data().toInt())) {
              index = m_proxyModel->index(i, 0);
              selectionModel->select(index, QItemSelectionModel::Select |
                                                QItemSelectionModel::Rows);
            }
          }
        } else {
          selectionModel->clearSelection();
        }
      }
    }
    // scroll to show selected rows
    QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

    if (!selectedIndexes.isEmpty()) {
      QModelIndex firstSelectedIndex = selectedIndexes.first();
      m_proxyView->scrollTo(firstSelectedIndex,
                            QAbstractItemView::PositionAtTop);
    }
  }

  m_proxyView->selectionModel()->blockSignals(false);
}

//-----------------------------------------------------------------------------

bool VectorInspectorPanel::isSelected(int index) const {
  return (std::find(m_selectedStrokeIndexes.begin(),
                    m_selectedStrokeIndexes.end(),
                    index) != m_selectedStrokeIndexes.end());
}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::onSelectionChanged() {
  if (m_currentImageType != TImage::VECTOR) return;

  if (m_initiatedByVectorInspector) {
  } else {
    m_initiatedBySelectTool = true;

    setRowHighlighting();

    m_initiatedBySelectTool = false;
  }
}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::showEvent(QShowEvent*) {
  QObject::connect(TApp::instance()->getCurrentLevel(),
                   &TXshLevelHandle::xshLevelSwitched, this,
                   &VectorInspectorPanel::onLevelSwitched);

  QObject::connect(TApp::instance()->getCurrentFrame(),
                   &TFrameHandle::frameSwitched, this,
                   &VectorInspectorPanel::onLevelSwitched);

  QObject::connect(TApp::instance()->getCurrentSelection(),
                   &TSelectionHandle::selectionSwitched, this,
                   &VectorInspectorPanel::onSelectionSwitched);
  QObject::connect(TApp::instance()->getCurrentSelection(),
                   &TSelectionHandle::selectionChanged, this,
                   &VectorInspectorPanel::onSelectionChanged);

  QObject::connect(TApp::instance()->getCurrentTool(),
                   &ToolHandle::toolSwitched, this,
                   &VectorInspectorPanel::onToolSwitched);
  QObject::connect(TApp::instance()->getCurrentTool(),
                   &ToolHandle::toolEditingFinished, this,
                   &VectorInspectorPanel::onToolEditingFinished);

  QObject::connect(TApp::instance()->getCurrentScene(),
                   &TSceneHandle::sceneSwitched, this,
                   &VectorInspectorPanel::onSceneChanged);
  QObject::connect(TApp::instance()->getCurrentScene(),
                   &TSceneHandle::sceneChanged, this,
                   &VectorInspectorPanel::onSceneChanged);

  onLevelSwitched();
  onSelectionChanged();
}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::hideEvent(QHideEvent*) {
  QObject::disconnect(TApp::instance()->getCurrentLevel(),
                      &TXshLevelHandle::xshLevelSwitched, this,
                      &VectorInspectorPanel::onLevelSwitched);

  QObject::disconnect(TApp::instance()->getCurrentFrame(),
                      &TFrameHandle::frameSwitched, this,
                      &VectorInspectorPanel::onLevelSwitched);

  QObject::disconnect(TApp::instance()->getCurrentSelection(),
                      &TSelectionHandle::selectionSwitched, this,
                      &VectorInspectorPanel::onSelectionSwitched);
  QObject::disconnect(TApp::instance()->getCurrentSelection(),
                      &TSelectionHandle::selectionChanged, this,
                      &VectorInspectorPanel::onSelectionChanged);

  QObject::disconnect(TApp::instance()->getCurrentTool(),
                      &ToolHandle::toolSwitched, this,
                      &VectorInspectorPanel::onToolSwitched);
  QObject::disconnect(TApp::instance()->getCurrentTool(),
                      &ToolHandle::toolEditingFinished, this,
                      &VectorInspectorPanel::onToolEditingFinished);

  QObject::disconnect(TApp::instance()->getCurrentScene(),
                      &TSceneHandle::sceneSwitched, this,
                      &VectorInspectorPanel::onSceneChanged);
  QObject::disconnect(TApp::instance()->getCurrentScene(),
                      &TSceneHandle::sceneChanged, this,
                      &VectorInspectorPanel::onSceneChanged);

  if (m_vectorImage) {
    QObject::disconnect(m_vectorImage, &TVectorImage::changedStrokes, this,
                        &VectorInspectorPanel::onChangedStrokes);

    QObject::disconnect(m_vectorImage, &TVectorImage::enteredGroup, this,
                        &VectorInspectorPanel::onEnteredGroup);

    QObject::disconnect(m_vectorImage, &TVectorImage::exitedGroup, this,
                        &VectorInspectorPanel::onExitedGroup);

    QObject::disconnect(m_vectorImage, &TVectorImage::changedStrokeOrder, this,
                        &VectorInspectorPanel::onStrokeOrderChanged);

    QObject::disconnect(m_vectorImage, &TVectorImage::selectedAllStrokes, this,
                        &VectorInspectorPanel::onSelectedAllStrokes);

    m_vectorImage = 0;
  }
}

void VectorInspectorPanel::setSourceModel(QAbstractItemModel* model) {
  m_proxyModel->setSourceModel(model);
}

void VectorInspectorPanel::filterRegExpChanged() {
  QString pattern = m_filterPatternLineEdit->text();
  switch (m_filterSyntaxComboBox->currentIndex()) {
  case 1:  // Wildcard
    pattern =
        QRegularExpression::wildcardToRegularExpression("*" + pattern + "*");
    break;
  case 2:  // FixedString
    pattern = QRegularExpression::escape(pattern);
    break;
  }
  QRegularExpression regExp(pattern, QRegularExpression::CaseInsensitiveOption);
  m_proxyModel->setFilterRegularExpression(regExp);
  setRowHighlighting();
}

void VectorInspectorPanel::filterColumnChanged() {
  m_proxyModel->setFilterKeyColumn(m_filterColumnComboBox->currentIndex());
}

void VectorInspectorPanel::sortChanged() {}

void VectorInspectorPanel::copySelectedItemsToClipboard() {
  QModelIndexList indexes = m_proxyView->selectionModel()->selectedIndexes();

  QString selectedText;
  QMap<int, QString> rows;  // Use a map to sort by row number automatically

  // Collect data from each selected cell, grouped by row
  for (const QModelIndex& index : qAsConst(indexes)) {
    rows[index.row()] +=
        index.data(Qt::DisplayRole).toString() + "\t";  // Tab-separated values
  }

  // Concatenate rows into a single string, each row separated by a newline
  for (const QString& row : qAsConst(rows)) {
    selectedText += row.trimmed() + "\n";  // Remove trailing tab
  }

  // Copy the collected data to the clipboard
  QClipboard* clipboard = QApplication::clipboard();
  clipboard->setText(selectedText);
}

void VectorInspectorPanel::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);
  QAction* copyAction = menu.addAction(tr("Copy"));
  connect(copyAction, &QAction::triggered, this,
          &VectorInspectorPanel::copySelectedItemsToClipboard);

  // Show the menu at the cursor position
  menu.exec(event->globalPos());
}

void VectorInspectorPanel::showContextMenu(const QPoint& pos) {
  QPoint globalPos = m_proxyView->viewport()->mapToGlobal(pos);
  QMenu menu;
  QAction* copyAction = menu.addAction(tr("Copy"));
  connect(copyAction, &QAction::triggered, this,
          &VectorInspectorPanel::copySelectedItemsToClipboard);

  menu.exec(globalPos);
}

void VectorInspectorPanel::onEnteredGroup() {
  updateStrokeListData();
  m_proxyView->selectionModel()->clearSelection();
}

void VectorInspectorPanel::onExitedGroup() { updateStrokeListData(); }

void VectorInspectorPanel::onChangedStrokes() { updateStrokeListData(); }

void VectorInspectorPanel::onToolEditingFinished() {}

void VectorInspectorPanel::onSceneChanged() {
  if (m_currentImageType != TImage::VECTOR) return;
}

void VectorInspectorPanel::onStrokeOrderChanged(int fromIndex, int count,
                                                int moveBefore, bool regroup) {
  m_strokeOrderChangedInProgress = true;
  bool isSortOrderAscending      = true;

  ToolHandle* currentTool = TApp::instance()->getCurrentTool();
  StrokeSelection* strokeSelection =
      dynamic_cast<StrokeSelection*>(currentTool->getTool()->getSelection());

  strokeSelection->notifyView();

  updateStrokeListData();

  std::vector<int> theIndexes = strokeSelection->getSelection();

  QItemSelectionModel* selectionModel = m_proxyView->selectionModel();

  selectionModel->clearSelection();

  Qt::SortOrder sortOrder = m_proxyView->header()->sortIndicatorOrder();

  if (sortOrder == Qt::AscendingOrder) {
    isSortOrderAscending = true;
  } else if (sortOrder == Qt::DescendingOrder) {
    isSortOrderAscending = false;
  }

  QModelIndex index =
      m_proxyModel->index(0, 0);  // the first column of the first row

  int moveAmount = (fromIndex < moveBefore) ? moveBefore - 1 - fromIndex
                                            : moveBefore - fromIndex;

  int maxIndexValue   = m_proxyModel->rowCount() - 1;  // theIndexes.size() - 1;
  int minIndexValue   = 0;
  int currentRowIndex = 0;

  // get the maximum stroke index value, assume sorted values, no gaps
  int lastRow            = m_proxyModel->rowCount() - 1;
  QModelIndex firstIndex = m_proxyModel->index(0, 0);
  QModelIndex lastIndex  = m_proxyModel->index(lastRow, 0);
  int maxStrokeIndex     = std::max(m_proxyModel->data(firstIndex).toInt(),
                                    m_proxyModel->data(lastIndex).toInt());

  // iterate and create list of selected indexes
  m_selectedStrokeIndexes.clear();

  if (moveAmount >= 0) {
    for (auto it = theIndexes.rbegin(); it != theIndexes.rend(); ++it) {
      currentRowIndex = *it + moveAmount;

      // at the upper row limit?
      if (currentRowIndex > maxIndexValue) {
        currentRowIndex = maxIndexValue;
        maxIndexValue--;
      }
      m_selectedStrokeIndexes.push_back(currentRowIndex);
    }
  } else {
    for (auto it = theIndexes.begin(); it != theIndexes.end(); ++it) {
      currentRowIndex = *it + moveAmount;

      // at the lower row limit?
      if (currentRowIndex < minIndexValue) {
        currentRowIndex = minIndexValue;
        minIndexValue++;
      }
      m_selectedStrokeIndexes.push_back(currentRowIndex);
    }
  }

  // now set selected rows in the QTreeView
  for (int i = 0; i < (int)m_proxyModel->rowCount(); i++) {
    if (isSelected(m_proxyModel->index(i, 0).data().toInt())) {
      index = m_proxyModel->index(i, 0);
      selectionModel->select(
          index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
  }

  // scroll to show selected rows
  QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

  if (!selectedIndexes.isEmpty()) {
    QModelIndex firstSelectedIndex = selectedIndexes.first();
    m_proxyView->scrollTo(firstSelectedIndex, QAbstractItemView::PositionAtTop);
  }

  m_strokeOrderChangedInProgress = false;
}

void VectorInspectorPanel::onToolSwitched() {
  if (m_currentImageType != TImage::VECTOR) return;

  setRowHighlighting();
}

void VectorInspectorPanel::onSelectedAllStrokes() { setRowHighlighting(); }

void VectorInspectorPanel::onSelectedStroke(const QModelIndex& index) {
  m_selectingRowsForStroke = true;
  setRowHighlighting();
  m_selectingRowsForStroke = false;
}

void VectorInspectorPanel::onVectorInspectorSelectionChanged(
    const QItemSelection& selected, const QItemSelection& deselected) {
  // Handle the selection changed event
  if (m_strokeOrderChangedInProgress) {
  } else if (m_initiatedBySelectTool) {
  } else if (m_selectingRowsForStroke) {
  } else {
    m_initiatedByVectorInspector = true;

    updateSelectToolSelectedRows(selected, deselected);

    m_initiatedByVectorInspector = false;
  }
}

void VectorInspectorPanel::updateSelectToolSelectedRows(
    const QItemSelection& selected, const QItemSelection& deselected) {
  ToolHandle* currentTool = TApp::instance()->getCurrentTool();
  VectorSelectionTool* vectorSelectionTool =
      dynamic_cast<VectorSelectionTool*>(currentTool->getTool());
  StrokeSelection* strokeSelection =
      dynamic_cast<StrokeSelection*>(currentTool->getTool()->getSelection());

  if (strokeSelection) {
    m_proxyView->blockSignals(true);

    QModelIndexList selectedIndexes = selected.indexes();
    for (const QModelIndex& index : selectedIndexes) {
      if (index.column() == 0) {  // Check if the column is the first column
        int selectedStrokeIndex = index.data(Qt::DisplayRole).toInt();
        strokeSelection->select(selectedStrokeIndex, 1);
        strokeSelection->notifyView();
      }
    }

    QModelIndexList deselectedIndexes = deselected.indexes();
    int priorRowStrokeIndexValue      = -1;
    for (const QModelIndex& index : deselectedIndexes) {
      if (index.column() == 0) {  // Check if the column is the first column
        int deselectedStrokeIndex = index.data(Qt::DisplayRole).toInt();
        if (deselectedStrokeIndex == priorRowStrokeIndexValue) {
        } else {
          strokeSelection->select(deselectedStrokeIndex, 0);
          strokeSelection->notifyView();
          priorRowStrokeIndexValue = deselectedStrokeIndex;
        }
      }
    }
    m_proxyView->blockSignals(false);
  }
}

void VectorInspectorPanel::updateStrokeListData() {
  m_vectorImage->getStrokeListData(parentWidget(), m_sourceModel);
  setSourceModel(m_sourceModel);
  m_proxyModel->invalidate();
}

void VectorInspectorPanel::changeWindowTitle() {
  TApp* app = TApp::instance();

  TXshSimpleLevel* currentLevel = app->getCurrentLevel()->getSimpleLevel();
  std::wstring levelName = (currentLevel) ? currentLevel->getName() : L"";

  TFrameHandle* currentFrame = app->getCurrentFrame();
  std::string frameNumber =
      (currentFrame) ? std::to_string(currentFrame->getFrame() + 1) : "";

  QString name =
      "Vector Inspector  ::  Level : " + QString::fromStdWString(levelName) +
      "  ::  Frame : " + QString::fromStdString(frameNumber);

  parentWidget()->setWindowTitle(name);
}
