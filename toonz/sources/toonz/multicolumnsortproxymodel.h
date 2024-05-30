#ifndef MULTICOLUMNSORTPROXYMODEL_H
#define MULTICOLUMNSORTPROXYMODEL_H

#include <QSortFilterProxyModel>

class MultiColumnSortProxyModel : public QSortFilterProxyModel {
public:
  MultiColumnSortProxyModel(QObject *parent = nullptr);

protected:
  bool lessThan(const QModelIndex &left,
                const QModelIndex &right) const override;
};

#endif  // MULTICOLUMNSORTPROXYMODEL_H
