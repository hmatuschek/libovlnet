#include "network.hh"
#include "node.hh"

#define NET_NODE_REFRESH_INTERVAL (15*60)
#define NET_NODE_TIMEOUT          (20*60)


/* ******************************************************************************************** *
 * Implementation of SearchQuery
 * ******************************************************************************************** */
SearchQuery::SearchQuery(const Identifier &id, const QString &prefix)
  : QObject(), _id(id), _prefix(), _best(), _queried()
{
  char hash[OVL_HASH_SIZE];
  QByteArray prefName = prefix.toUtf8();
  OVLHash((const uint8_t *)prefName.constData(), prefName.size(), (uint8_t *)hash);
  _prefix = Identifier(hash);
}

SearchQuery::~SearchQuery() {
  // pass...
}

void
SearchQuery::ignore(const Identifier &id) {
  _queried.insert(id);
}

const Identifier &
SearchQuery::id() const {
  return _id;
}

const Identifier &
SearchQuery::netid() const {
  return _prefix;
}

void
SearchQuery::update(const NodeItem &node) {
  // Skip nodes already queried or in the best list -> done
  if (_queried.contains(node.id())) { return; }
  // Perform an "insort" into best list
  Distance d = _id-node.id();
  QList<NodeItem>::iterator item = _best.begin();
  while ((item != _best.end()) && (d>=(_id-item->id()))) {
    // if the node is in list -> quit
    if (item->id() == node.id()) { return; }
    // continue
    item++;
  }
  _best.insert(item, node);
  while (_best.size() > OVL_K) { _best.pop_back(); }
}

bool
SearchQuery::next(NodeItem &node) {
  QList<NodeItem>::iterator item = _best.begin();
  for (; item != _best.end(); item++) {
    if (! _queried.contains(item->id())) {
      _queried.insert(item->id());
      node = *item;
      return true;
    }
  }
  return false;
}

QList<NodeItem> &
SearchQuery::best() {
  return _best;
}

const QList<NodeItem> &
SearchQuery::best() const {
  return _best;
}

const NodeItem &
SearchQuery::first() const {
  return _best.first();
}

void
SearchQuery::searchFailed() {
  emit failed(_id, _best);
  this->deleteLater();
}

void
SearchQuery::searchSucceeded() {
  emit succeeded(_id, _best);
  this->deleteLater();
}


/* ******************************************************************************************** *
 * Implementation of FindNodeQuery
 * ******************************************************************************************** */
FindNodeQuery::FindNodeQuery(const Identifier &id, const QString prefix)
  : SearchQuery(id, prefix)
{
  // pass...
}

FindNodeQuery::~FindNodeQuery() {
  // pass...
}

bool
FindNodeQuery::isSearchComplete() const {
  foreach (NodeItem node, _best) {
    if (node.id() == _id)
      return true;
  }
  return false;
}

void
FindNodeQuery::searchCompleted() {
  foreach (NodeItem node, _best) {
    if (node.id() == _id) {
      this->searchSucceeded();
      return;
    }
  }
  this->searchFailed();
}

void
FindNodeQuery::searchSucceeded() {
  emit found(_best.first());
  SearchQuery::searchSucceeded();
}


/* ******************************************************************************************** *
 * Implementation of FindNeighbourhoodQuery
 * ******************************************************************************************** */
NeighbourhoodQuery::NeighbourhoodQuery(const Identifier &id, const QString prefix)
  : SearchQuery(id, prefix)
{
  // pass...
}

NeighbourhoodQuery::~NeighbourhoodQuery() {
  // pass...
}

bool
NeighbourhoodQuery::isSearchComplete() const {
  return false;
}

void
NeighbourhoodQuery::searchCompleted() {
  emit completed(_id, _best);
  if (_best.size())
    this->searchSucceeded();
  else
    this->searchFailed();
}


/* ******************************************************************************************** *
 * Implementation of Network
 * ******************************************************************************************** */
Network::Network(const Identifier &id, QObject *parent)
  : QObject(parent), _buckets(id), _nodeTimer()
{
  // check for dead nodes every minute
  _nodeTimer.setInterval(1000*60);
  _nodeTimer.setSingleShot(false);
  _nodeTimer.start();

  connect(&_nodeTimer, SIGNAL(timeout()), this, SLOT(checkNodes()));
}

Identifier
Network::netid() const {
  char hash[20];
  QByteArray prefix = this->prefix().toUtf8();
  OVLHash((const uint8_t *)prefix.constData(), prefix.size(), (uint8_t *)hash);
  return Identifier(hash);
}

void
Network::getNearest(const Identifier &id, QList<NodeItem> &nodes) const {
  _buckets.getNearest(id, nodes);
}

void
Network::addCandidate(const NodeItem &node) {
  if (! _buckets.contains(node.id())) {
    _buckets.addCandidate(node.id(), node.addr(), node.port());
  }
}

void
Network::nodeReachableEvent(const NodeItem &node) {
  // signal success
  emit nodeReachable(node);
  // If the buckets are empty -> we are likely bootstrapping
  bool bootstrapping = _buckets.empty();
  // Given that this is a ping response -> add the node to the corresponding
  // bucket if space is left
  if (_buckets.add(node.id(), node.addr(), node.port())) {
    emit nodeAppeard(node);
  }
  if (bootstrapping) {
    emit connected();
    logDebug() << "Still boot strapping: Search for myself.";
    search(new NeighbourhoodQuery(_buckets.id(), prefix()));
  }
}

void
Network::checkNodes() {
  // Collect nodes older than 15min from the buckets
  QList<NodeItem> oldNodes;
  _buckets.getOlderThan(NET_NODE_REFRESH_INTERVAL, oldNodes);
  // send a ping to all of them
  QList<NodeItem>::iterator node = oldNodes.begin();
  for (; node != oldNodes.end(); node++) {
    logDebug() << "Node " << node->id() << " needs update -> ping.";
    this->ping(*node);
  }

  // Get disappeared nodes
  oldNodes.clear(); _buckets.getOlderThan(NET_NODE_TIMEOUT, oldNodes);
  for (node = oldNodes.begin(); node != oldNodes.end(); node++) {
    emit nodeLost(node->id());
  }

  // are buckets non-empty (this node is connected to the network)
  bool connected = (0 != _buckets.numNodes());

  // Remove dead nodes from the buckets
  _buckets.removeOlderThan(NET_NODE_TIMEOUT);

  // check if the last node was removed
  if (connected && (0 == _buckets.numNodes())) {
    // If the last node was removed -> signal connection loss
    emit disconnected();
  }

  // Update neighbourhood
  if (_buckets.numNodes()) {
    // search for myself, this will certainly fail but results in a list
    // of the closest nodes, which will be added to the buckets as candidates
    search(new NeighbourhoodQuery(root().id()));
  }
}
