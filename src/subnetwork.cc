#include "subnetwork.hh"
#include <QJsonDocument>
#include <QJsonArray>
#include "httpclient.hh"


/* ********************************************************************************************** *
 * Implementation of SubNetwork
 * ********************************************************************************************** */
SubNetwork::SubNetwork(Node &node, const QString &prefix, QObject *parent)
  : Network(node.id(), parent), _node(node), _prefix(prefix)
{
  // Get notified about every successful pings in the base network
  // this allows to keep the sub net bucket upto date and to discover new members
  QObject::connect(&_node, SIGNAL(nodeReachable(NodeItem)), this, SLOT(addCandidate(NodeItem)));
}

SubNetwork::~SubNetwork() {
  // pass...
}

const QString &
SubNetwork::prefix() const {
  return _prefix;
}

Node &
SubNetwork::root() {
  return _node;
}

bool
SubNetwork::hasService(const QString &name) const {
  return _node.hasService(_prefix+"::"+name);
}

bool
SubNetwork::registerService(const QString &service, AbstractService *handler) {
  return _node.registerService(_prefix+"::"+service, handler);
}

void
SubNetwork::ping(const NodeItem &node) {
  _node.sendPing(node.id(), node.addr(), node.port(), this->netid());
}

void
SubNetwork::search(SearchQuery *query) {
  QList<NodeItem> nodes;
  _buckets.getNearest(query->id(), nodes);
  foreach (NodeItem item, nodes) {
    query->update(item);
  }
  // Check if search can be performed
  if (0 == query->best().size()) {
    logError() << "Cannot search for " << query->id() << ": Bucket are empty.";
    query->searchCompleted();
    return;
  }
  _node.sendSearch(query->best().first(), query);
}

void
SubNetwork::_updateBuckets() {
  // Ping old nodes
  QList<NodeItem> old_nodes;
  _buckets.getOlderThan(15*60, old_nodes);
  foreach (NodeItem node, old_nodes) {
    ping(node);
  }

  bool connected = (0 != _buckets.numNodes());
  // Remove all dead nodes
  _buckets.removeOlderThan(30*60);
  // if the last node left the buckets -> signal disconnect
  if (connected && (0 == _buckets.numNodes())) {
    emit this->disconnected();
  }
}

void
SubNetwork::_updateNeighbours() {
  // simply start a search for the neighbourhood of myself
  search(new NeighbourhoodQuery(_node.id()));
}
