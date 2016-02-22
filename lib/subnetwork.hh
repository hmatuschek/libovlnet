#ifndef SUBNETWORK_HH
#define SUBNETWORK_HH

#include <QObject>
#include "crypto.hh"
#include "buckets.hh"

class Node;
class SubNetFindNodeService;

/** Base class of all subnetworks. A subnetwork spans a overlay network on a subset of the complete
 * OVL (rrot) network providing specialized services. To join a subnetwork you only need to
 * search for the neightbours of yourself in a node being part of the subnetwork.
 * @ingroup subnet */
class SubNetwork : public QObject
{
  Q_OBJECT

public:
  explicit SubNetwork(Node &node, const QString &prefix, QObject *parent = 0);

  inline Node &node() { return _node; }
  inline const Node &node() const { return _node; }
  inline const QString &prefix() const { return _prefix; }

  void bootstrap(const Identifier &id);
  void findNode(const Identifier &id);
  void findNeighbours(const Identifier &id);

  inline void nodesNear(const Identifier &id, QList<NodeItem> &nodes) {
    _buckets.getNearest(id, nodes);
  }

signals:
  void connected();
  void disconnected();
  void nodeFound(const NodeItem &node);
  void nodeNotFound(const Identifier &id, const QList<NodeItem> &best);
  void neighboursFound(const Identifier &id, const QList<NodeItem> &neighbours);

protected:
  void addCandidate(const NodeItem &node);
  void nodeSearchSucceeded(const NodeItem &node);
  void nodeSearchFailed(const Identifier &nodeid, const QList<NodeItem> &best);
  // Allows internal SubNetFindNodeQuery to access these methods.
  friend class SubNetFindNodeQuery;

protected:
  Node &_node;
  QString _prefix;
  Buckets _buckets;

  SubNetFindNodeService *_findNodeService;
  // Allow root netork node to access the subnet buckets
  friend class Node;
};


#endif // SUBNETWORK_HH
