#ifndef SUBNETWORK_HH
#define SUBNETWORK_HH

#include "buckets.hh"
#include "network.hh"
#include "node.hh"


class SubNetwork: public Network
{
  Q_OBJECT

public:
  SubNetwork(Node &node, const QString &prefix, QObject *parent=0);
  virtual ~SubNetwork();

  const QString &prefix() const;
  Node & root();

  bool hasService(const QString &name) const;
  bool registerService(const QString& service, AbstractService *handler);

  void ping(const NodeItem &node);

  void search(SearchQuery *query);

protected slots:
  void _updateBuckets();
  void _updateNeighbours();

protected:
  Node &_node;
  QString _prefix;
};


#endif // SUBNETWORK_HH
