#include "dht.hh"
#include <QJsonDocument>
#include <QJsonArray>
#include "httpclient.hh"


/* ********************************************************************************************** *
 * Implementation of DHTService
 * ********************************************************************************************** */
DHT::DHT(SubNetwork &subnet, QObject *parent)
  : QObject(parent), _subnet(subnet)
{
  // register data search service
  _subnet.registerService("dht", new HttpService(_subnet, new DHTSearchHandler(*this), this));

  // Whenever the neighbourhood of some ID has been determined
  /// @bug nearestMembers is not a signal!
  connect(&_subnet, SIGNAL(nearestMembers(Identifier,QList<NodeItem>)),
          this, SLOT(membersFoundEvent(Identifier,QList<NodeItem>)));
}

SubNetwork &
DHT::subnet() {
  return _subnet;
}

bool
DHT::isResponsible(const Identifier &item) const {
  QList<NodeItem> nodes; _subnet.getNearest(item, nodes);
  if (0 == nodes.size()) { return true; }
  // Check if this node is responsible for the given item
  return (nodes.back().id()-item) < (_subnet.root().id()-item);
}

bool
DHT::hasNodesFor(const Identifier &item) const {
  return _table.contains(item);
}

void
DHT::nodesFor(const Identifier &item, QList<NodeItem> &nodes) const {
  _table[item].get(nodes);
}

void
DHT::addNodeFor(const Identifier &item, const NodeItem &node) {
  if (! _table.contains(item)) {
    _table[item] = NodeRefTable();
  }
  _table[item].add(node);
}

void
DHT::addAnnouncement(const Identifier &item) {
  _announcements.insert(item, QDateTime::currentDateTime());
}

void
DHT::announce(const Identifier &item) {
  // start search for nearest neighbours of item
  DHTAnnounceQuery *query = new DHTAnnounceQuery(_subnet, item);
  connect(query, SIGNAL(announced(Identifier)), this, SLOT(itemAnnounced()));
}

void
DHT::membersFoundEvent(const Identifier &id, const QList<NodeItem> &nodes) {
  // If members should be notified
  if (! _announcements.contains(id))
    return;
  // if there is a neighbourhood -> update timestamp
  if (nodes.size())
    _announcements.insert(id, QDateTime::currentDateTime());

  // Announce id to all nodes
  foreach (NodeItem node, nodes) {
    new JsonQuery(_subnet.prefix()+"::dht", "/"+id.toBase32(), _subnet.root(), node);
  }
}

void
DHT::itemFoundEvent(const Identifier &id, const QList<NodeItem> &nodes) {
  emit itemFound(id, nodes);
}


/* ********************************************************************************************** *
 * Implementation of DHT::NodeRef
 * ********************************************************************************************** */
DHT::NodeRef::NodeRef()
  : NodeItem()
{
  // pass...
}

DHT::NodeRef::NodeRef(const NodeItem &node)
  : NodeItem(node), _timestamp(QDateTime::currentDateTime())
{
  // pass...
}

DHT::NodeRef::NodeRef(const NodeRef &other)
  : NodeItem(other), _timestamp(other._timestamp)
{
  // pass...
}

DHT::NodeRef &
DHT::NodeRef::operator =(const NodeRef &other) {
  NodeItem::operator =(other);
  _timestamp = other._timestamp;
  return *this;
}

bool
DHT::NodeRef::isOlderThan(size_t sec) const {
  return _timestamp.secsTo(QDateTime::currentDateTime())>sec;
}


/* ********************************************************************************************** *
 * Implementation of DHT::NodeRefTable
 * ********************************************************************************************** */
DHT::NodeRefTable::NodeRefTable()
  : QHash<Identifier, DHT::NodeRef>()
{
  // pass...
}

DHT::NodeRefTable::NodeRefTable(const NodeRefTable &other)
  : QHash<Identifier, DHT::NodeRef>(other)
{
  // pass...
}

DHT::NodeRefTable &
DHT::NodeRefTable::operator =(const NodeRefTable &other) {
  QHash<Identifier, DHT::NodeRef>::operator =(other);
  return *this;
}

void
DHT::NodeRefTable::add(const NodeItem &node) {
  (*this)[node.id()] = NodeRef(node);
}

void
DHT::NodeRefTable::removeOlderThan(size_t sec) {
  QList<Identifier> rem;
  // Collect node references that are outdated
  NodeRefTable::iterator item = this->begin();
  for (; item != this->end(); item++) {
    if (item->isOlderThan(sec)) {
      rem.append(item->id());
    }
  }
  // Remove outdated references
  QList<Identifier>::iterator node = rem.begin();
  for (; node != rem.end(); node++) {
    this->remove(*node);
  }
}

void
DHT::NodeRefTable::get(QList<NodeItem> &nodes) const {
  NodeRefTable::const_iterator item = this->begin();
  for (; item != this->end(); item++) {
    nodes.push_back(*item);
  }
}



/* ********************************************************************************************** *
 * Implementation of DHTSubNetHandler
 * ********************************************************************************************** */
DHTSearchHandler::DHTSearchHandler(DHT &dht)
  : HttpRequestHandler(), _dht(dht)
{
  // pass...
}

DHTSearchHandler::~DHTSearchHandler() {
  // pass...
}

bool
DHTSearchHandler::acceptReqest(HttpRequest *request) {
  return (HTTP_GET == request->method()) || (HTTP_POST == request->method());
}

HttpResponse *
DHTSearchHandler::processRequest(HttpRequest *request) {
  if ((HTTP_GET == request->method()) && (request->uri().path().startsWith("/"))) {
    Identifier itemid = Identifier::fromBase32(request->uri().path().mid(1));
    if (! itemid.isValid()) {
      return new HttpStringResponse(request->version(), HTTP_BAD_REQUEST, "", request->socket());
    }
    return _processSearch(itemid, request);
  } else if (HTTP_POST == request->method()) {
    Identifier itemid = Identifier::fromBase32(request->uri().path().mid(1));
    if (! itemid.isValid()) {
      return new HttpStringResponse(request->version(), HTTP_BAD_REQUEST, "", request->socket());
    }
    return _processAnnouncement(itemid, request);
  }
  return new HttpStringResponse(request->version(), HTTP_BAD_REQUEST, "", request->socket());
}

HttpResponse *
DHTSearchHandler::_processSearch(const Identifier &itemid, HttpRequest *request) {
  // Will hold the node list of the response
  QList<NodeItem> nodes;
  HttpResponseCode res_code;

  if (_dht.hasNodesFor(itemid)) {
    // Return nodes that provides the requested item
    _dht.nodesFor(itemid, nodes);
    res_code = HTTP_OK;
  } else {
    // Return nodes that are near the requested item
    _dht.subnet().getNearest(itemid, nodes);
    res_code = HTTP_SEE_OTHER;
  }

  // Assemble response node list
  QJsonArray nodelst;
  foreach (NodeItem node, nodes) {
    QJsonArray nodeitem;
    nodeitem.append(node.id().toBase32());
    nodeitem.append(node.addr().toString());
    nodeitem.append(node.port());
    nodelst.append(nodeitem);
  }
  // go.
  return new HttpJsonResponse(QJsonDocument(nodelst), request, res_code);
}

HttpResponse *
DHTSearchHandler::_processAnnouncement(const Identifier &item, HttpRequest *request) {
  if (! _dht.isResponsible(item)) {
    // Return nodes that are near the requested item
    QList<NodeItem> nodes;
    _dht.subnet().getNearest(item, nodes);
    // Assemble response node list
    QJsonArray nodelst;
    foreach (NodeItem node, nodes) {
      QJsonArray nodeitem;
      nodeitem.append(node.id().toBase32());
      nodeitem.append(node.addr().toString());
      nodeitem.append(node.port());
      nodelst.append(nodeitem);
    }
    // go.
    return new HttpJsonResponse(QJsonDocument(nodelst), request, HTTP_SEE_OTHER);
  }

  // Otherwise
  _dht.addNodeFor(item, request->remote());
  return new HttpStringResponse(request->version(), HTTP_OK, "", request->socket());
}

/* ********************************************************************************************** *
 * Implementation of DHTSearchRequest
 * ********************************************************************************************** */
DHTSearchRequest::DHTSearchRequest(DHT &dht, const NodeItem &remote, SearchQuery *query)
  : JsonQuery("dht", "/"+query->id().toBase32(), dht.subnet(), remote),
    _dht(dht), _queryobj(query), _redirect(false)
{
  // pass...
}

SearchQuery *
DHTSearchRequest::query() {
  return _queryobj;
}

void
DHTSearchRequest::error() {
  logDebug() << "Search for " << _queryobj->id() << " at " << _remoteId << " failed.";
  // Get next node
  NodeItem next;
  if (! _queryobj->next(next)) {
    logDebug() << "Search for "<< _queryobj->id() << " failed.";
    _queryobj->searchFailed();
  } else {
    // query next node
    new DHTSearchRequest(_dht, next, _queryobj);
  }
  JsonQuery::error();
}

bool
DHTSearchRequest::accept() {
  if ((HTTP_OK != _response->responseCode()) && (HTTP_SEE_OTHER != _response->responseCode())) {
    logError() << "Cannot query '" << _query << "': Node returned " << _response->responseCode();
    return false;
  }
  if (! _response->hasResponseHeader("Content-Length")) {
    logError() << "Node response has no length!";
    return false;
  }
  if (! _response->hasResponseHeader("Content-Type")) {
    logError() << "Node response has no content type!";
    return false;
  }
  if ("application/json" != _response->responseHeader("Content-Type")) {
    logError() << "Response content type '" << _response->responseHeader("Content-Type")
               << " is not 'application/json'!";
    return false;
  }

  // Set flag if response is a redirect
  _redirect = (HTTP_SEE_OTHER == _response->responseCode());

  return true;
}

void
DHTSearchRequest::finished(const QJsonDocument &doc) {
  if (! doc.isArray()) {
    logDebug() << "Malformed search response from " << _remoteId << ".";
    this->failed();
  }

  // Process response
  QList<NodeItem> result;
  QJsonArray nodes = doc.array();
  foreach (QJsonValue item, nodes) {
    // Skip invalid entries
    if (! item.isArray()) { continue; }
    QJsonArray node = item.toArray();
    if (3 != node.size()) { continue; }
    Identifier nodeid = Identifier::fromBase32(node.at(0).toString());
    QHostAddress nodeaddr = QHostAddress(node.at(0).toString());
    uint16_t port = node.at(0).toInt();
    if ((! nodeid.isValid()) || nodeaddr.isNull() || (0 == port)) { continue; }
    // Assemble node item
    NodeItem new_node(nodeid, nodeaddr, port);
    if (_redirect) {
      // update search query queue
      _queryobj->update(new_node);
    } else {
      // add to results
      result.append(new_node);
    }
    // add node as candidate
    _dht.subnet().addCandidate(new_node);
  }

  if (result.size()) {
    _dht.itemFoundEvent(_queryobj->id(), result);
  }

  // Get next node if response was a
  NodeItem next;
  if (! _queryobj->next(next)) {
    logDebug() << "Search for "<< _queryobj->id() << " completed.";
    _queryobj->searchFailed();
  } else {
    // query next node
    new DHTSearchRequest(_dht, next, _queryobj);
  }

  JsonQuery::finished(doc);
}


/* ********************************************************************************************** *
 * Implementation of AnnounceSearchQuery
 * ********************************************************************************************** */
DHTAnnounceQuery::DHTAnnounceQuery(Network &net, const Identifier &id)
  : QObject(), _network(net), _item(id), _numQueries(0)
{
  NeighbourhoodQuery *query = new NeighbourhoodQuery(_item);
  connect(query, SIGNAL(succeeded(Identifier,QList<NodeItem>)), this, SLOT(_onNeighboursFound(Identifier,QList<NodeItem>)));
  connect(query, SIGNAL(failed(Identifier,QList<NodeItem>)), this, SLOT(_onError()));
  _numQueries = 1;
  _network.search(query);
}

void
DHTAnnounceQuery::_onNeighboursFound(const Identifier &id, const QList<NodeItem> &nodes) {
  foreach (NodeItem node, nodes) {
    // Send a push request to the node.
    JsonQuery *ann = new JsonQuery("dht", "/"+_item.toBase32(), QJsonDocument(), _network, node);
    _numQueries++;
    connect(ann, SIGNAL(success(NodeItem,QJsonDocument)), this, SLOT(_onNeighbourNotified()));
    connect(ann, SIGNAL(failed()), this, SLOT(_onError()));
  }
}

void
DHTAnnounceQuery::_onNeighbourNotified() {
  emit announced(_item);
  if (_numQueries)
    _numQueries--;
  if (0 == _numQueries)
    this->deleteLater();
}

void
DHTAnnounceQuery::_onError() {
  if (_numQueries)
    _numQueries--;
  if (0 == _numQueries)
    this->deleteLater();
}
