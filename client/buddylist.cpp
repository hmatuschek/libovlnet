#include "buddylist.h"
#include "application.h"

#include <QString>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

// Number of seconds before a node is considered as lost
#define NODE_LOSS_TIMEOUT 60


/* ********************************************************************************************* *
 * Implementation of BuddyList::Item
 * ********************************************************************************************* */
BuddyList::Item::Item(Item *parent)
  : _parent(parent)
{
  // pass...
}

BuddyList::Item::~Item() {
  // pass...
}

BuddyList::Item *
BuddyList::Item::parent() const {
  return _parent;
}

/* ********************************************************************************************* *
 * Implementation of BuddyList::NodeItem
 * ********************************************************************************************* */
BuddyList::Node::Node(const Identifier &id, BuddyList::Buddy *buddy)
  : Item(buddy), ::NodeItem(id, QHostAddress(), 0), _lastSeen()
{
  // pass...
}

BuddyList::Node::Node(const Identifier &id, const QHostAddress &addr, uint16_t port, Buddy *parent)
  : Item(parent), ::NodeItem(id, addr, port), _lastSeen(QDateTime::currentDateTime())
{
  // pass...
}

bool
BuddyList::Node::hasBeenSeen() const {
  return _lastSeen.isValid() && (!_addr.isNull());
}

bool
BuddyList::Node::isOlderThan(size_t seconds) const {
  return (_lastSeen.addSecs(seconds) < QDateTime::currentDateTime());
}

void
BuddyList::Node::update(const QHostAddress &addr, uint16_t port) {
  _lastSeen = QDateTime::currentDateTime();
  _addr = addr;
  _port = port;
}

void
BuddyList::Node::invalidate() {
  _lastSeen = QDateTime();
  _addr = QHostAddress();
  _port = 0;
}

bool
BuddyList::Node::isReachable() const {
  return hasBeenSeen() && !isOlderThan(NODE_LOSS_TIMEOUT);
}


/* ********************************************************************************************* *
 * Implementation of Buddy
 * ********************************************************************************************* */
BuddyList::Buddy::Buddy(const QString &name)
  : Item(0), _name(name), _nodeTable()
{
  // pass...
}

size_t
BuddyList::Buddy::numNodes() const {
  return _nodes.size();
}

BuddyList::Node *
BuddyList::Buddy::node(size_t idx) {
  return _nodes[idx];
}

BuddyList::Node *
BuddyList::Buddy::node(const Identifier &id) {
  return _nodes[_nodeTable[id]];
}

bool
BuddyList::Buddy::hasNode(const Identifier &id) const {
  return _nodeTable.contains(id);
}

const QString &
BuddyList::Buddy::name() const {
  return _name;
}

bool
BuddyList::Buddy::isReachable() const {
  for (int i=0; i<_nodes.size(); i++) {
    if (_nodes[i]->isReachable()) { return true; }
  }
  return false;
}

QJsonObject
BuddyList::Buddy::toJson() const {
  QJsonArray nodes;
  QVector<BuddyList::Node *>::const_iterator node = _nodes.begin();
  for (; node != _nodes.end(); node++) {
    nodes.append(QString((*node)->id().toHex()));
  }
  QJsonObject obj;
  obj.insert("name", _name);
  obj.insert("nodes", nodes);
  return obj;
}

BuddyList::Buddy *
BuddyList::Buddy::fromJson(const QJsonObject &obj) {
  Buddy *buddy = 0;
  if (obj.contains("name") && obj["name"].isString()) {
    buddy = new Buddy(obj["name"].toString());
    // Add nodes
    if (obj.contains("nodes") && obj["nodes"].isArray()) {
      QJsonArray nodes = obj["nodes"].toArray();
      for (int i=0; i<nodes.size(); i++) {
        buddy->_nodes.append(
              new BuddyList::Node(
                QByteArray::fromHex(nodes.at(i).toString().toLocal8Bit()), buddy));
      }
    }
  }
  return buddy;
}

void
BuddyList::Buddy::delNode(const Identifier &id) {
  if (! _nodeTable.contains(id)) { return; }
  size_t idx = _nodeTable[id];
  _nodeTable.remove(id);
  delete _nodes[idx]; _nodes.remove(idx);
  // update indices
  for (int i=0; i<_nodes.size(); i++) {
    _nodeTable[_nodes[i]->id()] = i;
  }
}

void
BuddyList::Buddy::addNode(const Identifier &id, const QHostAddress &host, uint16_t port) {
  _nodeTable[id] = _nodes.size();
  _nodes.append(new BuddyList::Node(id, host, port, this));
}


/* ********************************************************************************************* *
 * Implementation of BuddyList
 * ********************************************************************************************* */
BuddyList::BuddyList(Application &application, const QString path, QObject *parent)
  : QAbstractItemModel(parent), _application(application), _file(path),
    _presenceTimer(), _searchTimer()
{
  // Setup timer to update presence of buddy nodes every 10 seconds
  _presenceTimer.setInterval(1000*10);
  _presenceTimer.setSingleShot(false);
  connect(&_presenceTimer, SIGNAL(timeout()), this, SLOT(_onUpdateNodes()));
  _presenceTimer.start();

  // Setup timer to search for offline buddies every 2 minutes
  _searchTimer.setInterval(1000*60*2);
  _searchTimer.setSingleShot(false);
  connect(&_searchTimer, SIGNAL(timeout()), this, SLOT(_onSearchNodes()));
  _searchTimer.start();

  // Get notified if a node is reachable
  connect(&_application.dht(), SIGNAL(nodeFound(NodeItem)), this, SLOT(_onNodeFound(NodeItem)));
  connect(&_application.dht(), SIGNAL(nodeReachable(NodeItem)), this, SLOT(_onNodeReachable(NodeItem)));

  // Read buddy list from file
  if (! _file.open(QIODevice::ReadOnly)) {
    logInfo() << "Can not read buddy list from " << _file.fileName(); return;
  }
  logDebug() << "Read buddy list from file " << _file.fileName();

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(_file.readAll(), &err);
  _file.close();
  if (! doc.isArray()) {
    logError() << "Malformed buddy list:" << err.offset << err.errorString(); return;
  }
  QJsonArray lst = doc.array();
  QJsonArray::iterator obj = lst.begin();
  for (; obj != lst.end(); obj++) {
    Buddy *buddy = 0;
    if (((*obj).isObject()) && (buddy = Buddy::fromJson((*obj).toObject()))) {
      size_t idx = _buddies.size();
      _buddies.append(buddy);
      _buddyTable[buddy->name()] = idx;
      // Add to nodes table
      Buddy::const_iterator node = buddy->begin();
      for (; node != buddy->end(); node++) {
        _nodes.insert((*node)->id(), idx);
      }
    } else {
      logWarning() << "Malformed buddy in list:" << (*obj).toString();
    }
  }
}

BuddyList::~BuddyList() {
  QVector<Buddy *>::iterator item = _buddies.begin();
  for (; item != _buddies.end(); item++) {
    delete *item;
  }
  _buddyTable.clear();
}

size_t
BuddyList::numBuddies() const {
  return _buddies.size();
}

bool
BuddyList::isBuddy(const QModelIndex &idx) const {
  if (! idx.isValid()) { return false; }
  return 0 != dynamic_cast<Buddy *>(
        reinterpret_cast<Item *>(idx.internalPointer()));
}

bool
BuddyList::isNode(const QModelIndex &idx) const {
  if (! idx.isValid()) { return false; }
  return 0 != dynamic_cast<Node *>(
        reinterpret_cast<Item *>(idx.internalPointer()));
}

bool
BuddyList::hasBuddy(const QString &name) const {
  return _buddyTable.contains(name);
}

bool
BuddyList::hasNode(const Identifier &id) const {
  return _nodes.contains(id);
}

BuddyList::Buddy *
BuddyList::getBuddy(size_t idx) const {
  return _buddies[idx];
}
BuddyList::Buddy *
BuddyList::getBuddy(const QString &name) const {
  return _buddies[_buddyTable[name]];
}

BuddyList::Buddy *
BuddyList::getBuddy(const Identifier &id) const {
  return _buddies[_nodes[id]];
}

BuddyList::Buddy *
BuddyList::getBuddy(const QModelIndex &idx) const {
  if (! idx.isValid()) { return 0; }
  return dynamic_cast<Buddy *>(
        reinterpret_cast<Item *>(idx.internalPointer()));
}

BuddyList::Node *
BuddyList::getNode(const QModelIndex &idx) const {
  if (! idx.isValid()) { return 0; }
  return dynamic_cast<Node *>(
        reinterpret_cast<Item *>(idx.internalPointer()));
}

QString
BuddyList::buddyName(const Identifier &id) const {
  return _buddies[_nodes[id]]->name();
}

void
BuddyList::addBuddy(const QString &name, const Identifier &node) {
  if (! _buddyTable.contains(name)) {
    Buddy *buddy = new Buddy(name);
    buddy->addNode(node);
    _buddyTable.insert(name, _buddies.size());
    _nodes.insert(node, _buddies.size());
    _buddies.append(buddy);
    emit buddyAdded(name);
  }
  save();
}

void
BuddyList::addNode(const QString &name, const Identifier &node) {
  if (! hasBuddy(name)) { return; }
  _buddies[_buddyTable[name]]->addNode(node);
  emit nodeAdded(name, node);
  save();
}

void
BuddyList::delBuddy(const QString &name) {
  if (! _buddyTable.contains(name)) { return; }
  size_t idx = _buddyTable[name];
  delete _buddies[idx];
  _buddyTable.remove(name);
  _buddies.remove(idx);
  // Update indices
  for (int i=0; i<_buddies.size(); i++) {
    _buddyTable[_buddies[i]->name()] = i;
  }
  emit buddyRemoved(name);
  save();
}

void
BuddyList::delNode(const QString &name, const Identifier &node) {
  if (! _buddyTable.contains(name)) { return; }
  size_t idx = _buddyTable[name];
  if (! _buddies[idx]->hasNode(node)) { return; }
  _buddies[idx]->delNode(node);
  _nodes.remove(node);
  emit nodeRemoved(name, node);
  save();
}

void
BuddyList::save()  {
  if (!_file.open(QIODevice::WriteOnly)) {
    logError() << "Cannot write buddy list!"; return;
  }

  QJsonDocument doc;
  QJsonArray lst;
  QVector<Buddy *>::const_iterator buddy = _buddies.begin();
  for (; buddy != _buddies.end(); buddy++) {
    lst.append((*buddy)->toJson());
  }
  doc.setArray(lst);
  _file.write(doc.toJson());
  _file.close();
}

QModelIndex
BuddyList::index(int row, int column, const QModelIndex &parent) const {
  if (! hasIndex(row, column, parent)) { return QModelIndex(); }
  // Single column model
  if (0 != column) { return QModelIndex(); }
  if ((! parent.isValid()) && (_buddies.size() > row)) {
    // If row/column & parent addresses buddy
    return createIndex(row, column, _buddies.at(row));
  } else if ( parent.isValid() && (_buddies.size() > parent.row()) ) {
    // If row/column & parent addresses node
    return createIndex(row, column, _buddies[parent.row()]->node(row));
  }
  return QModelIndex();
}

QModelIndex
BuddyList::parent(const QModelIndex &child) const {
  Item *item = reinterpret_cast<Item *>(child.internalPointer());
  if (Node *node = dynamic_cast<Node *>(item)) {
    Buddy *buddy = reinterpret_cast<Buddy *>(node->parent());
    return createIndex(_buddies.indexOf(buddy), 0, buddy);
  }
  return QModelIndex();
}

int
BuddyList::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    Item *item = reinterpret_cast<Item *>(parent.internalPointer());
    if (Buddy *buddy = dynamic_cast<Buddy *>(item)) {
      return buddy->numNodes();
    }
    // Nodes to not have children
    return 0;
  }
  return _buddies.size();
}

int
BuddyList::columnCount(const QModelIndex &parent) const {
  return 1;
}

QVariant
BuddyList::data(const QModelIndex &index, int role) const {
  // Dispatch by role
  if (Qt::DisplayRole == role) {
    Item *item = reinterpret_cast<Item *>(index.internalPointer());
    if (Node *node = dynamic_cast<Node *>(item)) { return node->id().toBase32(); }
    if (Buddy *buddy = dynamic_cast<Buddy *>(item)) { return buddy->name(); }
  } else if (Qt::DecorationRole == role) {
    Item *item = reinterpret_cast<Item *>(index.internalPointer());
    if (Node *node = dynamic_cast<Node *>(item)) {
      if (node->isReachable()) {
        return QIcon("://icons/fork.png");
      }
      return QIcon("://icons/fork_gray.png");
    }
    if (Buddy *buddy = dynamic_cast<Buddy *>(item)) {
      if (buddy->isReachable()) {
        return QIcon("://icons/person.png");
      }
      return QIcon("://icons/person_gray.png");
    }
  }
  return QVariant();
}



void
BuddyList::_onNodeFound(const NodeItem &node) {
  // check if node belongs to a buddy
  if (! _nodes.contains(node.id())) { return; }
  logDebug() << "Node " << node.id() << " found @"
             << node.addr() << ":" << node.port() << ": Send ping request.";
  // Send ping to node
  _application.dht().ping(node.addr(), node.port());
}

void
BuddyList::_onNodeReachable(const NodeItem &node) {
  // check if node belongs to a buddy
  if (! _nodes.contains(node.id())) { return; }
  logDebug() << "Node " << node.id() << " reachable at " << node.addr() << ":" << node.port();
  BuddyList::Buddy *buddy = _buddies[_nodes[node.id()]];
  BuddyList::Node *nodeitem = buddy->node(node.id());
  if (! nodeitem->hasBeenSeen()) {
    // Update node
    nodeitem->update(node.addr(), node.port());
    emit appeared(node.id());
    logDebug() << "Node " << node.id() << " appeared.";
  }
  // Update node
  nodeitem->update(node.addr(), node.port());
  // Update items (buddy and all its nodes)
  QModelIndex bidx = index(_nodes[node.id()], 0, QModelIndex()), nidx = bidx;
  if (buddy->numNodes()) { nidx = index(buddy->numNodes()-1, 0, bidx); }
  emit dataChanged(bidx, nidx);
}

void
BuddyList::_onUpdateNodes() {
  QHash<Identifier, size_t>::iterator node = _nodes.begin();
  for (; node != _nodes.end(); node++) {
    BuddyList::Buddy *buddy = _buddies[node.value()];
    BuddyList::Node *nodeitem = buddy->node(node.key());
    if (nodeitem->hasBeenSeen() && nodeitem->isOlderThan(NODE_LOSS_TIMEOUT)) {
      // Lost contact to node
      nodeitem->invalidate();
      emit disappeared(node.key());
      // Update items (buddy and all its nodes)
      QModelIndex bidx = index(node.value(), 0, QModelIndex()), nidx = bidx;
      if (buddy->numNodes()) { nidx = index(buddy->numNodes()-1, 0, bidx); }
      emit dataChanged(bidx, nidx);
    } else if (nodeitem->hasBeenSeen() && nodeitem->isOlderThan(NODE_LOSS_TIMEOUT/2)) {
      // If last contact is older than 30 second -> ping node
      _application.dht().ping(nodeitem->addr(), nodeitem->port());
    }
  }
}

void
BuddyList::_onSearchNodes() {
  QHash<Identifier, size_t>::iterator node = _nodes.begin();
  for (; node != _nodes.end(); node++) {
    BuddyList::Node *nodeitem = _buddies[node.value()]->node(node.key());
    if (! nodeitem->hasBeenSeen()) {
      _application.dht().findNode(node.key());
    }
  }
}
