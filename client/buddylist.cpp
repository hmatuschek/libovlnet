#include "buddylist.h"
#include "application.h"

#include <QString>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>


/* ********************************************************************************************* *
 * Implementation of Buddy::NodeItem
 * ********************************************************************************************* */
Buddy::NodeItem::NodeItem()
  : PeerItem(), _lastSeen()
{
  // pass...
}

Buddy::NodeItem::NodeItem(const QHostAddress &addr, uint16_t port)
  : PeerItem(addr, port), _lastSeen(QDateTime::currentDateTime())
{
  // pass...
}

Buddy::NodeItem::NodeItem(const NodeItem &other)
  : PeerItem(other), _lastSeen(other._lastSeen)
{
  // pass...
}

bool
Buddy::NodeItem::hasBeenSeen() const {
  return _lastSeen.isValid() && (!_addr.isNull());
}

bool
Buddy::NodeItem::isOlderThan(size_t seconds) const {
  return (_lastSeen.addSecs(seconds) < QDateTime::currentDateTime());
}

void
Buddy::NodeItem::update(const QHostAddress &addr, uint16_t port) {
  _lastSeen = QDateTime::currentDateTime();
  _addr = addr;
  _port = port;
}

void
Buddy::NodeItem::invalidate() {
  _lastSeen = QDateTime();
  _addr = QHostAddress();
  _port = 0;
}


/* ********************************************************************************************* *
 * Implementation of Buddy
 * ********************************************************************************************* */
Buddy::Buddy()
  : _nodes()
{
  // pass...
}

const QHash<Identifier, Buddy::NodeItem> &
Buddy::nodes() const {
  return _nodes;
}

Buddy::NodeItem &
Buddy::node(const Identifier &id) {
  return _nodes[id];
}

QJsonObject
Buddy::toJson() const {
  QJsonObject obj;
  QJsonArray nodes;

  QHash<Identifier, Buddy::NodeItem>::const_iterator node = _nodes.begin();
  for (; node != _nodes.end(); node++) {
    nodes.append(QString(node.key().toHex()));
  }
  obj.insert("nodes", nodes);
  return obj;
}

Buddy *
Buddy::fromJson(const QJsonObject &obj) {
  Buddy *buddy = new Buddy();
  if (obj.contains("nodes") && obj["nodes"].isArray()) {
    QJsonArray nodes = obj["nodes"].toArray();
    for (int i=0; i<nodes.size(); i++) {
      buddy->_nodes.insert(
            QByteArray::fromHex(nodes.at(i).toString().toLocal8Bit()), NodeItem());
    }
  }
  return buddy;
}


/* ********************************************************************************************* *
 * Implementation of BuddyList
 * ********************************************************************************************* */
BuddyList::BuddyList(Application &application, const QString path, QObject *parent)
  : QObject(parent), _application(application), _file(path), _presenceTimer(), _searchTimer()
{
  // Setup timer to update presence of buddy nodes every 10 seconds
  _presenceTimer.setInterval(1000*10);
  _presenceTimer.setSingleShot(false);
  QObject::connect(&_presenceTimer, SIGNAL(timeout()), this, SLOT(_onUpdateNodes()));
  _presenceTimer.start();

  // Setup timer to search for offline buddies every 2 minutes
  _searchTimer.setInterval(1000*60*2);
  _searchTimer.setSingleShot(false);
  QObject::connect(&_searchTimer, SIGNAL(timeout()), this, SLOT(_onSearchNodes()));
  _searchTimer.start();

  // Read buddy list from file
  if (! _file.open(QIODevice::ReadOnly)) {
    qDebug() << "Can not read buddy list from" << _file.fileName(); return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(_file.readAll());
  _file.close();
  if (! doc.isObject()) {
    qDebug() << "Malformed buddy list."; return;
  }
  QJsonObject object = doc.object();
  QJsonObject::iterator obj = object.begin();
  for (; obj != object.end(); obj++) {
    Buddy *buddy = 0;
    if ((obj.value().isObject()) && (buddy = Buddy::fromJson(obj.value().toObject()))) {
      _buddies[obj.key()] = buddy;
      // Add to node table
      QHash<Identifier, Buddy::NodeItem>::const_iterator node = buddy->nodes().begin();
      for (; node != buddy->nodes().end(); node++) {
        _nodes.insert(node.key(), buddy);
      }
    } else {
      qDebug() << "Malformed buddy" << obj.key() << "in list.";
    }
  }
}

BuddyList::~BuddyList() {
  QHash<QString, Buddy *>::iterator item = _buddies.begin();
  for (; item != _buddies.end(); item++) {
    delete *item;
  }
  _buddies.clear();
}

bool
BuddyList::hasBuddy(const QString &name) const {
  return _buddies.contains(name);
}

bool
BuddyList::hasNode(const Identifier &id) const {
  return _nodes.contains(id);
}

Buddy *
BuddyList::getBuddy(const QString &name) const {
  return _buddies[name];
}

Buddy *
BuddyList::getBuddy(const Identifier &name) const {
  return _nodes[name];
}

void
BuddyList::addBuddy(const QString &name, const Identifier &node) {
  if (! _buddies.contains(name)) {
    Buddy *buddy = new Buddy();
    buddy->_nodes.insert(node, Buddy::NodeItem());
    _buddies.insert(name, buddy);
    _nodes.insert(node, buddy);
    emit buddyAdded(name);
  } else {
    _buddies[name]->_nodes.insert(node, Buddy::NodeItem());
    _nodes.insert(node, _buddies[name]);
    emit nodeAdded(name, node);
  }
  save();
}

void
BuddyList::addBuddy(const QString &name, const QList<Identifier> &nodes) {
  if (! _buddies.contains(name)) {
    Buddy *buddy = new Buddy();
    _buddies.insert(name, buddy);
    QList<Identifier>::const_iterator item = nodes.begin();
    for (; item != nodes.end(); item++) {
      buddy->_nodes.insert(*item, Buddy::NodeItem());
      _nodes.insert(*item, buddy);
    }
    emit buddyAdded(name);
  } else {
    Buddy *buddy = _buddies[name];
    QList<Identifier>::const_iterator item = nodes.begin();
    for (; item != nodes.end(); item++) {
      buddy->_nodes.insert(*item, Buddy::NodeItem());
      _nodes.insert(*item, buddy);
      emit nodeAdded(name, *item);
    }
  }
  save();
}

void
BuddyList::addBuddy(const QString &name, const QSet<Identifier> &nodes) {
  if (! _buddies.contains(name)) {
    Buddy *buddy = new Buddy();
    _buddies.insert(name, buddy);
    QSet<Identifier>::const_iterator item = nodes.begin();
    for (; item != nodes.end(); item++) {
      buddy->_nodes.insert(*item, Buddy::NodeItem());
      _nodes.insert(*item, buddy);
    }
    emit buddyAdded(name);
  } else {
    Buddy *buddy = _buddies[name];
    QSet<Identifier>::const_iterator item = nodes.begin();
    for (; item != nodes.end(); item++) {
      buddy->_nodes.insert(*item, Buddy::NodeItem());
      _nodes.insert(*item, buddy);
      emit nodeAdded(name, *item);
    }
  }
  save();
}

void
BuddyList::delBuddy(const QString &name) {
  if (! _buddies.contains(name)) { return; }
  delete _buddies[name];
  _buddies.remove(name);
  emit buddyRemoved(name);
  save();
}

void
BuddyList::delNode(const QString &name, const Identifier &node) {
  if (! _buddies.contains(name)) { return; }
  if (! _buddies[name]->_nodes.contains(node)) { return; }
  _buddies[name]->_nodes.remove(node);
  _nodes.remove(node);
  emit nodeRemoved(name, node);
  save();
}

const QHash<QString, Buddy *> &
BuddyList::buddies() const {
  return _buddies;
}

void
BuddyList::save()  {
  if (!_file.open(QIODevice::WriteOnly)) {
    qDebug() << "Cannot write buddy list!"; return;
  }

  QJsonDocument doc;
  QJsonObject lst;
  QHash<QString, Buddy *>::const_iterator buddy = _buddies.begin();
  for (; buddy != _buddies.end(); buddy++) {
    lst.insert(buddy.key(), (*buddy)->toJson());
  }
  doc.setObject(lst);
  _file.write(doc.toJson());
  _file.close();
}

void
BuddyList::_onNodeFound(const NodeItem &node) {
  // check if node belongs to a buddy
  if (! _nodes.contains(node.id())) { return; }
  // Send ping to node
  _application.dht().ping(node.addr(), node.port());
}

void
BuddyList::_onNodeReacable(const NodeItem &node) {
  // check if node belongs to a buddy
  if (! _nodes.contains(node.id())) { return; }
  if (! _nodes[node.id()]->node(node.id()).hasBeenSeen()) {
    // Update node
    _nodes[node.id()]->node(node.id()).update(node.addr(), node.port());
    emit appeared(node.id());
  }
  // Update node
  _nodes[node.id()]->node(node.id()).update(node.addr(), node.port());
}

void
BuddyList::_onUpdateNodes() {
  QHash<Identifier, Buddy *>::iterator node = _nodes.begin();
  for (; node != _nodes.end(); node++) {
    Buddy::NodeItem &nodeitem = (*node)->node(node.key());
    if (nodeitem.hasBeenSeen() && nodeitem.isOlderThan(60)) {
      // Lost contact to node
      nodeitem.invalidate();
      emit disappeared(node.key());
    } else if (nodeitem.hasBeenSeen() && nodeitem.isOlderThan(30)) {
      // If last contact is older than 30 second -> ping node
      _application.dht().ping(nodeitem.addr(), nodeitem.port());
    }
  }
}

void
BuddyList::_onSearchNodes() {
  QHash<Identifier, Buddy *>::iterator node = _nodes.begin();
  for (; node != _nodes.end(); node++) {
    Buddy::NodeItem &nodeitem = (*node)->node(node.key());
    if (! nodeitem.hasBeenSeen()) {
      _application.dht().findNode(node.key());
    }
  }
}
