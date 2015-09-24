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

/** Size of of the hash to use, e.g. RMD160 -> 20bytes. */
#define DHT_HASH_SIZE        20
/** Maximum message size per UDP packet. */
#define DHT_MAX_MESSAGE_SIZE 1024
/** Minimum message size per UDP packet. */
#define DHT_MIN_MESSAGE_SIZE (DHT_HASH_SIZE+1)

/** The size of the triple (hash, IPv4, port). */
#define DHT_TRIPLE_SIZE (DHT_HASH_SIZE + 4 + 2)
/** The max. number of triples in a response. */
#define DHT_MAX_TRIPLES int((DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-1)/DHT_TRIPLE_SIZE)
/** The max. data response. */
#define DHT_MAX_DATA_SIZE (DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-8)

/** The bucket size.
 * It is ensured that a complete bucket can be transferred with one UDP message. */
#define DHT_K std::min(8, DHT_MAX_TRIPLES)

// Forward declarations
class Identifier;
class FindNodeQuery;
class Node;


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
  Bucket(const Identifier &self, size_t size=DHT_K);
  /** Copy constructor. */
  Bucket(const Bucket &other);

  void getNearest(const Identifier &id, QList<NodeItem> &best) const;
  void getOlderThan(size_t age, QList<NodeItem> &nodes) const;
  void removeOlderThan(size_t age);

  /** Returns @c true if the bucket is full. */
  bool full() const;
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



/** The structure of the UDP datagrams transferred. */
typedef struct {
  /** Possible message types. */
  typedef enum {
    PING = 0,
    ANNOUNCE,
    FIND_NODE,
    FIND_VALUE,
    GET_DATA,
  } Type;

  /** Represents a triple of ID, IP address and port as transferred via UDP. */
  typedef struct {
    /** The ID of a node. */
    char     id[DHT_HASH_SIZE];
    /** The IP of the node. */
    uint32_t  ip;
    /** The port of the node. */
    uint16_t port;
  } DHTTriple;

  /** The magic cookie to match a response to a request. */
  char    cookie[DHT_HASH_SIZE];

  /** Payload */
  union {
    struct {
      uint8_t type; // == PING;
      char    id[DHT_HASH_SIZE];
    } ping;

    struct {
      uint8_t type; // == ANNOUNCE
      char    who[DHT_HASH_SIZE];
      char    what[DHT_HASH_SIZE];
    } announce;

    struct {
      uint8_t type; // == FIND_NODE
      char    id[DHT_HASH_SIZE];
    } find_node;

    struct {
      uint8_t type; // == FIND_VALUE
      char    id[DHT_HASH_SIZE];
    } find_value;

    struct {
      uint8_t   success;
      DHTTriple triples[DHT_MAX_TRIPLES];
    } result;

    struct {
      uint8_t  type; // == GET_DATA
      char     id[DHT_HASH_SIZE];
      uint64_t offset;
      uint64_t length;
    } get_data;

    struct {
      uint64_t offset;
      char     data[DHT_MAX_DATA_SIZE];
    } data;

    struct {
      uint64_t offset;
    } ack_data;
  } payload;
} Message;


class SearchQuery
{
protected:
  SearchQuery(const Identifier &id);

public:
  const Identifier &id() const;
  void update(const NodeItem &nodes);
  bool next(NodeItem &node);
  QList<NodeItem> &best();
  const NodeItem &first() const;

protected:
  Identifier _id;
  QList<NodeItem> _best;
  QSet<Identifier> _queried;
};


class FindNodeQuery: public SearchQuery
{
public:
  FindNodeQuery(const Identifier &id);

  bool found() const;
};


class FindValueQuery: public SearchQuery
{
public:
  FindValueQuery(const Identifier &id);
};


class Request
{
protected:
  Request(Message::Type type);

public:
  inline Message::Type type() const { return _type; }
  inline const Identifier &cookie() const { return _cookie; }
  inline const QDateTime &timestamp() const { return _timestamp; }
  inline size_t olderThan(size_t seconds) const {
    return (_timestamp.addSecs(seconds) < QDateTime::currentDateTime());
  }

protected:
  Message::Type _type;
  Identifier    _cookie;
  QDateTime     _timestamp;
};


class PingRequest: public Request
{
public:
  PingRequest();
};

class FindNodeRequest: public Request
{
public:
  FindNodeRequest(FindNodeQuery *query);
  inline FindNodeQuery *query() const { return _findNodeQuery; }

protected:
  FindNodeQuery *_findNodeQuery;
};


class FindValueRequest: public Request
{
public:
  FindValueRequest(FindValueQuery *query);
  inline FindValueQuery *query() const { return _findValueQuery; }

protected:
  FindValueQuery *_findValueQuery;
};


/** Implements a node in the DHT. */
class Node: public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * @param id Specifies the identifier of the node.
   * @param addr Specifies the network address the node will bind to.
   * @param port Specifies the network port the node will listen on.
   * @param parent Optional pararent object. */
  explicit Node(const Identifier &id, const QHostAddress &addr=QHostAddress::Any, quint16 port=7741,
                QObject *parent=0);
  /** Destructor. */
  virtual ~Node();

  void ping(const QHostAddress &addr, uint16_t port);
  void ping(const PeerItem &peer);
  void findNode(const Identifier &id);
  void findValue(const Identifier &id);
  void announce(const Identifier &id);

  virtual QIODevice *data(const Identifier &id);

signals:
  void nodeReachable(const NodeItem &node);

  void nodeFound(const NodeItem &node);
  void nodeNotFound(const Identifier &id);

  void valueFound(const Identifier &id, const QList<NodeItem> &nodes);
  void valueNotFound(const Identifier &id);

protected:
  /** Sends a FindNode message to the node @c to to search for the node specified by @c id.
   * Any response to that request will be forwarded to the specified @c query. */
  void sendFindNode(const NodeItem &to, FindNodeQuery *query);
  void sendFindValue(const NodeItem &to, FindValueQuery *query);
  bool isPendingAnnouncement(const Identifier &id) const;
  void sendAnnouncement(const NodeItem &to, const Identifier &what);

  void _processPingResponse(const Message &msg, size_t size, PingRequest *req,
                            const QHostAddress &addr, uint16_t port);

  void _processFindNodeResponse(const Message &msg, size_t size, FindNodeRequest *req,
                                const QHostAddress &addr, uint16_t port);

  void _processFindValueResponse(const Message &msg, size_t size, FindValueRequest *req,
                                const QHostAddress &addr, uint16_t port);

  void _processPingRequest(const Message &msg, size_t size,
                           const QHostAddress &addr, uint16_t port);

  void _processFindNodeRequest(const Message &msg, size_t size,
                               const QHostAddress &addr, uint16_t port);

  void _processFindValueRequest(const Message &msg, size_t size,
                                const QHostAddress &addr, uint16_t port);

  void _processAnnounceRequest(const Message &msg, size_t size,
                               const QHostAddress &addr, uint16_t port);

protected slots:
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

  QTimer _requestTimer;
  QTimer _nodeTimer;
  QTimer _announcementTimer;
};



#endif // __VLF_DHT_H__
