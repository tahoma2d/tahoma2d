#include <multicolumnsortproxymodel.h>

MultiColumnSortProxyModel::MultiColumnSortProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

bool MultiColumnSortProxyModel::lessThan(const QModelIndex &left,
                                         const QModelIndex &right) const {
  int sortColumn = left.column();

  int primaryColumn   = 0;  // Stroke column on the VectorInspectorPanel
  int secondaryColumn = 5;  // Quad column on the VectorInspectorPanel
  int tertiaryColumn  = 6;  // P column on the VectorInspectorPanel

  QModelIndex leftIndex  = left.sibling(left.row(), sortColumn);
  QModelIndex rightIndex = right.sibling(right.row(), sortColumn);

  QString leftValue  = sourceModel()->data(leftIndex).toString();
  QString rightValue = sourceModel()->data(rightIndex).toString();

  if (leftValue != rightValue) {
    if (sortColumn == 1) {  // Group Id
      QStringList leftList = leftValue.split(".");
      QStringList rightList = rightValue.split(".");

      int min = std::min(leftList.count(), rightList.count());
      for (int i = 0; i < min; i++) {
        QString l = leftList[i];
        QString r = rightList[i];
        l.remove("(");
        l.remove(")");
        r.remove("(");
        r.remove(")");
        if (l.toInt() != r.toInt()) return l.toInt() < r.toInt();
      }

      if (leftList.count() != rightList.count())
        return leftList.count() < rightList.count();  // Left has less groups

    } else if (sortColumn == 4) // Self Loop
      return leftValue < rightValue;

    return leftValue.toInt() < rightValue.toInt();
  }

  // A difference in the first column values?
  if (sortColumn != primaryColumn) {
    leftIndex  = left.sibling(left.row(), primaryColumn);
    rightIndex = right.sibling(right.row(), primaryColumn);

    leftValue  = sourceModel()->data(leftIndex).toString();
    rightValue = sourceModel()->data(rightIndex).toString();

    if (leftValue != rightValue) return leftValue.toInt() < rightValue.toInt();
  }

  // No difference, check the secondary column.
  if (sortColumn != primaryColumn) {
    leftIndex  = left.sibling(left.row(), secondaryColumn);
    rightIndex = right.sibling(right.row(), secondaryColumn);

    leftValue  = sourceModel()->data(leftIndex).toString();
    rightValue = sourceModel()->data(rightIndex).toString();

    if (leftValue != rightValue) return leftValue.toInt() < rightValue.toInt();
  }

  // No difference, check the tertiary column.
  if (sortColumn != primaryColumn) {
    leftIndex  = left.sibling(left.row(), tertiaryColumn);
    rightIndex = right.sibling(right.row(), tertiaryColumn);

    leftValue  = sourceModel()->data(leftIndex).toString();
    rightValue = sourceModel()->data(rightIndex).toString();

    if (leftValue != rightValue) return leftValue.toInt() < rightValue.toInt();
  }

  return leftValue < rightValue;
}