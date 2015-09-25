#ifndef __VLF_DHT_H__
#define __VLF_DHT_H__

#include <inttypes.h>

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <QPair>
#include <QList>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QDateTime>
#include <QTimer>

// Forward declaration
class Identifier;

/** The distance between two identifiers. */
class Distance : public QByteArray
{
public:
  /** Computes the distance between the identifiers @c a and @c b. */
  Distance(const Identifier &a, const Identifier &b);
  /** Copy constructor. */
  Distance(const Distance &distance);
  /** Returns the bit at index @c idx of the distance. Bit 0 is the MSB. */
  bool bit(size_t idx) const;
  /** Returns the index of the leading non-zero bit of the distance.*/
  size_t leadingBit() const;
};


/** Represents an identifier in the DHT. */
class Identifier : public QByteArray
{
public:
  /** Creates a new random identifier. */
  Identifier();
  /** Constructor. */
  Identifier(const char *id);
  /** Constructor. */
  Identifier(const QByteArray &id);
  /** Copy constructor. */
  Identifier(const Identifier &other);

  /** Assignment. */
  Identifier &operator=(const Identifier &other);
  /** Comparison. */
  bool operator==(const Identifier &other) const;
  /** Computes the distance. */
  Distance operator-(const Identifier &other) const;
};

inline QDebug &operator<<(QDebug &stream, const Identifier &id) {
  stream << id.toHex();
  return stream;
}

/** Represents a peer (IP address + port) in the network. */
class PeerItem
{
public:
  PeerItem();
  PeerItem(const QHostAddress &addr, uint16_t port);
  PeerItem(const PeerItem &other);

  PeerItem &operator=(const PeerItem &other);

  const QHostAddress &addr() const;
  uint16_t port() const;

protected:
  QHostAddress _addr;
  uint16_t     _port;
};


/** Represents a node (ID + IP address + port) in the network. */
class NodeItem: public PeerItem
{
public:
  NodeItem();
  NodeItem(const Identifier &id, const QHostAddress &addr, uint16_t port);
  NodeItem(const Identifier &id, const PeerItem &peer);
  NodeItem(const NodeItem &other);

  NodeItem &operator=(const NodeItem &other);

  const Identifier &id() const;

protected:
  Identifier _id;
};


class AnnouncementItem: public PeerItem
{
public:
  AnnouncementItem();
  AnnouncementItem(const QHostAddress &addr, uint16_t port);
  AnnouncementItem(const AnnouncementItem &other);

  AnnouncementItem &operator =(const AnnouncementItem &other);

  bool olderThan(size_t seconds);

protected:
  QDateTime _timestamp;
};


/** Represents a single k-bucket. */
class Bucket
{
public:
  /** An element of the @c Bucket. */
  struct Item {
    /** Empty constructor. */
    Item();
    /** Constructor from identifier, address, port and prefix. */
    Item(const QHostAddress &addr, uint16_t port, size_t prefix);
    /** Copy constructor. */
    Item(const Item &other);
    /** Assignment operator. */
    Item &operator=(const Item &other);

    /** Returns the precomputed @c prefix of the item w.r.t. the ID of the node. */
    size_t prefix() const;
    const PeerItem &peer() const;
    /** The address of the item. */
    const QHostAddress &addr() const;
    /** The port of the item. */
    uint16_t port() const;
    /** The time of the item last seen. */
    const QDateTime &lastSeen() const;
    /** Returns true if the entry is older than the specified seconds. */
    inline size_t olderThan(size_t seconds) const {
      return (_lastSeen.addSecs(seconds) < QDateTime::currentDateTime());
    }

  protected:
    /** The prefix -- index of the leading bit of the difference between this identifier and the
     * identifier of the node. */
    size_t       _prefix;
    PeerItem     _peer;
    /** The time, the item was last seen. */
    QDateTime    _lastSeen;
  };

public:
  /** Constructor. */
  Bucket(const Identifier &self);
  /** Copy constructor. */
  Bucket(const Bucket &other);

  void getNearest(const Identifier &id, QList<NodeItem> &best) const;
  void getOlderThan(size_t age, QList<NodeItem> &nodes) const;
  void removeOlderThan(size_t age);

  /** Returns @c true if the bucket is full. */
  bool full() const;
  /** Returns the number of nodes held in the bucket. */
  size_t numNodes() const;
  /** Returns @c true if the bucket contains the given identifier. */
  bool contains(const Identifier &id) const;
  /** Add or updates an item. */
  void add(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** The prefix of the bucket. */
  size_t prefix() const;

  /** Splits the bucket at its prefix. Means all item with a higher prefix (smaller distance)
   * than the prefix of this bucket are moved to the new one. */
  void split(Bucket &newBucket);

protected:
  /** Adds an item. */
  void add(const Identifier &id, const Item &item);

protected:
  Identifier _self;
  /** The maximal bucket size. */
  size_t _maxSize;
  /** The prefix of the bucket. */
  size_t _prefix;
  /** Item table. */
  QHash<Identifier, Item> _triples;
};


/** A ordered list of buckets. */
class Buckets
{
public:
  /** Constructor. */
  Buckets(const Identifier &self);

  bool empty() const;
  bool contains(const Identifier &id) const;
  /** Returns the number of nodes in the buckets. */
  size_t numNodes() const;

  /** Adds or updates an item. */
  void add(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** Collects the nearest known nodes. */
  void getNearest(const Identifier &id, QList<NodeItem> &best) const;
  /** Collects all nodes that are "older" than the specified age (in seconds). */
  void getOlderThan(size_t seconds, QList<NodeItem> &nodes) const;
  /** Removes all nodes that are "older" than the specified age (in seconds). */
  void removeOlderThan(size_t seconds);

protected:
  /** Returns the bucket index, an item should be searched for. */
  QList<Bucket>::iterator index(const Identifier &id);

protected:
  Identifier _self;
  /** The bucket list. */
  QList<Bucket> _buckets;
};




struct Message;
class Request;
class FindNodeQuery;
class FindValueQuery;
class Request;
class PingRequest;
class FindNodeRequest;
class FindValueRequest;


/** Implements a node in the DHT. */
class DHT: public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * @param id Specifies the identifier of the node.
   * @param addr Specifies the network address the node will bind to.
   * @param port Specifies the network port the node will listen on.
   * @param parent Optional pararent object. */
  explicit DHT(const Identifier &id, const QHostAddress &addr=QHostAddress::Any, quint16 port=7741,
                QObject *parent=0);
  /** Destructor. */
  virtual ~DHT();

  /** Sends a ping request to the given peer. */
  void ping(const QString &addr, uint16_t port);
  /** Sends a ping request to the given peer. */
  void ping(const QHostAddress &addr, uint16_t port);
  /** Sends a ping request to the given peer. */
  void ping(const PeerItem &peer);
  /** Starts the search for a node with the given identifier. */
  void findNode(const Identifier &id);
  /** Starts the search for a value with the given identifier. */
  void findValue(const Identifier &id);
  /** Announces a value. */
  void announce(const Identifier &id);

  /** Returns the number of nodes in the buckets. */
  size_t numNodes() const;
  /** Returns the number of keys held by this DHT node. */
  size_t numKeys() const;
  /** Retunrs the number of data items provided by this node. */
  size_t numData() const;

  /** Needs to be implemented to provide the data for the given id. */
  virtual QIODevice *data(const Identifier &id);

signals:
  /** Gets emitted if a ping was replied. */
  void nodeReachable(const NodeItem &node);
  /** Gets emitted if the given node has been found. */
  void nodeFound(const NodeItem &node);
  /** Gets emitted if the given node was not found. */
  void nodeNotFound(const Identifier &id);
  /** Gets emitted if the given value was found. */
  void valueFound(const Identifier &id, const QList<NodeItem> &nodes);
  /** Gets emitted if the given value was not found. */
  void valueNotFound(const Identifier &id);

protected:
  /** Sends a FindNode message to the node @c to to search for the node specified by @c id.
   * Any response to that request will be forwarded to the specified @c query. */
  void sendFindNode(const NodeItem &to, FindNodeQuery *query);
  /** Sends a FindValue message to the node @c to to search for the node specified by @c id.
   * Any response to that request will be forwarded to the specified @c query. */
  void sendFindValue(const NodeItem &to, FindValueQuery *query);
  /** Returns @c true if the given identifier belongs to a value being announced. */
  bool isPendingAnnouncement(const Identifier &id) const;
  /** Sends an Annouce message to the given node. */
  void sendAnnouncement(const NodeItem &to, const Identifier &what);

private:
  /** Processes a Ping response. */
  void _processPingResponse(const Message &msg, size_t size, PingRequest *req,
                            const QHostAddress &addr, uint16_t port);
  /** Processes a FindNode response. */
  void _processFindNodeResponse(const Message &msg, size_t size, FindNodeRequest *req,
                                const QHostAddress &addr, uint16_t port);
  /** Processes a FindValue response. */
  void _processFindValueResponse(const Message &msg, size_t size, FindValueRequest *req,
                                const QHostAddress &addr, uint16_t port);
  /** Processes a Ping request. */
  void _processPingRequest(const Message &msg, size_t size,
                           const QHostAddress &addr, uint16_t port);
  /** Processes a FindNode request. */
  void _processFindNodeRequest(const Message &msg, size_t size,
                               const QHostAddress &addr, uint16_t port);
  /** Processes a FindValue request. */
  void _processFindValueRequest(const Message &msg, size_t size,
                                const QHostAddress &addr, uint16_t port);
  /** Processes an Announce request. */
  void _processAnnounceRequest(const Message &msg, size_t size,
                               const QHostAddress &addr, uint16_t port);

private slots:
  /** Gets called on the reception of a UDP package. */
  void _onReadyRead();
  /** Gets called regulary to check the request timeouts. */
  void _onCheckRequestTimeout();
  /** Gets called regulary to check the timeout of the node in the buckets. */
  void _onCheckNodeTimeout();
  /** Gets called regulary to check the announcement timeouts. */
  void _onCheckAnnouncementTimeout();

protected:
  /** The identifier of the node. */
  Identifier _self;
  /** The network socket. */
  QUdpSocket _socket;
  /** The routing table. */
  Buckets    _buckets;
  /** A list of candidate peers to join the buckets. */
  QList<PeerItem> _candidates;
  /** The key->value map of the received announcements. */
  QHash<Identifier, QHash<Identifier, AnnouncementItem> > _announcements;
  /** The kay->timestamp table of the data this node provides. */
  QHash<Identifier, QDateTime> _announcedData;
  /** The list of pending requests. */
  QHash<Identifier, Request *> _pendingRequests;
  /** Timer to check timeouts of requests. */
  QTimer _requestTimer;
  /** Timer to check nodes in buckets. */
  QTimer _nodeTimer;
  /** Timer to keep announcements up-to-date. */
  QTimer _announcementTimer;
};



#endif // __VLF_DHT_H__
