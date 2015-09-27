#ifndef BUDDYLIST_H
#define BUDDYLIST_H

#include "lib/dht.h"

#include <QObject>
#include <QFile>
#include <QJsonObject>
#include <QSet>


class Buddy
{
public:
  Buddy();

  const QSet<Identifier> &nodes() const;
  QJsonObject toJson() const;

public:
  static Buddy *fromJson(const QJsonObject &obj);

protected:
  QSet<Identifier> _nodes;
  friend class BuddyList;
};


class BuddyList: public QObject
{
  Q_OBJECT

public:
  explicit BuddyList(const QString path, QObject *parent=0);
  virtual ~BuddyList();

  bool hasBuddy(const QString &name) const;
  bool hasNode(const Identifier &id) const;
  Buddy *getBuddy(const QString &name) const;
  Buddy *getBuddy(const Identifier &name) const;
  void addBuddy(const QString &name, const Identifier &node);
  void addBuddy(const QString &name, const QSet<Identifier> &nodes);
  void addBuddy(const QString &name, const QList<Identifier> &nodes);
  void delBuddy(const QString &name);
  void delNode(const QString &name, const Identifier &node);
  const QHash<QString, Buddy *> &buddies() const;

public slots:
  void save();

signals:
  void buddyAdded(const QString &name);
  void buddyRemoved(const QString &name);
  void nodeAdded(const QString &buddy, const Identifier &node);
  void nodeRemoved(const QString &buddy, const Identifier &node);

protected:
  QFile _file;
  QHash<QString, Buddy *> _buddies;
  QHash<Identifier, Buddy *> _nodes;
};


#endif // BUDDYLIST_H
