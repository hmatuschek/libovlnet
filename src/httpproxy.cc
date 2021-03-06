#include "httpproxy.hh"
#include "node.hh"
#include <QHostInfo>
#include <QTcpSocket>


/* ********************************************************************************************* *
 * Implementation of LocalHttpProxyServer
 * ********************************************************************************************* */
LocalHttpProxyServer::LocalHttpProxyServer(Node &dht, uint16_t port)
  : LocalHttpServer(new LocalHttpProxyServerHandler(dht), port)
{
  // pass...
}

LocalHttpProxyServer::~LocalHttpProxyServer() {
  // pass...
}


/* ********************************************************************************************* *
 * Implementation of LocalHttpProxyServerHandler
 * ********************************************************************************************* */
LocalHttpProxyServerHandler::LocalHttpProxyServerHandler(Node &dht)
  : HttpRequestHandler(), _dht(dht)
{
  // pass...
}

LocalHttpProxyServerHandler::~LocalHttpProxyServerHandler() {
  // pass...
}

bool
LocalHttpProxyServerHandler::acceptReqest(HttpRequest *request) {
  if (request->hasHeader("Host")) {
    logDebug() << "HTTP Proxy: Accecpt request for '" << request->header("Host") << "'.";
    return true;
  }
  logInfo() << "HttpProxyHandler: Neglect request.";
  return false;
}

HttpResponse *
LocalHttpProxyServerHandler::processRequest(HttpRequest *request) {
  // Get Host
  HostName host(request->header("Host"));
  return new LocalHttpProxyResponse(_dht, host, request);
}


/* ********************************************************************************************* *
 * Implementation of LocalHttpProxyResponse
 * ********************************************************************************************* */
LocalHttpProxyResponse::LocalHttpProxyResponse(Node &dht, const HostName &id, HttpRequest *request)
  : HttpResponse(request->version(), HTTP_RESP_INCOMPLETE, request->socket()),
    _dht(dht), _destination(id), _request(request), _stream(0)
{  
  if (_destination.isOvlNode()) {
    // If destination is a OVL node "domain".
    connect(&_dht, SIGNAL(nodeFound(NodeItem)), this, SLOT(_onNodeFound(NodeItem)));
    connect(&_dht, SIGNAL(nodeNotFound(Identifier,QList<NodeItem>)),
            this, SLOT(_onNodeNotFound(Identifier,QList<NodeItem>)));
    _dht.search(new FindNodeQuery(_destination.ovlId()));
  } else {
    // Otherwise assume "normal" domain name.
    QTcpSocket *socket = new QTcpSocket(this); _stream = socket;
    connect(socket, SIGNAL(connected()), this, SLOT(_onConnected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(_onTcpError(QAbstractSocket::SocketError)));
    socket->connectToHost(_destination.name(), _destination.port());
  }
}

void
LocalHttpProxyResponse::_onNodeFound(NodeItem item) {
  // Skip if not searched for this node
  if (_destination.ovlId() != item.id()) { return; }
  // Connect to HTTP service at the node
  logDebug() << "HTTP Proxy: Found node " << item.id() << ".";

  SecureStream *socket = new SecureStream(_dht, this); _stream = socket;
  connect(socket, SIGNAL(established()), this, SLOT(_onConnected()));
  connect(socket, SIGNAL(error()), this, SLOT(_onConnectionError()));
  _dht.startConnection("::http", item, socket);
}

void
LocalHttpProxyResponse::_onNodeNotFound(Identifier id, QList<NodeItem> near) {
  // Skip if not searched for this node
  if (_destination.ovlId() != id) { return; }
  logDebug() << "HTTP Proxy: Cannot resolve " << _destination.ovlId();
  _onError();
}

void
LocalHttpProxyResponse::_onTcpError(QAbstractSocket::SocketError error) {
  logDebug() << "HTTP Proxy: TCP error: " << error;
  _onError();
}

void
LocalHttpProxyResponse::_onConnectionError() {
  logDebug() << "Cannot connect to node. Try rendezvous";
  delete _stream; _stream=0;
  connect(&_dht, SIGNAL(rendezvousInitiated(NodeItem)), this, SLOT(_onRendezvousInitiated(NodeItem)));
  connect(&_dht, SIGNAL(rendezvousFailed(Identifier)), this, SLOT(_onRendezvousFailed(Identifier)));
  _dht.rendezvous(_destination.ovlId());
}

void
LocalHttpProxyResponse::_onRendezvousInitiated(const NodeItem &node) {
  if (node.id() != _destination.ovlId()) { return; }
  logDebug() << "Rendezvous initiated. Retry connection.";
  SecureStream *socket = new SecureStream(_dht, this); _stream = socket;
  connect(socket, SIGNAL(established()), this, SLOT(_onConnected()));
  connect(socket, SIGNAL(error()), this, SLOT(_onError()));
  _dht.startConnection("::http", node, socket);
}

void
LocalHttpProxyResponse::_onRendezvousFailed(const Identifier &id) {
  if (id != _destination.ovlId()) { return; }
  logDebug() << "Rendezvous failed.";
  _onError();
}

void
LocalHttpProxyResponse::_onError() {
  logDebug() << "HTTP Proxy: Error: Send short response.";
  setResponseCode(HTTP_BAD_GATEWAY);
  connect(this, SIGNAL(headersSend()), this, SIGNAL(completed()));
  this->sendHeaders();
}

void
LocalHttpProxyResponse::_onConnected() {
  logDebug() << "HTTP Proxy: Connected to " << _destination.name() << ":" << _destination.port()
             << ": Forward request.";
  // Forward request
  switch(_request->method()) {
    case HTTP_GET: _stream->write("GET "); break;
    case HTTP_HEAD: _stream->write("HEAD "); break;
    case HTTP_POST: _stream->write("POST "); break;
    case HTTP_INVALID_METHOD: break; // <- Already handled
  }
  _stream->write(_request->uri().path().toUtf8());
  _stream->write(" ");
  switch(_request->version()) {
    case HTTP_1_0: _stream->write("HTTP/1.0\r\n"); break;
    case HTTP_1_1: _stream->write("HTTP/1.1\r\n"); break;
    case HTTP_INVALID_VERSION: break; // <- Already handled.
  }
  // Serialize headers:
  HttpRequest::iterator header = _request->begin();
  for (; header!=_request->end(); header++) {
    _stream->write(header.key().toUtf8());
    _stream->write(": ");
    _stream->write(header.value().toUtf8());
    _stream->write("\r\n");
  }
  _stream->write("\r\n");

  // If method == POST -> forward data
  if ((HTTP_POST == _request->method()) && _request->hasHeader("Content-Length")) {
    _requestSize = _request->header("Content-Length").toUInt();
    connect(_request->socket(), SIGNAL(readyRead()),
            this, SLOT(_onLocalReadyRead()));
  }
  // Parse response
  _parserState = PARSE_RESPONSE_CODE;
  connect(_stream, SIGNAL(readyRead()), this, SLOT(_onParseResponse()));
  _onParseResponse();
}

void
LocalHttpProxyResponse::_onLocalReadyRead() {
  QByteArray buffer = _request->socket()->read(
        std::min(size_t(0xffff), _requestSize));
  _requestSize -= buffer.size();
  _stream->write(buffer);
  if (0 == _requestSize) {
    disconnect(_request->socket(), SIGNAL(readyRead()),
               this, SLOT(_onLocalReadyRead()));
  }
}

void
LocalHttpProxyResponse::_onParseResponse() {
  while (_stream->bytesAvailable()) {
    if (PARSE_RESPONSE_CODE == _parserState) {
      if (!_stream->canReadLine()) { return; }
      QByteArray line = _stream->readLine();
      _request->socket()->write(line);
      _parserState = PARSE_RESPONSE_HEADER;
    } else if (PARSE_RESPONSE_HEADER == _parserState) {
      if (!_stream->canReadLine()) { return; }
      QByteArray line = _stream->readLine();
      _request->socket()->write(line);
      if ("\r\n" == line) {
        _parserState = FORWARD_RESPONSE_BODY;
      } else if (line.startsWith("Content-Length: ")) {
        _responseSize = line.mid(16, line.size()-18).toUInt();
      }
    } else if (FORWARD_RESPONSE_BODY == _parserState) {
      QByteArray buffer = _stream->read(std::min(size_t(0xffff), _responseSize));
      qint64 len = _request->socket()->write(buffer);
      if (len != buffer.size()) {
        logError() << "Cannot send " << buffer.size() << "b to client.";
      } else {
        logError() << "Send " << buffer.size() << "b to client.";
      }
      _responseSize -= buffer.size();
      if (0 == _responseSize) {
        _parserState = PARSE_RESPONSE_CODE;
          logDebug() << "Response send. Close connection";
          _socket->close();
          emit completed();
      }
      return;
    }
  }
}

void
LocalHttpProxyResponse::_onResponseFinishing(qint64 len) {
  if (0 == _socket->bytesToWrite()) {
    _socket->close();
    emit completed();
  }
}
