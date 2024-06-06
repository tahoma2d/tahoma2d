#include <multicolumnsortproxymodel.h>

MultiColumnSortProxyModel::MultiColumnSortProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {}

bool MultiColumnSortProxyModel::lessThan(const QModelIndex &left,
                                         const QModelIndex &right) const {
  int primaryColumn   = 0;  // Stroke column on the VectorInspectorPanel
  int secondaryColumn = 5;  // Quad column on the VectorInspectorPanel
  int tertiaryColumn  = 6;  // P column on the VectorInspectorPanel

  // A difference in the first column values?
  QModelIndex leftPrimaryIndex  = left.sibling(left.row(), primaryColumn);
  QModelIndex rightPrimaryIndex = right.sibling(right.row(), primaryColumn);

  QString leftPrimary  = sourceModel()->data(leftPrimaryIndex).toString();
  QString rightPrimary = sourceModel()->data(rightPrimaryIndex).toString();

  if (leftPrimary != rightPrimary) return leftPrimary < rightPrimary;

  // No difference, check the secondary column.
  QModelIndex leftSecondaryIndex  = left.sibling(left.row(), secondaryColumn);
  QModelIndex rightSecondaryIndex = right.sibling(right.row(), secondaryColumn);

  QString leftSecondary  = sourceModel()->data(leftSecondaryIndex).toString();
  QString rightSecondary = sourceModel()->data(rightSecondaryIndex).toString();

  // sort as numbers because of double digit values
  if (leftSecondary != rightSecondary)
    return leftSecondary.toInt() < rightSecondary.toInt();

  // No difference, check the tertiary column.
  QModelIndex leftTertiaryIndex  = left.sibling(left.row(), tertiaryColumn);
  QModelIndex rightTertiaryIndex = right.sibling(right.row(), tertiaryColumn);

  QString leftTertiary  = sourceModel()->data(leftTertiaryIndex).toString();
  QString rightTertiary = sourceModel()->data(rightTertiaryIndex).toString();

  return leftTertiary < rightTertiary;
}