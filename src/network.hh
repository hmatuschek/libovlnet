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
  /** Constructor. */
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
  void completed(const Identifier &id, const QList<NodeItem> &best);
  void failed(const Identifier &id, const QList<NodeItem> &best);
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


class FindNodeQuery: public SearchQuery
{
  Q_OBJECT

public:
  FindNodeQuery(const Identifier &id, const QString prefix="");
  virtual ~FindNodeQuery();

  bool isSearchComplete() const;
  void searchCompleted();
  void searchSucceeded();

signals:
  void found(const NodeItem &node);
};


class NeighbourhoodQuery: public SearchQuery
{
  Q_OBJECT

public:
  NeighbourhoodQuery(const Identifier &id, const QString prefix="");
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
  explicit Network(const Identifier &id, QObject *parent = 0);

  /** Returns a weak reference to the root network node. */
  virtual Node &root() = 0;

  virtual const QString &prefix() const = 0;
  virtual Identifier netid() const;

  /** Returns @c true if a handler is associated with the given service name. */
  virtual bool hasService(const QString &name) const = 0;
  /** Registers a service.
   * Returns @c true on success and @c false if a handler is already associated with the given
   * service. The ownership of the handler is transferred to the DHT. */
  virtual bool registerService(const QString& service, AbstractService *handler)=0;

  virtual void ping(const NodeItem &node) = 0;
  virtual void search(SearchQuery *query) = 0;
  void getNearest(const Identifier &id, QList<NodeItem> &nodes) const;

public slots:
  void addCandidate(const NodeItem &node);

protected:
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
  void checkNodes();

protected:
  Buckets _buckets;
  QTimer _nodeTimer;

  friend class Node;
};

#endif // NETWORK_HH
