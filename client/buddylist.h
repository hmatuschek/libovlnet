#ifndef BUDDYLIST_H
#define BUDDYLIST_H

#include "lib/dht.h"

#include <QObject>
#include <QFile>
#include <QJsonObject>
#include <QSet>

/** Represents a buddy, a collection of nodes which you trust. */
class Buddy
{
public:
  class NodeItem : public PeerItem {
  public:
    NodeItem();
    NodeItem(const QHostAddress &addr, uint16_t port);
    NodeItem(const NodeItem &other);

    bool hasBeenSeen() const;
    bool isOlderThan(size_t seconds) const;
    void update(const QHostAddress &addr, uint16_t port);
    void invalidate();

  protected:
    QDateTime _lastSeen;
  };

public:
  Buddy();

  const QHash<Identifier, NodeItem> &nodes() const;
  NodeItem &node(const Identifier &id);
  QJsonObject toJson() const;

public:
  static Buddy *fromJson(const QJsonObject &obj);

protected:
  QHash<Identifier, NodeItem> _nodes;
  friend class BuddyList;
};

class Application;

/** A list of @c Buddy instances being updated regularily. */
class BuddyList: public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * @param path Specifies the path to the JSON file containing the saved buddy list. */
  explicit BuddyList(Application &application, const QString path, QObject *parent=0);
  /** Destructor. */
  virtual ~BuddyList();

  /** Returns @c true of the given buddy is in the list. */
  bool hasBuddy(const QString &name) const;
  /** Returns @c true if the node is assigned to a buddy. */
  bool hasNode(const Identifier &id) const;
  /** Returns the specified buddy. */
  Buddy *getBuddy(const QString &name) const;
  /** Returns the budy associated with the given node. */
  Buddy *getBuddy(const Identifier &id) const;
  /** Returns the name of the buddy with the associated node. */
  QString buddyName(const Identifier &id) const;
  /** Add a buddy to the list or the given node to the speicified buddy. */
  void addBuddy(const QString &name, const Identifier &node);
  /** Add a buddy to the list or the given nodes to the speicified buddy. */
  void addBuddy(const QString &name, const QSet<Identifier> &nodes);
  /** Add a buddy to the list or the given nodes to the speicified buddy. */
  void addBuddy(const QString &name, const QList<Identifier> &nodes);
  /** Removes a buddy from the list. */
  void delBuddy(const QString &name);
  /** Removes a node from the given buddy. */
  void delNode(const QString &name, const Identifier &node);
  /** Retruns the table of buddies. */
  const QHash<QString, Buddy *> &buddies() const;

public slots:
  /** Saves the buddy list. */
  void save();

signals:
  void buddyAdded(const QString &name);
  void buddyRemoved(const QString &name);
  void nodeAdded(const QString &buddy, const Identifier &node);
  void nodeRemoved(const QString &buddy, const Identifier &node);
  void appeared(const Identifier &id);
  void disappeared(const Identifier &id);

protected slots:
  void _onNodeReachable(const NodeItem &node);
  void _onNodeFound(const NodeItem &node);
  void _onUpdateNodes();
  void _onSearchNodes();

protected:
  Application &_application;
  QFile _file;
  QHash<QString, Buddy *> _buddies;
  QHash<Identifier, QString> _nodes;
  QTimer _presenceTimer;
  QTimer _searchTimer;
};


#endif // BUDDYLIST_H
