#ifndef SEARCHCOMPLETION_H
#define SEARCHCOMPLETION_H

#include <QAbstractListModel>
#include "lib/dht.hh"


class SearchCompletionModel : public QAbstractListModel
{
  Q_OBJECT

public:
  explicit SearchCompletionModel(DHT &dht, QObject *parent = 0);

  int rowCount(const QModelIndex &parent) const;

protected slots:
  void _onNodeNotFound(const Identifier &id, const QList<NodeItem> &best);

protected:
  /** Weak reference to the DHT instance (for searching). */
  DHT &_dht;
  /** Last search results. */
  QList<NodeItem> _results;
};

#endif // SEARCHCOMPLETION_H
