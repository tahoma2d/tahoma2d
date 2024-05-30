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

//------------------------------------------------------------------------------

VectorInspectorPanel::VectorInspectorPanel(QWidget* parent,
                                           Qt::WindowFlags flags)
    : QWidget(parent)
    , proxyModel(new MultiColumnSortProxyModel(this))
    , proxyView(new QTreeView(this))
    , sourceModel(new QStandardItemModel(0, 10, this))
    , filterPatternLineEdit(new QLineEdit(this))
    , filterPatternLabel(new QLabel(tr("&Filter pattern (example 0|1):"), this))
    , filterSyntaxComboBox(new QComboBox(this))
    , filterSyntaxLabel(new QLabel(tr("Filter &syntax:"), this))
    , filterColumnComboBox(new QComboBox(this))
    , filterColumnLabel(new QLabel(tr("Filter &column:"), this))
    , proxyGroupBox(new QFrame(this)) {
  initiatedByVectorInspector   = false;
  initiatedBySelectTool        = false;
  strokeOrderChangedInProgress = false;
  selectingRowsForStroke       = false;

  std::vector<int> m_selectedStrokeIndexes = {};

  // Initialize the source model
  sourceModel->setHeaderData(0, Qt::Horizontal, tr("Stroke Order"));
  sourceModel->setHeaderData(1, Qt::Horizontal, tr("Group Id"));
  sourceModel->setHeaderData(2, Qt::Horizontal, tr("Id"));
  sourceModel->setHeaderData(3, Qt::Horizontal, tr("StyleId"));
  sourceModel->setHeaderData(4, Qt::Horizontal, tr("Self Loop"));
  sourceModel->setHeaderData(5, Qt::Horizontal, tr("Quad"));
  sourceModel->setHeaderData(6, Qt::Horizontal, tr("P"));
  sourceModel->setHeaderData(7, Qt::Horizontal, tr("x"));
  sourceModel->setHeaderData(8, Qt::Horizontal, tr("y"));
  sourceModel->setHeaderData(9, Qt::Horizontal, tr("Thickness"));

  // Set the source model for the proxy model
  proxyModel->setSourceModel(sourceModel);

  proxyView->setRootIsDecorated(false);
  proxyView->setAlternatingRowColors(true);
  proxyView->setModel(proxyModel);
  proxyView->setSortingEnabled(true);

  proxyView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  proxyView->setSelectionBehavior(QAbstractItemView::SelectRows);

  proxyView->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(proxyView, &QWidget::customContextMenuRequested, this,
          &VectorInspectorPanel::showContextMenu);

  filterPatternLabel->setBuddy(filterPatternLineEdit);
  filterPatternLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  filterSyntaxComboBox->addItem(tr("Regular expression"), QRegExp::RegExp);
  filterSyntaxComboBox->addItem(tr("Wildcard"), QRegExp::Wildcard);
  filterSyntaxComboBox->addItem(tr("Fixed string"), QRegExp::FixedString);
  filterSyntaxLabel->setBuddy(filterSyntaxComboBox);
  filterSyntaxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  filterColumnComboBox->addItem(tr("Stroke Order"));
  filterColumnComboBox->addItem(tr("Group Id"));
  filterColumnComboBox->addItem(tr("Id"));
  filterColumnComboBox->addItem(tr("StyleId"));
  filterColumnComboBox->addItem(tr("Self Loop"));
  filterColumnComboBox->addItem(tr("Quad"));
  filterColumnComboBox->addItem(tr("P"));
  filterColumnComboBox->addItem(tr("x"));
  filterColumnComboBox->addItem(tr("y"));
  filterColumnComboBox->addItem(tr("Thickness"));

  filterColumnLabel->setBuddy(filterColumnComboBox);
  filterColumnLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  setHeaderToolTips();

  connect(filterPatternLineEdit, &QLineEdit::textChanged, this,
          &VectorInspectorPanel::filterRegExpChanged);
  connect(filterSyntaxComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &VectorInspectorPanel::filterRegExpChanged);
  connect(filterColumnComboBox,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &VectorInspectorPanel::filterColumnChanged);

  connect(proxyView, &QTreeView::clicked, this,
          &VectorInspectorPanel::onSelectedStroke);

  connect(proxyView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &VectorInspectorPanel::onVectorInspectorSelectionChanged);

  proxyGroupBox->setContentsMargins(0, 0, 0, 0);

  QGridLayout* proxyLayout = new QGridLayout;

  // Set size policy for proxyView to be expanding
  proxyView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  // Create a QScrollArea and set proxyView as its widget
  QScrollArea* scrollArea = new QScrollArea;
  scrollArea->setWidget(proxyView);
  scrollArea->setWidgetResizable(true);

  // Add the scroll area to the layout instead of the proxyView directly
  proxyLayout->addWidget(scrollArea, 0, 0, 1, 3);

  proxyLayout->addWidget(filterPatternLabel, 1, 0, Qt::AlignRight);
  proxyLayout->addWidget(filterPatternLineEdit, 1, 1, 1, 2);
  proxyLayout->addWidget(filterSyntaxLabel, 2, 0, Qt::AlignRight);
  proxyLayout->addWidget(filterSyntaxComboBox, 2, 1, 1, 2);
  proxyLayout->addWidget(filterColumnLabel, 3, 0, Qt::AlignRight);
  proxyLayout->addWidget(filterColumnComboBox, 3, 1, 1, 2);
  proxyLayout->setContentsMargins(0, 0, 0, 0);

  // Set margins and spacing to zero
  proxyLayout->setContentsMargins(0, 0, 0, 0);
  proxyLayout->setSpacing(0);

  proxyGroupBox->setLayout(proxyLayout);

  // Add the scroll area to your main layout or widget
  QVBoxLayout* mainLayout = new QVBoxLayout;

  // Set margins and spacing to zero
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Add the proxyGroupBox to the main layout
  mainLayout->addWidget(proxyGroupBox, 1);

  setLayout(mainLayout);

  proxyView->sortByColumn(0, Qt::AscendingOrder);
  filterColumnComboBox->setCurrentIndex(
      6);  // position 6 for the 'P' column of the Vector Inspector QTreeView
  filterPatternLineEdit->setText(
      "2");  // set default filter here, like: "0|1" to filter on value 0 or 1
}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::setHeaderToolTips() {
  // Get the source model and set the tooltips
  if (sourceModel) {
    sourceModel->setHeaderData(0, Qt::Horizontal,
                               tr("Order of the stroke in the sequence"),
                               Qt::ToolTipRole);
    sourceModel->setHeaderData(1, Qt::Horizontal,
                               tr("Identifier for the group"), Qt::ToolTipRole);
    sourceModel->setHeaderData(2, Qt::Horizontal,
                               tr("Unique identifier for the stroke"),
                               Qt::ToolTipRole);
    sourceModel->setHeaderData(
        3, Qt::Horizontal, tr("The palette style index assigned to the stroke"),
        Qt::ToolTipRole);
    sourceModel->setHeaderData(4, Qt::Horizontal,
                               tr("Indicates if the stroke is a closed shape"),
                               Qt::ToolTipRole);
    sourceModel->setHeaderData(
        5, Qt::Horizontal,
        tr("Order of the quadratic equation in the sequence per stroke"),
        Qt::ToolTipRole);
    sourceModel->setHeaderData(
        6, Qt::Horizontal,
        tr("This point is parameter P0, P1, or P2 in the Quad equation"),
        Qt::ToolTipRole);
    sourceModel->setHeaderData(
        7, Qt::Horizontal, tr("X coordinate of the point"), Qt::ToolTipRole);
    sourceModel->setHeaderData(
        8, Qt::Horizontal, tr("Y coordinate of the point"), Qt::ToolTipRole);
    sourceModel->setHeaderData(9, Qt::Horizontal,
                               tr("Thickness of the stroke at this point"),
                               Qt::ToolTipRole);
  }
}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::onLevelSwitched() {
  std::string levelType = "";

  switch (TApp::instance()->getCurrentImageType()) {
  case TImage::MESH:
    levelType = "Mesh";
    break;
  case TImage::VECTOR:
    levelType = "Vector";
    break;
  case TImage::TOONZ_RASTER:
    levelType = "ToonzRaster";
    break;
  case TImage::RASTER:
    levelType = "Raster";
    break;
  default:
    levelType = "Unknown";
  }

  if (levelType == "Vector") {
    // ----------------------- the new stuff ----------------------------------

    // to prevent multiple connections, try to disconnect first in case
    // already connected previously.

    QObject::disconnect(TApp::instance()->getCurrentSelection(),
                        &TSelectionHandle::selectionChanged, this,
                        &VectorInspectorPanel::onSelectionChanged);
    QObject::connect(TApp::instance()->getCurrentSelection(),
                     &TSelectionHandle::selectionChanged, this,
                     &VectorInspectorPanel::onSelectionChanged);

    QObject::disconnect(TApp::instance()->getCurrentTool(),
                        &ToolHandle::toolSwitched, this,
                        &VectorInspectorPanel::onToolSwitched);
    QObject::connect(TApp::instance()->getCurrentTool(),
                     &ToolHandle::toolSwitched, this,
                     &VectorInspectorPanel::onToolSwitched);

    QObject::disconnect(TApp::instance()->getCurrentScene(),
                        &TSceneHandle::sceneSwitched, this,
                        &VectorInspectorPanel::onSceneChanged);
    QObject::connect(TApp::instance()->getCurrentScene(),
                     &TSceneHandle::sceneSwitched, this,
                     &VectorInspectorPanel::onSceneChanged);

    QObject::connect(TApp::instance()->getCurrentScene(),
                     &TSceneHandle::sceneChanged, this,
                     &VectorInspectorPanel::onSceneChanged);
    QObject::connect(TApp::instance()->getCurrentScene(),
                     &TSceneHandle::sceneChanged, this,
                     &VectorInspectorPanel::onSceneChanged);

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
      QObject::disconnect(m_vectorImage, &TVectorImage::changedStrokes, this,
                          &VectorInspectorPanel::onChangedStrokes);

      QObject::connect(m_vectorImage, &TVectorImage::changedStrokes, this,
                       &VectorInspectorPanel::onChangedStrokes);

      QObject::disconnect(m_vectorImage, &TVectorImage::enteredGroup, this,
                          &VectorInspectorPanel::onEnteredGroup);

      QObject::connect(m_vectorImage, &TVectorImage::enteredGroup, this,
                       &VectorInspectorPanel::onEnteredGroup);

      QObject::disconnect(m_vectorImage, &TVectorImage::exitedGroup, this,
                          &VectorInspectorPanel::onExitedGroup);

      QObject::connect(m_vectorImage, &TVectorImage::exitedGroup, this,
                       &VectorInspectorPanel::onExitedGroup);

      QObject::disconnect(m_vectorImage, &TVectorImage::changedStrokeOrder,
                          this, &VectorInspectorPanel::onStrokeOrderChanged);

      QObject::connect(m_vectorImage, &TVectorImage::changedStrokeOrder, this,
                       &VectorInspectorPanel::onStrokeOrderChanged);

      QObject::disconnect(m_vectorImage, &TVectorImage::selectedAllStrokes,
                          this, &VectorInspectorPanel::onSelectedAllStrokes);

      QObject::connect(m_vectorImage, &TVectorImage::selectedAllStrokes, this,
                       &VectorInspectorPanel::onSelectedAllStrokes);

      m_vectorImage->getStrokeListData(parentWidget(), sourceModel);
      setSourceModel(sourceModel);
      proxyModel->invalidate();

      // Autosizing columns after the model is populated
      for (int column = 1; column < proxyModel->columnCount(); ++column) {
        proxyView->resizeColumnToContents(column);
      }
      proxyView->setColumnWidth(0, 100);  // Stroke Order column width
    }

  } else {
    // disconnect all signals when the current level is not Vector
    QObject::disconnect(TApp::instance()->getCurrentSelection(),
                        &TSelectionHandle::selectionChanged, this,
                        &VectorInspectorPanel::onSelectionChanged);

    QObject::disconnect(TApp::instance()->getCurrentTool(),
                        &ToolHandle::toolSwitched, this,
                        &VectorInspectorPanel::onToolSwitched);

    QObject::disconnect(TApp::instance()->getCurrentScene(),
                        &TSceneHandle::sceneSwitched, this,
                        &VectorInspectorPanel::onSceneChanged);

    QObject::disconnect(TApp::instance()->getCurrentScene(),
                        &TSceneHandle::sceneChanged, this,
                        &VectorInspectorPanel::onSceneChanged);

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
  disconnect(proxyView->selectionModel(),
             &QItemSelectionModel::selectionChanged, this,
             &VectorInspectorPanel::onVectorInspectorSelectionChanged);
  QItemSelectionModel* selectionModel = proxyView->selectionModel();

  // Clear the previous selection
  selectionModel->clearSelection();
  proxyModel->invalidate();

  m_selectedStrokeIndexes = {};

  QModelIndex index =
      proxyModel->index(0, 0);  // the first column of the first row

  ToolHandle* currentTool = TApp::instance()->getCurrentTool();

  if ("T_Selection" == currentTool->getTool()->getName()) {
    StrokeSelection* strokeSelection =
        dynamic_cast<StrokeSelection*>(currentTool->getTool()->getSelection());

    if (strokeSelection) {
      if (strokeSelection) {
        // initialize the lookup value list.
        for (int i = 0; i < (int)proxyModel->rowCount(); i++) {
          if (strokeSelection->isSelected(i)) {
            index = proxyModel->index(i, 0);
            m_selectedStrokeIndexes.push_back(i);
          }
        }
        // now if selected rows then set them in the QTreeView
        if (m_selectedStrokeIndexes.size() > 0) {
          for (int i = 0; i < (int)proxyModel->rowCount(); i++) {
            if (isSelected(proxyModel->index(i, 0).data().toInt())) {
              index = proxyModel->index(i, 0);
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
      proxyView->scrollTo(firstSelectedIndex, QAbstractItemView::PositionAtTop);
    }
  }
  connect(proxyView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &VectorInspectorPanel::onVectorInspectorSelectionChanged);
}

//-----------------------------------------------------------------------------

bool VectorInspectorPanel::isSelected(int index) const {
  return (std::find(m_selectedStrokeIndexes.begin(),
                    m_selectedStrokeIndexes.end(),
                    index) != m_selectedStrokeIndexes.end());
}

//-----------------------------------------------------------------------------

void VectorInspectorPanel::onSelectionChanged() {
  if (initiatedByVectorInspector) {
  } else {
    initiatedBySelectTool = true;

    setRowHighlighting();

    initiatedBySelectTool = false;
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
}

void VectorInspectorPanel::setSourceModel(QAbstractItemModel* model) {
  proxyModel->setSourceModel(model);
}

void VectorInspectorPanel::filterRegExpChanged() {
  QRegExp::PatternSyntax syntax = QRegExp::PatternSyntax(
      filterSyntaxComboBox->itemData(filterSyntaxComboBox->currentIndex())
          .toInt());

  QRegExp regExp(filterPatternLineEdit->text(), Qt::CaseInsensitive, syntax);
  proxyModel->setFilterRegExp(regExp);
  setRowHighlighting();
}

void VectorInspectorPanel::filterColumnChanged() {
  proxyModel->setFilterKeyColumn(filterColumnComboBox->currentIndex());
}

void VectorInspectorPanel::sortChanged() {}

void VectorInspectorPanel::copySelectedItemsToClipboard() {
  QModelIndexList indexes = proxyView->selectionModel()->selectedIndexes();

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
  QPoint globalPos = proxyView->viewport()->mapToGlobal(pos);
  QMenu menu;
  QAction* copyAction = menu.addAction(tr("Copy"));
  connect(copyAction, &QAction::triggered, this,
          &VectorInspectorPanel::copySelectedItemsToClipboard);

  menu.exec(globalPos);
}

void VectorInspectorPanel::onEnteredGroup() {
  m_vectorImage->getStrokeListData(parentWidget(), sourceModel);
  setSourceModel(sourceModel);
  proxyModel->invalidate();
  proxyView->selectionModel()->clearSelection();
}

void VectorInspectorPanel::onExitedGroup() {
  m_vectorImage->getStrokeListData(parentWidget(), sourceModel);
  setSourceModel(sourceModel);
  setRowHighlighting();
}

void VectorInspectorPanel::onChangedStrokes() {
  m_vectorImage->getStrokeListData(parentWidget(), sourceModel);
  setSourceModel(sourceModel);
  setRowHighlighting();
}

void VectorInspectorPanel::onToolEditingFinished() {}

void VectorInspectorPanel::onSceneChanged() {}

void VectorInspectorPanel::onStrokeOrderChanged(int fromIndex, int count,
                                                int moveBefore, bool regroup) {
  strokeOrderChangedInProgress = true;
  bool isSortOrderAscending    = true;

  ToolHandle* currentTool = TApp::instance()->getCurrentTool();
  StrokeSelection* strokeSelection =
      dynamic_cast<StrokeSelection*>(currentTool->getTool()->getSelection());

  strokeSelection->notifyView();

  m_vectorImage->getStrokeListData(parentWidget(), proxyModel);
  setSourceModel(proxyModel);

  std::vector<int> theIndexes = strokeSelection->getSelection();

  QItemSelectionModel* selectionModel = proxyView->selectionModel();

  selectionModel->clearSelection();

  Qt::SortOrder sortOrder = proxyView->header()->sortIndicatorOrder();

  if (sortOrder == Qt::AscendingOrder) {
    isSortOrderAscending = true;
  } else if (sortOrder == Qt::DescendingOrder) {
    isSortOrderAscending = false;
  }

  QModelIndex index =
      proxyModel->index(0, 0);  // the first column of the first row

  int moveAmount = (fromIndex < moveBefore) ? moveBefore - 1 - fromIndex
                                            : moveBefore - fromIndex;

  int maxIndexValue   = proxyModel->rowCount() - 1;  // theIndexes.size() - 1;
  int minIndexValue   = 0;
  int currentRowIndex = 0;

  // get the maximum stroke index value, assume sorted values, no gaps
  int lastRow            = proxyModel->rowCount() - 1;
  QModelIndex firstIndex = proxyModel->index(0, 0);
  QModelIndex lastIndex  = proxyModel->index(lastRow, 0);
  int maxStrokeIndex     = std::max(proxyModel->data(firstIndex).toInt(),
                                proxyModel->data(lastIndex).toInt());

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
  for (int i = 0; i < (int)proxyModel->rowCount(); i++) {
    if (isSelected(proxyModel->index(i, 0).data().toInt())) {
      index = proxyModel->index(i, 0);
      selectionModel->select(
          index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
  }

  // scroll to show selected rows
  QModelIndexList selectedIndexes = selectionModel->selectedIndexes();

  if (!selectedIndexes.isEmpty()) {
    QModelIndex firstSelectedIndex = selectedIndexes.first();
    proxyView->scrollTo(firstSelectedIndex, QAbstractItemView::PositionAtTop);
  }

  strokeOrderChangedInProgress = false;
}

void VectorInspectorPanel::onToolSwitched() {
  QObject::disconnect(TApp::instance()->getCurrentTool(),
                      &ToolHandle::toolEditingFinished, this,
                      &VectorInspectorPanel::onToolEditingFinished);

  QObject::connect(TApp::instance()->getCurrentTool(),
                   &ToolHandle::toolEditingFinished, this,
                   &VectorInspectorPanel::onToolEditingFinished);

  setRowHighlighting();
}

void VectorInspectorPanel::onSelectedAllStrokes() { setRowHighlighting(); }

void VectorInspectorPanel::onSelectedStroke(const QModelIndex& index) {
  selectingRowsForStroke = true;
  setRowHighlighting();
  selectingRowsForStroke = false;
}

void VectorInspectorPanel::onVectorInspectorSelectionChanged(
    const QItemSelection& selected, const QItemSelection& deselected) {
  // Handle the selection changed event
  if (strokeOrderChangedInProgress) {
  } else if (initiatedBySelectTool) {
  } else if (selectingRowsForStroke) {
  } else {
    initiatedByVectorInspector = true;

    updateSelectToolSelectedRows(selected, deselected);

    initiatedByVectorInspector = false;
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
    disconnect(proxyView, &QTreeView::clicked, this,
               &VectorInspectorPanel::onSelectedStroke);

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
    connect(proxyView, &QTreeView::clicked, this,
            &VectorInspectorPanel::onSelectedStroke);
  }
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
