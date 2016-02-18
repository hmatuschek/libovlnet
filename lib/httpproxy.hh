#ifndef __OVLNET_HTTP_PROXY_H__
#define __OVLNET_HTTP_PROXY_H__

#include "httpservice.hh"

/** A local HTTP proxy server into the OVL network.
 * @ingroup http */
class LocalHttpProxyServer: public LocalHttpServer
{
  Q_OBJECT

public:
  /** Constructor
   * @param dht Specifies the OVL node instance.
   * @param port Specifies the local TCP port to listen on (default 8080). */
  LocalHttpProxyServer(Node &dht, uint16_t port=8080);
  /** Destructor. */
  virtual ~LocalHttpProxyServer();
};


/** A simple proxy request handler.
 * @ingroup http */
class LocalHttpProxyServerHandler: public HttpRequestHandler
{
  Q_OBJECT

public:
  /** Constructor.
   * @param dht Specifies the OVL node instance.
   * @param parent Specifies the optional QObject parent. */
  LocalHttpProxyServerHandler(Node &dht, QObject *parent=0);
  /** Destructor. */
  virtual ~LocalHttpProxyServerHandler();

  /** Accepts all requests. */
  bool acceptReqest(HttpRequest *request);
  /** Forwards the request. */
  HttpResponse *processRequest(HttpRequest *request);

protected:
  /** A weak reference to the DHT node. */
  Node &_dht;
};


/** A HttpResponse wrapping a HTTP connection to a node.
 * @ingroup http */
class LocalHttpProxyResponse: public HttpResponse
{
  Q_OBJECT

public:
  /** Constructor.
   * @param dht Specifies the OVL node instance.
   * @param id Specifeis the hostname to connect to.
   * @param request Specifies the request this is a response to. */
  LocalHttpProxyResponse(Node &dht, const HostName &id, HttpRequest *request);

protected slots:
  /** Gets called if the remote node has been found. */
  void _onNodeFound(NodeItem item);
  /** Gets called if the remote node cannot be found. */
  void _onNodeNotFound(Identifier id, QList<NodeItem> near);
  /** Gets called if the remote host cannot be connected. */
  void _onTcpError(QAbstractSocket::SocketError error);
  /** Gets called if the remote node cannot be connected. */
  void _onConnectionError();
  /** Gets called if the rendezvous was successful. */
  void _onRendezvousInitiated(const NodeItem &node);
  /** Gets called if the rendezvous failed. */
  void _onRendezvousFailed(const Identifier &id);
  /** Gets called on error. */
  void _onError();
  /** Gets called on successful connection. */
  void _onConnected();
  /** Parses the response from the remote node or host. */
  void _onParseResponse();
  /** Receives data from the local client. */
  void _onLocalReadyRead();
  /** Gets called while final data is send. */
  void _onResponseFinishing(qint64 len);


protected:
  /** Possible states of the response parser. */
  typedef enum {
    /** Parse the response code. */
    PARSE_RESPONSE_CODE,
    /** Parse response headers. */
    PARSE_RESPONSE_HEADER,
    /** Forward response body. */
    FORWARD_RESPONSE_BODY
  } ParserState;

protected:
  /** A weak reference to the OVL node. */
  Node &_dht;
  /** The remote node or host. */
  HostName _destination;
  /** The current client request. */
  HttpRequest *_request;
  /** The connection to the remote host or node. */
  QIODevice *_stream;
  /** The size of the request body. */
  size_t _requestSize;
  /** The response parser state. */
  ParserState _parserState;
  /** The size of the response body. */
  size_t _responseSize;
};


#endif // __OVLNET_HTTP_PROXY_H__
