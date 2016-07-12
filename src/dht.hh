/** @defgroup dht Distributed hash table service.
 * The DHT service is implemented as a HTTP service named @c NETPREFIX::dht. A @c GET request
 * to @c /ITEMID will either return a list of nodes associated with the given item or
 * a list of nodes which are closer to the item. An empty @c POST request to @c /ITEMID
 * will announce the item to the node. The later announcement might be ignored by the node if it
 * is not responsible for the item. That is, if it "knows" more than @c OVL_K nodes which are
 * closer to the item.
 *
 * Please note that the DHT service only implements how items are located within a subnetwork. It
 * does not implement how these items are exchanged between nodes. Moreover, in contrast to many
 * other services, the DHT service cannot be registered with the root network. For the DHT to work
 * properly, one has to assume that all participating nodes provide the DHT service. As one cannot
 * assume that for all nodes in the root network, the DHT service can only be registered with
 * a subnetwork.
 * @ingroup services */

#ifndef __OVL_DHT_HH__
#define __OVL_DHT_HH__

#include "subnetwork.hh"
#include "httpservice.hh"
#include "httpclient.hh"


/** Implements a distributed hash table. */
class DHT: public QObject
{
  Q_OBJECT

protected:
  /** Implements the reference to a node for an item. */
  class NodeRef: public NodeItem {
  public:
    /** Empty constructor. */
    NodeRef();
    /** Constructs a node reference item from the given node item. */
    explicit NodeRef(const NodeItem &node);
    /** Copy constructor. */
    NodeRef(const NodeRef &other);
    /** Assignment operator. */
    NodeRef &operator =(const NodeRef &other);
    /** Returns @c true if the node reference is older than the given number of seconds. */
    bool isOlderThan(size_t sec) const;
  protected:
    /** The timestamp of the node reference. */
    QDateTime _timestamp;
  };

  /** Implements a node reference table. That is a list of nodes associated with an item. */
  class NodeRefTable: public QHash<Identifier, NodeRef> {
  public:
    /** Constructor. */
    NodeRefTable();
    /** Copy constructor. */
    NodeRefTable(const NodeRefTable &other);
    /** Assignment operator. */
    NodeRefTable &operator= (const NodeRefTable &other);
    /** Adds or updates a node. */
    void add(const NodeItem &node);
    /** Removes all node references which are older than the specified number of seconds. */
    void removeOlderThan(size_t sec);
    /** Returns a list of nodes. */
    void get(QList<NodeItem> &nodes) const;
  };

public:
  /** Constructs a DHT service for the specified subnetwork.
   * The constructor also registeres the servic handlers with the subnetwork to provide the
   * DHT service.
   * @param subnet Specifies the subnetwork to add a DHT service to.
   * @param parent Specifies the optional QObject parent. */
  DHT(SubNetwork &subnet, QObject *parent=0);

  /** Returns the subnetwork. */
  SubNetwork &subnet();
  /** Adds an announcement item. This item will then be announced regularily to the neighbours of
   * the item. */
  void addAnnouncement(const Identifier &item);

signals:
  /** Gets emitted if the given item identifier can be found at the given nodes. */
  void itemFound(const Identifier &id, const QList<NodeItem> &nodes);

protected slots:  
  void membersFoundEvent(const Identifier &id, const QList<NodeItem> &nodes);
  void itemFoundEvent(const Identifier &id, const QList<NodeItem> &nodes);

protected:
  /** Helper function returning @c true if the node is responsible for the given item. */
  bool isResponsible(const Identifier &item) const;
  /** Helper function returning @c true if the DHT has some nodes associated with the given item. */
  bool hasNodesFor(const Identifier &item) const;
  /** Returns the nodes associated with the given item. */
  void nodesFor(const Identifier &item, QList<NodeItem> &nodes) const;
  /** Associate a node with some item. */
  void addNodeFor(const Identifier &item, const NodeItem &node);
  /** Announce item. */
  void announce(const Identifier &item);

protected:
  /** Holds a weak reference to the subnetwork. */
  SubNetwork &_subnet;
  /** Holds a list of announcements to make. */
  QHash<Identifier, QDateTime> _announcements;
  /** Holds the list of nodes associated with the items, this instance is responsible for. */
  QHash<Identifier, NodeRefTable> _table;

  friend class DHTSearchHandler;
  friend class DHTSearchRequest;
};


/** Implements a DHT search handler. */
class DHTSearchHandler: public HttpRequestHandler
{
public:
  DHTSearchHandler(DHT &dht);
  virtual ~DHTSearchHandler();

  bool acceptReqest(HttpRequest *request);
  HttpResponse *processRequest(HttpRequest *request);

protected:
  HttpResponse *_processSearch(const Identifier &item, HttpRequest *request);
  HttpResponse *_processAnnouncement(const Identifier &item, HttpRequest *request);

protected:
  DHT &_dht;
};


class DHTSearchRequest: public JsonQuery
{
  Q_OBJECT

public:
  DHTSearchRequest(DHT &dht, const NodeItem &remote, SearchQuery *query);

  SearchQuery *query();

protected slots:
  bool accept();
  void finished(const QJsonDocument &doc);
  void error();

protected:
  DHT &_dht;
  SearchQuery *_queryobj;
  bool _redirect;
};


/** Implements a self-destructing query object to first search for the neighbourhood of the item
 * to announce. Followed by announcements queries to each node in the neighbourhood of the item.
 * @ingroup dht */
class DHTAnnounceQuery: public QObject
{
  Q_OBJECT

public:
  /** Constructs a new DHT announcement query.
   * @param net Specifies the network in which the item should be announced.
   * @param item The identifier of the item to announce. */
  DHTAnnounceQuery(Network &net, const Identifier &item);

protected slots:
  /** Gets called once the neighbours of the item are found. */
  void _onNeighboursFound(const Identifier &id, const QList<NodeItem> &nodes);
  /** Gets called everytime the item has been announced to a neighbour. */
  void _onNeighbourNotified();
  /** Gets called on errors. */
  void _onError();

signals:
  /** Gets emitted once the item has been announced. */
  void announced(const Identifier &id);

protected:
  /** A weak reference to the network in which the item gets announced. */
  Network &_network;
  /** The identifier of the item to announce. */
  Identifier _item;
  /** The query counter. */
  size_t _numQueries;
};


#endif // __OVL_DHT_HH__
