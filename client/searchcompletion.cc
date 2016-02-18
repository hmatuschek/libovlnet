#include "searchcompletion.hh"

SearchCompletionModel::SearchCompletionModel(Node &dht, QObject *parent)
  : QAbstractListModel(parent), _dht(dht), _results()
{
  connect(&_dht, SIGNAL(nodeNotFound(Identifier,QList<NodeItem>)),
          this, SLOT(_onNodeNotFound(Identifier,QList<NodeItem>)));
}

void
SearchCompletionModel::_onNodeNotFound(const Identifier &id, const QList<NodeItem> &best) {
  _results = best;
}

int
SearchCompletionModel::rowCount(const QModelIndex &parent) const {
  return _results.size();
}



