#ifndef __VLF_DHT_H__
#define __VLF_DHT_H__

#include <QObject>

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <inttypes.h>
#include <QPair>
#include <QList>
#include <QVector>
#include <QSet>
#include <QHash>
#include <QDateTime>


/** Size of of the hash to use. */
#define DHT_HASH_SIZE        32
/** Maximum message size per UDP packet. */
#define DHT_MAX_MESSAGE_SIZE 1024
/** Minimum message size per UDP packet. */
#define DHT_MIN_MESSAGE_SIZE DHT_HASH_SIZE

/** The max. number of hashes fitting into an "announce" message payload. */
#define DHT_MAX_NUM_HASHES int((DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-1)/DHT_HASH_SIZE)
/** The size of the triple (hash, IP, port). */
#define DHT_TRIPLE_SIZE (DHT_HASH_SIZE + 4 + 2)
/** The max. number of triples in a response. */
#define DHT_MAX_TRIPLES int((DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-1)/DHT_TRIPLE_SIZE)
/** The max. data response. */
#define DHT_MAX_DATA_SIZE (DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-8)

/** The bucket size.
 * It is ensured that a complete bucket can be transferred with one UDP message. */
#define DHT_K std::min(20, DHT_MAX_TRIPLES)

// Forward declarations
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
  /** Empty constructor. */
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


/** Represents a single k-bucket. */
class Bucket
{
public:
  /** An element of the @c Bucket. */
  struct Item {
    /** Empty constructor. */
    Item();
    /** Constructor from identifier, address, port and prefix. */
    Item(const Identifier &id, const QHostAddress &addr, uint16_t port, size_t prefix);
    /** Copy constructor. */
    Item(const Item &other);
    /** Assignment operator. */
    Item &operator=(const Item &other);

    /** Returns the ID of the item. */
    const Identifier &id() const;
    /** Returns the @c prefix of the item w.r.t. the ID of the node. */
    size_t prefix() const;
    /** The address of the item. */
    const QHostAddress &addr() const;
    /** The port of the item. */
    uint16_t port() const;
    /** The time of the item last seen. */
    const QDateTime &lastSeen() const;

  protected:
    /** The identifier of the item. */
    Identifier   _id;
    /** The prefix -- index of the leading bit of the difference between this identifier and the
     * identifier of the node. */
    size_t       _prefix;
    /** The address of the item. */
    QHostAddress _addr;
    /** The port of the item. */
    uint16_t     _port;
    /** The time, the item was last seen. */
    QDateTime    _lastSeen;
  };

public:
  /** Constructor. */
  Bucket(size_t size=DHT_K);
  /** Copy constructor. */
  Bucket(const Bucket &other);

  /** Returns the table of triples in the bucket. */
  inline const QHash<Identifier, Item> &triples() { return _triples; }

  /** Returns @c true if the bucket is full. */
  bool full() const;
  /** Returns @c true if the bucket contains the given identifier. */
  bool contains(const Identifier &id);
  /** Add or updates an item. */
  void add(const Item &item);
  /** The minimum prefix of the bucket. */
  size_t prefix() const;
  /** Splits the bucket at its prefix. Means all item with a higher prefix (smaller distance)
   * than the prefix of this bucket are moved to the new one. */
  void split(Bucket &newBucket);

protected:
  /** The maximal bucket size. */
  size_t _maxSize;
  /** The prefix of the bucket. */
  size_t _prefix;
  /** Triple table. */
  QHash<Identifier, Item> _triples;
  /** History of items (oldest first). */
  QList<Identifier>       _history;
};


/** A ordered list of buckets. */
class Buckets
{
public:
  /** Constructor. */
  Buckets();

  /** Adds or updates an item. */
  void add(const Bucket::Item &item);
  const Bucket &getBucket(const Bucket::Item &item);

protected:
  /** Returns the bucket index, an item should be searched for. */
  size_t index(const Bucket::Item &item) const;

protected:
  /** The bucket list. */
  QVector<Bucket> _buckets;
};


/** Represents a triple of ID, IP address and port as transferred via UDP. */
typedef struct {
  /** The ID of a node. */
  char     id[DHT_HASH_SIZE];
  /** The IP of the node. */
  uint8_t  ip[4];
  /** The port of the node. */
  uint16_t port;
} DHTTriple;


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
      char    payload[DHT_MAX_NUM_HASHES];
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
  };
} Message;


class RequestItem
{
protected:
  RequestItem(Message::Type type);

public:
  inline Message::Type type() const { return _type; }
  inline const QDateTime &timestamp() const { return _timeStamp; }

protected:
  Message::Type _type;
  QDateTime     _timeStamp;
};


class PingRequestItem: public RequestItem
{
public:
  PingRequestItem(const Identifier &id);

  inline const Identifier &identifier() const { return _id; }

protected:
  Identifier _id;
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

signals:

protected slots:
  /** Gets called on the reception of a UDP package. */
  void _onReadyRead();

protected:
  /** The identifier of the node. */
  Identifier _id;
  /** The network socket. */
  QUdpSocket _socket;
  /** The routing table. */
  Buckets    _buckets;
  /** The kex->value map. */
  QHash<Identifier, QVector<Bucket::Item> > _table;
  /** The list of pending requests. */
  QHash<Identifier, RequestItem *> _pendingRequests;
};



#endif // __VLF_DHT_H__
