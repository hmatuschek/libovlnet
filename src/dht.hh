#ifndef __OVL_DHT_HH__
#define __OVL_DHT_HH__

#include "subnetwork.hh"
#include "httpservice.hh"
#include "httpclient.hh"

class DHTAnnounceQuery: public QObject
{
  Q_OBJECT

public:
  DHTAnnounceQuery(Network &net, const Identifier &item);

protected slots:
  void _onNeighboursFound(const Identifier &id, const QList<NodeItem> &nodes);
  void _onNeighbourNotified();
  void _onError();

signals:
  void announced(const Identifier &id);

protected:
  Network &_network;
  Identifier _item;
  size_t _numQueries;
};


class DHT: public QObject
{
  Q_OBJECT

protected:
  class NodeRef: public NodeItem {
  public:
    NodeRef();
    explicit NodeRef(const NodeItem &node);
    NodeRef(const NodeRef &other);
    NodeRef &operator =(const NodeRef &other);
    bool isOlderThan(size_t sec) const;
  protected:
    QDateTime _timestamp;
  };

  class NodeRefTable: public QHash<Identifier, NodeRef> {
  public:
    NodeRefTable();
    NodeRefTable(const NodeRefTable &other);

    NodeRefTable &operator= (const NodeRefTable &other);

    void add(const NodeItem &node);
    void removeOlderThan(size_t sec);
    void get(QList<NodeItem> &nodes) const;
  };

public:
  DHT(SubNetwork &subnet, QObject *parent=0);

  SubNetwork &subnet();

  void addAnnouncement(const Identifier &item);

signals:
  void itemFound(const Identifier &id, const QList<NodeItem> &nodes);

protected slots:
  void membersFoundEvent(const Identifier &id, const QList<NodeItem> &nodes);
  void itemFoundEvent(const Identifier &id, const QList<NodeItem> &nodes);

protected:
  bool isResponsible(const Identifier &item) const;
  bool hasNodesFor(const Identifier &item) const;
  void nodesFor(const Identifier &item, QList<NodeItem> &nodes) const;
  void addNodeFor(const Identifier &item, const NodeItem &node);
  void announce(const Identifier &item);

protected:
  SubNetwork &_subnet;
  QHash<Identifier, QDateTime> _announcements;
  QHash<Identifier, NodeRefTable> _table;

  friend class DHTSearchHandler;
  friend class DHTSearchRequest;
};


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

#endif // __OVL_DHT_HH__
