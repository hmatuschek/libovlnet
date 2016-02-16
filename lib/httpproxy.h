#ifndef __OVLNET_HTTP_PROXY_H__
#define __OVLNET_HTTP_PROXY_H__

#include "httpservice.h"

/** A local HTTP proxy server into the OVL network.
 * @ingroup http */
class LocalHttpProxyServer: public LocalHttpServer
{
  Q_OBJECT

public:
  LocalHttpProxyServer(DHT &dht, uint16_t port=8080);
  virtual ~LocalHttpProxyServer();
};


/** A simple proxy request handler.
 * @ingroup http */
class LocalHttpProxyServerHandler: public HttpRequestHandler
{
  Q_OBJECT

public:
  /** Constructor. */
  LocalHttpProxyServerHandler(DHT &dht, QObject *parent=0);
  virtual ~LocalHttpProxyServerHandler();

  /** Accepts all requests. */
  bool acceptReqest(HttpRequest *request);
  /** Forwards the request. */
  HttpResponse *processRequest(HttpRequest *request);

protected:
  DHT &_dht;
};


/** A HttpResponse wrapping a HTTP connection to a node.
 * @ingroup http */
class LocalHttpProxyResponse: public HttpResponse
{
  Q_OBJECT

public:
  LocalHttpProxyResponse(DHT &dht, const Identifier &id, HttpRequest *request);

protected slots:
  void _onNodeFound(NodeItem item);
  void _onNodeNotFound(Identifier id, QList<NodeItem> near);
  void _onCloseLocal();
  void _onConnected();
  void _onParseResponse();
  void _onLocalReadyRead();

protected:
  typedef enum {
    PARSE_RESPONSE_CODE,
    PARSE_RESPONSE_HEADER,
    FORWARD_RESPONSE_BODY
  } ParserState;

protected:
  DHT &_dht;
  Identifier _destination;
  HttpRequest *_request;
  SecureStream *_stream;
  size_t _requestSize;
  ParserState _parserState;
  size_t _responseSize;
};


#endif // __OVLNET_HTTP_PROXY_H__
