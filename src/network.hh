#ifndef NETWORK_HH
#define NETWORK_HH

#include <QObject>
#include <QTimer>
#include "buckets.hh"

/* Forward declarations. */
class Node;
class AbstractService;


/** Base class of all search queries.
 * Search queryies are used to keep track of nodes and values that can be found in the OVL
 * network. */
class SearchQuery: public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * @param id Specifies the identifier to search for.
   * @param prefix Specifies the network prefix to search in. */
  SearchQuery(const Identifier &id, const QString &prefix);

  /** Destructor. */
  virtual ~SearchQuery();

  /** Ignore the following node ID. */
  void ignore(const Identifier &id);

  /** Returns the identifier of the element being searched for. */
  const Identifier &id() const;
  /** Returns the network identifier. */
  const Identifier &netid() const;

  /** Update the search queue (ordered list of nodes to query). */
  virtual void update(const NodeItem &nodes);

  /** Returns the next node to query or @c false if no node left to query. */
  virtual bool next(NodeItem &node);

  /** Returns the current search query. This list is also the list of the closest nodes to the
   * target known. */
  QList<NodeItem> &best();
  /** Returns the current search query. This list is also the list of the closest nodes to the
   * target known. */
  const QList<NodeItem> &best() const;

  /** Returns the first element from the search queue. */
  const NodeItem &first() const;

  /** Returns true, if the search is complete. */
  virtual bool isSearchComplete() const = 0;

  /** Gets called if there is no progress in search or if @c isSearchComplete returned @c true. */
  virtual void searchCompleted() = 0;

  /** Should be called if the search query succeeds.
   * This will delete the search query instance. */
  virtual void searchSucceeded();

  /** Gets called if the search query failed.
   * This will delete the search query instance. */
  virtual void searchFailed();

signals:
  /** Gets emitted if the search succeeded or failed. */
  void completed(const Identifier &id, const QList<NodeItem> &best);
  /** Gets emitted if the search failed. */
  void failed(const Identifier &id, const QList<NodeItem> &best);
  /** Gets emitted if the search succeeded. */
  void succeeded(const Identifier &id, const QList<NodeItem> &best);

protected:
  /** The identifier of the element being searched for. */
  Identifier _id;
  /** The identifier or prefix of the subnetwork to search. */
  Identifier _prefix;
  /** The current search queue. */
  QList<NodeItem> _best;
  /** The set of nodes already asked. */
  QSet<Identifier> _queried;
};


/** A specialized @c SearchQuery that resolves a node identifier. */
class FindNodeQuery: public SearchQuery
{
  Q_OBJECT

public:
  /** Constructs a node search query.
   * @param id Specifies the node identifier.
   * @param prefix Specifies the network prefix to search in. */
  FindNodeQuery(const Identifier &id, const QString prefix="");
  /** Destructor. */
  virtual ~FindNodeQuery();

  bool isSearchComplete() const;
  void searchCompleted();
  void searchSucceeded();

signals:
  /** Gets emitted if the node has been found. */
  void found(const NodeItem &node);
};


/** A @c SearchQuery specialization searching for the neighbourhood of a node or any other
 * identifier. In contrast to the @c FindNodeQuery, this query continues until the neighbourhood
 * of an identifier has be found. */
class NeighbourhoodQuery: public SearchQuery
{
  Q_OBJECT

public:
  /** Constructs a neighbourhood search query.
   * @param id Specifies the identifier to search for.
   * @param prefix Specifies the network prefix to search in. */
  NeighbourhoodQuery(const Identifier &id, const QString prefix="");
  /** Destructor. */
  virtual ~NeighbourhoodQuery();

  bool isSearchComplete() const;
  void searchCompleted();
};


/** Base class of all networks (root network as implemented by @c Node)
 * and sub-networks implemented by @c SubNetwork. */
class Network : public QObject
{
  Q_OBJECT

public:
  /** Constructs a network.
   * @param id Specifies the identifier of the node.
   * @param parent Specifies the QObject parent. */
  explicit Network(const Identifier &id, QObject *parent = 0);

  /** Returns a weak reference to the root network node. */
  virtual Node &root() = 0;

  /** Returns the network prefix (name). Returns an empty string for the roo network. */
  virtual const QString &prefix() const = 0;
  /** Returns the network identifier. */
  virtual Identifier netid() const;

  /** Returns @c true if a handler is associated with the given service name. */
  virtual bool hasService(const QString &name) const = 0;
  /** Registers a service.
   * Returns @c true on success and @c false if a handler is already associated with the given
   * service. The ownership of the handler is transferred to the DHT. */
  virtual bool registerService(const QString& service, AbstractService *handler)=0;

  /** Sends a ping to the given node. */
  virtual void ping(const NodeItem &node) = 0;
  /** Starts a search query (e.g., @c FindNodeQuery). */
  virtual void search(SearchQuery *query) = 0;
  /** Returns the nearest neighbours within this network from the buckets. */
  void getNearest(const Identifier &id, QList<NodeItem> &nodes) const;

public slots:
  /** Adds a candidate node for the network. */
  void addCandidate(const NodeItem &node);

protected:
  /** Gets called once a node replied to a ping request within this network. */
  virtual void nodeReachableEvent(const NodeItem &node);

signals:
  /** Gets emitted as the Node enters the network. */
  void connected();
  /** Gets emitted as the Node leaves the network. */
  void disconnected();
  /** Gets emitted if a node leaves the buckets. */
  void nodeLost(const Identifier &id);
  /** Gets emitted if a node enters the buckets. */
  void nodeAppeard(const NodeItem &node);
  /** Gets emitted if a ping was replied. */
  void nodeReachable(const NodeItem &node);

protected slots:
  /** Gets called periodically to check for any unreachable node in the buckets. */
  void checkNodes();

protected:
  /** The buckets of nodes for this network. */
  Buckets _buckets;
  /** Bucket update timer. */
  QTimer _nodeTimer;

  friend class Node;
};

#endif // NETWORK_HH
