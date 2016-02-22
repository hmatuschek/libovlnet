#include "subnetwork.hh"
#include "node.hh"


/* ******************************************************************************************** *
 * Declaration of SubNetFindNodeQuery
 * ******************************************************************************************** */
class SubNetFindNodeQuery
{
public:
  SubNetFindNodeQuery(const Identifier &id, const NodeItem &boot, SubNetwork &subnet);
  SubNetFindNodeQuery(const Identifier &id, SubNetwork &subnet);

  inline SubNetwork &subnet() { return _subnet; }
  inline const Identifier &id() const { return _query; }

  void candidatesReceived(const QList<NodeItem> &candidates);
  void requestFailed();

protected:
  void update(const NodeItem &node);
  bool next(NodeItem &node);

protected:
  Identifier _query;
  SubNetwork &_subnet;
  QList<NodeItem> _best;
  QSet<Identifier> _done;
};


/* ******************************************************************************************** *
 * Declaration of SubNetFindNodeOutRequest
 * ******************************************************************************************** */
class SubNetFindNodeOutRequest: public SecureSocket
{
public:
  SubNetFindNodeOutRequest(SubNetFindNodeQuery &query);

protected:
  bool start(const Identifier &streamId, const PeerItem &peer);
  void handleDatagram(const uint8_t *data, size_t len);
  void failed();

protected:
  SubNetFindNodeQuery &_query;
};


/* ******************************************************************************************** *
 * Implementation of SubNetFindNodeOutRequest
 * ******************************************************************************************** */
SubNetFindNodeOutRequest::SubNetFindNodeOutRequest(SubNetFindNodeQuery &query)
  : SecureSocket(query.subnet().node()), _query(query)
{
  // pass...
}

bool
SubNetFindNodeOutRequest::start(const Identifier &streamId, const PeerItem &peer) {
  if (! SecureSocket::start(streamId, peer)) {
    return false;
  }
  // Connection started send query
  if (! sendDatagram((const uint8_t *)_query.id().constData(), OVL_HASH_SIZE)) {
    return false;
  }
  return true;
}

void
SubNetFindNodeOutRequest::handleDatagram(const uint8_t *data, size_t len) {
  size_t nNodes = len/OVL_TRIPLE_SIZE;
  QList<NodeItem> nodes;
  for (size_t i=0; i<nNodes; i++, data+=OVL_TRIPLE_SIZE) {
    Identifier id((const char *)data);
    QHostAddress addr(data+OVL_HASH_SIZE);
    uint16_t port = ntohs(*((const uint16_t *)(data+OVL_HASH_SIZE+16)));
    nodes.push_back(NodeItem(id, addr, port));
  }
  _query.candidatesReceived(nodes);
  _query.subnet().node().socketClosed(id());
}

void
SubNetFindNodeOutRequest::failed() {
  _query.requestFailed();
}


/* ******************************************************************************************** *
 * Implementation of SubNetFindNodeQuery
 * Represents an ongoing search for a node in the subnetwork
 * ******************************************************************************************** */
SubNetFindNodeQuery::SubNetFindNodeQuery(const Identifier &id, const NodeItem &boot, SubNetwork &subnet)
  : _query(id), _subnet(subnet)
{
  // First: add my ID to _done
  _done.insert(_subnet.node().id());

  // Add first guess
  _best.append(boot);

  // query first node
  NodeItem nextNode;
  if (! next(nextNode)) {
    subnet.nodeSearchFailed(_query, _best);
    return;
  }
  SubNetFindNodeOutRequest *request = new SubNetFindNodeOutRequest(*this);
  _subnet.node().startConnection(_subnet.prefix()+"::findnode", nextNode, request);
}


SubNetFindNodeQuery::SubNetFindNodeQuery(const Identifier &id, SubNetwork &subnet)
  : _query(id), _subnet(subnet)
{
  // First: add my ID to _done
  _done.insert(_subnet.node().id());

  // Get first guess from buckets
  _subnet.nodesNear(_query, _best);

  // query first node
  NodeItem nextNode;
  if (! next(nextNode)) {
    subnet.nodeSearchFailed(_query, _best);
    return;
  }
  SubNetFindNodeOutRequest *request = new SubNetFindNodeOutRequest(*this);
  _subnet.node().startConnection(_subnet.prefix() + "::findnode", nextNode, request);
}

void
SubNetFindNodeQuery::candidatesReceived(const QList<NodeItem> &candidates) {
  QList<NodeItem>::const_iterator node = candidates.begin();
  for (; node != candidates.end(); node++) {
    _subnet.addCandidate(*node);
    if (_query == node->id()) {
      _subnet.nodeSearchSucceeded(*node);
      return;
    }
    update(*node);
  }

  // query first node
  NodeItem nextNode;
  if (! next(nextNode)) {
    _subnet.nodeSearchFailed(_query, _best);
    return;
  }
  SubNetFindNodeOutRequest *request = new SubNetFindNodeOutRequest(*this);
  _subnet.node().startConnection(_subnet.prefix() + "::findnode", nextNode, request);
}

void
SubNetFindNodeQuery::requestFailed() {
  // query first node
  NodeItem nextNode;
  if (! next(nextNode)) {
    _subnet.nodeSearchFailed(_query, _best);
    return;
  }
  SubNetFindNodeOutRequest *request = new SubNetFindNodeOutRequest(*this);
  _subnet.node().startConnection(_subnet.prefix() + "::findnode", nextNode, request);
}

bool
SubNetFindNodeQuery::next(NodeItem &node) {
  if (0 == _best.size()) { return false; }
  QList<NodeItem>::iterator item = _best.begin();
  for (; item != _best.end(); item++) {
    if (! _done.contains(item->id())) {
      _done.insert(node.id());
      node = *item;
      return true;
    }
  }
  return false;
}

void
SubNetFindNodeQuery::update(const NodeItem &node) {
  if (_done.contains(node.id())) { return; }
  Distance d = _query-node.id();
  // Insort node into queue
  QList<NodeItem>::iterator item = _best.begin();
  while ((item != _best.end()) && (d>=(_query-item->id()))) {
    // if the node is in list -> quit
    if (item->id() == node.id()) { return; }
    // continue
    item++;
  }
  _best.insert(item, node);
  // Keep only the K best
  while (_best.size() > OVL_K) { _best.pop_back(); }
}


/* ******************************************************************************************** *
 * Implementation of SubNetFindNodeInRequest
 * ******************************************************************************************** */
class SubNetFindNodeInRequest: public SecureSocket
{
public:
  SubNetFindNodeInRequest(SubNetwork &subnet);

protected:
  void handleDatagram(const uint8_t *data, size_t len);

protected:
  SubNetwork &_subnet;
};

SubNetFindNodeInRequest::SubNetFindNodeInRequest(SubNetwork &subnet)
  : SecureSocket(subnet.node()), _subnet(subnet)
{
  // pass...
}

void
SubNetFindNodeInRequest::handleDatagram(const uint8_t *data, size_t len) {
  if (len != OVL_HASH_SIZE) {
    _subnet.node().socketClosed(id());
  }

  // Get nodes from buckets
  Identifier nodeId((const char *)data);
  QList<NodeItem> nodes; _subnet.nodesNear(nodeId, nodes);
  // Determine howmay nodes to return
  size_t nNodes = std::min(std::min(nodes.size(), OVL_K),
                           OVL_SEC_MAX_DATA_SIZE/OVL_TRIPLE_SIZE);
  // Assemble response
  uint8_t resp[OVL_SEC_MAX_DATA_SIZE];
  QList<NodeItem>::iterator node = nodes.begin();
  for (size_t i=0; i<nNodes; i++, node++) {
    memcpy(resp+i*OVL_TRIPLE_SIZE, node->id().constData(), OVL_HASH_SIZE);
    memcpy(resp+i*OVL_TRIPLE_SIZE+OVL_HASH_SIZE, node->addr().toIPv6Address().c, 16);
    *((uint16_t *)(resp+i*OVL_TRIPLE_SIZE+OVL_HASH_SIZE+16)) = htons(node->port());
  }
  sendDatagram(resp, nNodes*OVL_TRIPLE_SIZE);
  _subnet.node().socketClosed(id());
}


/* ******************************************************************************************** *
 * Implementation of SubNetFindNodeService
 * ******************************************************************************************** */
class SubNetFindNodeService: public AbstractService
{
public:
  SubNetFindNodeService(SubNetwork &subnet);

  SecureSocket *newSocket();
  bool allowConnection(const NodeItem &peer);
  void connectionStarted(SecureSocket *stream);
  void connectionFailed(SecureSocket *stream);

protected:
  SubNetwork &_subnet;
};

SubNetFindNodeService::SubNetFindNodeService(SubNetwork &subnet)
  : AbstractService(), _subnet(subnet)
{
  // done...
}

SecureSocket *
SubNetFindNodeService::newSocket() {
  return new SubNetFindNodeInRequest(_subnet);
}

bool
SubNetFindNodeService::allowConnection(const NodeItem &peer) {
  return true;
}

void
SubNetFindNodeService::connectionStarted(SecureSocket *stream) {
  // pass...
}

void
SubNetFindNodeService::connectionFailed(SecureSocket *stream) {
  delete stream;
}

/* ******************************************************************************************** *
 * Implementation of SubNetwork
 * ******************************************************************************************** */
SubNetwork::SubNetwork(Node &node, const QString &prefix, QObject *parent)
  : QObject(parent), _node(node), _prefix(prefix), _buckets(_node.id())
{
  _findNodeService = new SubNetFindNodeService(*this);
  _node.registerService(prefix+"::findnode", _findNodeService);
  //connect(&_node, SIGNAL(nodeFound(NodeItem)), this, SLOT(_onRootNodeFound(NodeItem)));
}

void
SubNetwork::bootstrap(const Identifier &id) {
  /// @bug Implement
}

void
SubNetwork::addCandidate(const NodeItem &node) {
  _buckets.addCandidate(node.id(), node.addr(), node.port());
}

void
SubNetwork::nodeSearchFailed(const Identifier &nodeid, const QList<NodeItem> &best) {
  emit nodeNotFound(nodeid, best);
}

void
SubNetwork::nodeSearchSucceeded(const NodeItem &node) {
  emit nodeFound(node);
}

