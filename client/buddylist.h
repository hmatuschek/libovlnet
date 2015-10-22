#ifndef BUDDYLIST_H
#define BUDDYLIST_H

#include "lib/dht.h"

#include <QObject>
#include <QFile>
#include <QJsonObject>
#include <QSet>
#include <QAbstractItemModel>

class Application;


/** A list of @c Buddy instances being updated regularily. */
class BuddyList: public QAbstractItemModel
{
  Q_OBJECT

public:
  // Forward decl.
  class Buddy;

  class Item
  {
  protected:
    Item(Item *parent);

  public:
    virtual ~Item();
    Item *parent() const;

  protected:
    Item *_parent;
  };


  class Node : public Item, public ::NodeItem
  {
  public:
    Node(const Identifier &id, Buddy *parent);
    Node(const Identifier &id, const QHostAddress &addr, uint16_t port, Buddy *parent);

    bool hasBeenSeen() const;
    bool isOlderThan(size_t seconds) const;
    void update(const QHostAddress &addr, uint16_t port);
    void invalidate();
    bool isReachable() const;

  protected:
    QDateTime _lastSeen;
  };


  /** Represents a buddy, a collection of nodes you trust. */
  class Buddy: public Item
  {
  public:
    typedef QVector<Node *>::iterator iterator;
    typedef QVector<Node *>::const_iterator const_iterator;

  public:
    Buddy(const QString &name);

    size_t numNodes() const;
    bool hasNode(const Identifier &id) const;
    Node *node(size_t idx);
    Node *node(const Identifier &id);
    /** Returns the index of the given node within this buddy or -1 if the node is not associated
     * with this buddy. */
    int index(const Identifier &id) const;
    void delNode(const Identifier &id);
    void addNode(const Identifier &id, const QHostAddress &host=QHostAddress(), uint16_t port=0);

    QJsonObject toJson() const;
    const QString &name() const;

    bool isReachable() const;

    inline iterator begin() { return _nodes.begin(); }
    inline iterator end() { return _nodes.end(); }
    inline const_iterator begin() const { return _nodes.begin(); }
    inline const_iterator end() const { return _nodes.end(); }

  public:
    static Buddy *fromJson(const QJsonObject &obj);

  protected:
    QString _name;
    QVector<Node *> _nodes;
    QHash<Identifier, size_t> _nodeTable;
    friend class BuddyList;
  };


public:
  /** Constructor.
   * @param path Specifies the path to the JSON file containing the saved buddy list. */
  explicit BuddyList(Application &application, const QString path, QObject *parent=0);
  /** Destructor. */
  virtual ~BuddyList();

  /** Retruns the number of buddies. */
  size_t numBuddies() const;
  bool isBuddy(const QModelIndex &idx) const;
  bool isNode(const QModelIndex &idx) const;
  /** Returns @c true of the given buddy is in the list. */
  bool hasBuddy(const QString &name) const;
  /** Returns @c true if the node is assigned to a buddy. */
  bool hasNode(const Identifier &id) const;

  /** Returns the specified buddy. */
  Buddy *getBuddy(size_t idx) const;
  Buddy *getBuddy(const QModelIndex &idx) const;
  /** Returns the specified buddy. */
  Buddy *getBuddy(const QString &name) const;
  /** Returns the budy associated with the given node. */
  Buddy *getBuddy(const Identifier &id) const;
  /** Returns the name of the buddy with the associated node. */
  QString buddyName(const Identifier &id) const;

  /** Add a buddy to the list. */
  void addBuddy(const QString &name, const Identifier &node);
  /** Removes a buddy by index. */
  void delBuddy(const QModelIndex &idx);
  Node *getNode(const QModelIndex &idx) const;
  void delNode(const QModelIndex &idx);
  /** Removes a buddy from the list. */
  void delBuddy(const QString &name);
  /** Removes a node from the given buddy. */
  void delNode(const QString &name, const Identifier &node);

  // Implementation of QAbstractItemModel
  QModelIndex index(int row, int column, const QModelIndex &parent) const;
  QModelIndex parent(const QModelIndex &child) const;
  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;

public slots:
  /** Saves the buddy list. */
  void save();

signals:
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

  QVector<Buddy *> _buddies;
  QHash<QString, size_t> _buddyTable;
  QHash<Identifier, size_t> _nodes;

  QTimer _presenceTimer;
  QTimer _searchTimer;
};


#endif // BUDDYLIST_H
