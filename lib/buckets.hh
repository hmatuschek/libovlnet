#ifndef __OVL_BUCKETS_HH__
#define __OVL_BUCKETS_HH__

#include "dht_config.hh"
#include "logger.hh"

#include <QByteArray>
#include <QList>
#include <QHash>
#include <QHostAddress>
#include <QDateTime>

#include <inttypes.h>


// Forward declaration
class Identifier;

/** The distance between two identifiers.
 * @ingroup core */
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


/** Represents an identifier in the DHT.
 * @ingroup core */
class Identifier : public QByteArray
{
public:
  /** Creates an empty identifier. */
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

  /** Returns @c true if the identifier is empty. */
  bool isNull() const;
  /** Returns @c true if the identifier is a valid DHT ID. */
  bool isValid() const;

  /** Returns the base-32 (RFC4648) representation of the key. */
  QString toBase32() const;

public:
  /** Constructs a new random identifier. */
  static Identifier create();
  /** Constructs an identifier from its base-32 (RFC4648) representation. */
  static Identifier fromBase32(const QString &base32);
};

/** Logger output operation for identifier. */
inline QTextStream &operator<<(QTextStream &stream, const Identifier &id) {
  stream << id.toBase32();
  return stream;
}


/** Represents a peer (IP address + port) in the network.
 * @ingroup core */
class PeerItem
{
public:
  /** Empty constructor. */
  PeerItem();
  /** Constructor from address and port. */
  PeerItem(const QHostAddress &addr, uint16_t port);
  /** Copy constructor. */
  PeerItem(const PeerItem &other);
  /** Assignment operator. */
  PeerItem &operator=(const PeerItem &other);

  /** Returns the address of the peer. */
  const QHostAddress &addr() const;
  /** Returns the port of the peer. */
  uint16_t port() const;

protected:
  /** The address of the peer. */
  QHostAddress _addr;
  /** The port of the peer. */
  uint16_t     _port;
};


/** Represents a node (ID + IP address + port, or ID + Peer) in the network.
 * @ingroup core */
class NodeItem: public PeerItem
{
public:
  /** Empty constructor. */
  NodeItem();
  /** Constructor from ID, address and port. */
  NodeItem(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** Constructor from ID and peer. */
  NodeItem(const Identifier &id, const PeerItem &peer);
  /** Copy constructor. */
  NodeItem(const NodeItem &other);
  /** Assignment operator. */
  NodeItem &operator=(const NodeItem &other);

  /** Returns the identifier of the node. */
  const Identifier &id() const;

protected:
  /** The identifier of the node. */
  Identifier _id;
};


/** Represents an announcement made by another node.
 * @ingroup internal */
class AnnouncementItem: public PeerItem
{
public:
  /** Empty constructor. */
  AnnouncementItem();
  /** Constructor from @c addr and @c port. */
  AnnouncementItem(const QHostAddress &addr, uint16_t port);
  /** Copy constructor. */
  AnnouncementItem(const AnnouncementItem &other);
  /** Assignment operator. */
  AnnouncementItem &operator =(const AnnouncementItem &other);
  /** Returns true if the announcement is older than the given amount of seconds. */
  bool olderThan(size_t seconds);

protected:
  /** The timestamp of the announcement. */
  QDateTime _timestamp;
};


/** Represents a single k-bucket.
 * @ingroup internal */
class Bucket
{
public:
  /** An element of the @c Bucket. */
  class Item
  {
  public:
    /** Empty constructor. */
    Item();
    /** Constructor from identifier, address, port and prefix. */
    Item(const QHostAddress &addr, uint16_t port, size_t prefix, const QDateTime &lastSeen=QDateTime());
    /** Copy constructor. */
    Item(const Item &other);
    /** Assignment operator. */
    Item &operator=(const Item &other);

    /** Returns the precomputed @c prefix of the item w.r.t. the ID of the node. */
    size_t prefix() const;
    /** Retruns the address and port as a @c PeerItem. */
    const PeerItem &peer() const;
    /** The address of the item. */
    const QHostAddress &addr() const;
    /** The port of the item. */
    uint16_t port() const;
    /** The time of the item last seen. */
    const QDateTime &lastSeen() const;
    /** Returns true if the entry is older than the specified seconds. */
    inline bool olderThan(size_t seconds) const {
      if (! _lastSeen.isValid()) { return true; }
      return (_lastSeen.addSecs(seconds) < QDateTime::currentDateTime());
    }
    /** Returns the number of lost pings to this node. */
    inline size_t lostPings() const {
      return _lostPings;
    }
    /** Increments the ping-lost counter. */
    inline void pingLost() { _lostPings++; }

  protected:
    /** The prefix -- index of the leading bit of the difference between this identifier and the
     * identifier of the node. */
    size_t       _prefix;
    /** The address and port of the item. */
    PeerItem     _peer;
    /** The time, the item was last seen. */
    QDateTime    _lastSeen;
    /** The number of times, a ping request was not answered. */
    size_t       _lostPings;
  };

public:
  /** Constructor. */
  Bucket(const Identifier &self);
  /** Copy constructor. */
  Bucket(const Bucket &other);

  /** The the nodes that are closest to the given identifier. */
  void getNearest(const Identifier &id, QList<NodeItem> &best) const;
  /** Get all nodes that are older than the given age. */
  void getOlderThan(size_t age, QList<NodeItem> &nodes) const;
  /** Removes all nodes that are older than the given age. */
  void removeOlderThan(size_t age);
  /** Removes a single node specified by the given identifier. */
  void removeNode(const Identifier &id);

  /** Returns @c true if the bucket is full. */
  bool full() const;
  /** Returns the number of nodes held in the bucket. */
  size_t numNodes() const;
  /** Returns the list of all nodes held in the bucket. */
  void nodes(QList<NodeItem> &lst) const;
  /** Returns @c true if the bucket contains the given identifier. */
  bool contains(const Identifier &id) const;
  /** Returns the given node. */
  NodeItem getNode(const Identifier &id) const;
  /** Adds or updates an node. */
  bool add(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** Adds a candidate node. */
  void addCandidate(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** The prefix of the bucket. */
  size_t prefix() const;
  /** Increments the ping-loss counter of the node. */
  void pingLost(const Identifier &id);

  /** Splits the bucket at its prefix. Means all item with a higher prefix (smaller distance)
   * than the prefix of this bucket are moved to the new one. */
  void split(Bucket &newBucket);

protected:
  /** Adds an item. */
  bool add(const Identifier &id, const Item &item);

protected:
  /** Myself. */
  Identifier _self;
  /** The maximal bucket size. */
  size_t _maxSize;
  /** The prefix of the bucket. */
  size_t _prefix;
  /** Item table. */
  QHash<Identifier, Item> _triples;
};


/** A ordered list of buckets.
 * @ingroup internal */
class Buckets
{
public:
  /** Constructor. */
  Buckets(const Identifier &self);

  /** Returns @c true if the bucket list is empty. */
  bool empty() const;
  /** Returns true if the buckets contain the given node. */
  bool contains(const Identifier &id) const;
  /** Gets the specified node. */
  NodeItem getNode(const Identifier &id) const;
  /** Returns the number of nodes in the buckets. */
  size_t numNodes() const;
  /** Returns the list of all nodes in the buckets. */
  void nodes(QList<NodeItem> &lst) const;
  /** Collects the nearest known nodes. */
  void getNearest(const Identifier &id, QList<NodeItem> &best) const;

  /** Adds or updates an node. */
  bool add(const Identifier &id, const QHostAddress &addr, uint16_t port);
  /** Adds a candidate node. */
  void addCandidate(const Identifier &id, const QHostAddress &addr, uint16_t port);

  /** Collects all nodes that are "older" than the specified age (in seconds). */
  void getOlderThan(size_t seconds, QList<NodeItem> &nodes) const;
  /** Removes all nodes that are "older" than the specified age (in seconds). */
  void removeOlderThan(size_t seconds);
  /** Increment the ping-loss counter. */
  void pingLost(const Identifier &id);

protected:
  /** Returns the bucket index, an item should be searched for. */
  QList<Bucket>::iterator index(const Identifier &id);

protected:
  /** My identifier. */
  Identifier _self;
  /** The bucket list. */
  QList<Bucket> _buckets;
};


#endif // __OVL_BUCKETS_HH__
