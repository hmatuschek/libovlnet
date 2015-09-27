#include "buddylist.h"

#include <QString>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>


/* ********************************************************************************************* *
 * Implementation of BuddyList
 * ********************************************************************************************* */
BuddyList::BuddyList(const QString path, QObject *parent)
  : QObject(parent), _file(path)
{
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
      QSet<Identifier>::const_iterator node = buddy->nodes().begin();
      for (; node != buddy->nodes().end(); node++) {
        _nodes.insert(*node, buddy);
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
    buddy->_nodes.insert(node);
    _buddies.insert(name, buddy);
    _nodes.insert(node, buddy);
    emit buddyAdded(name);
  } else {
    _buddies[name]->_nodes.insert(node);
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
      buddy->_nodes.insert(*item);
      _nodes.insert(*item, buddy);
    }
    emit buddyAdded(name);
  } else {
    Buddy *buddy = _buddies[name];
    QList<Identifier>::const_iterator item = nodes.begin();
    for (; item != nodes.end(); item++) {
      buddy->_nodes.insert(*item);
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
      buddy->_nodes.insert(*item);
      _nodes.insert(*item, buddy);
    }
    emit buddyAdded(name);
  } else {
    Buddy *buddy = _buddies[name];
    QSet<Identifier>::const_iterator item = nodes.begin();
    for (; item != nodes.end(); item++) {
      buddy->_nodes.insert(*item);
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


/* ********************************************************************************************* *
 * Implementation of Buddy
 * ********************************************************************************************* */
Buddy::Buddy()
  : _nodes()
{
  // pass...
}

const QSet<Identifier> &
Buddy::nodes() const {
  return _nodes;
}

QJsonObject
Buddy::toJson() const {
  QJsonObject obj;
  QJsonArray nodes;

  QSet<Identifier>::const_iterator node = _nodes.begin();
  for (; node != _nodes.end(); node++) {
    nodes.append(QString(node->toHex()));
  }
  obj.insert("nodes", nodes);
  return obj;
}

Buddy *
Buddy::fromJson(const QJsonObject &obj) {
  Buddy *buddy = new Buddy();
  if (obj.contains("nodes") && obj["nodes"].isArray()) {
    QJsonArray nodes = obj["nodes"].toArray();
    for (size_t i=0; i<nodes.size(); i++) {
      buddy->_nodes.insert(QByteArray::fromHex(nodes.at(i).toString().toLocal8Bit()));
    }
  }
  return buddy;
}
