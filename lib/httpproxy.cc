#include "httpproxy.hh"
#include "dht.hh"


/* ********************************************************************************************* *
 * Implementation of LocalHttpProxyServer
 * ********************************************************************************************* */
LocalHttpProxyServer::LocalHttpProxyServer(DHT &dht, uint16_t port)
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
LocalHttpProxyServerHandler::LocalHttpProxyServerHandler(DHT &dht, QObject *parent)
  : HttpRequestHandler(parent), _dht(dht)
{
  // pass...
}

LocalHttpProxyServerHandler::~LocalHttpProxyServerHandler() {
  // pass...
}

bool
LocalHttpProxyServerHandler::acceptReqest(HttpRequest *request) {
  if (! request->hasHeader("Host")) {
    logInfo() << "HttpProxyHandler: Request without a host!";
    return false;
  }
  logDebug() << "HTTP Proxy: Accecpt request for '" << request->header("Host") << "'.";
  return true;
}

HttpResponse *
LocalHttpProxyServerHandler::processRequest(HttpRequest *request) {
  // Dispatch by TLD
  if (request->header("Host").endsWith(".ovl")) {
    // if host domain is of form ID.ovl -> connect to ID's HTTP server
    QString id = request->header("Host").simplified();
    id.chop(4);
    return new LocalHttpProxyResponse(_dht, Identifier::fromBase32(id), request);
  }

  return new HttpStringResponse(HTTP_BAD_GATEWAY,
                                "<h1>Bad Gateway</h1> Do not support request outside of OVL net.",
                                request->connection());
}


/* ********************************************************************************************* *
 * Implementation of LocalHttpProxyResponse
 * ********************************************************************************************* */
LocalHttpProxyResponse::LocalHttpProxyResponse(DHT &dht, const Identifier &id, HttpRequest *request)
  : HttpResponse(HTTP_RESP_INCOMPLETE, request->connection()), _dht(dht), _destination(id), _request(request),
    _stream(0)
{
  connect(&_dht, SIGNAL(nodeFound(NodeItem)), this, SLOT(_onNodeFound(NodeItem)));
  connect(&_dht, SIGNAL(nodeNotFound(Identifier,QList<NodeItem>)),
          this, SLOT(_onNodeNotFound(Identifier,QList<NodeItem>)));
  logDebug() << "HTTP Proxy: Try to resolve " << _destination;
  _dht.findNode(_destination);
}

void
LocalHttpProxyResponse::_onNodeFound(NodeItem item) {
  // Skip if not searched for this node
  if (_destination != item.id()) { return; }
  // Connect to HTTP service at the node
  logDebug() << "HTTP Proxy: Found node " << item.id() << ". Connect to HTTP service.";

  _stream = new SecureStream(_dht, this);
  connect(_stream, SIGNAL(established()), this, SLOT(_onConnected()));
  _dht.startConnection(80, item, _stream);
}

void
LocalHttpProxyResponse::_onNodeNotFound(Identifier id, QList<NodeItem> near) {
  // Skip if not searched for this node
  if (_destination != id) { return; }
  logDebug() << "HTTP Proxy: Cannot resolve " << _destination;
  setResponseCode(HTTP_BAD_GATEWAY);
  connect(this, SIGNAL(headersSend()), this, SIGNAL(completed()));
  this->sendHeaders();
}

void
LocalHttpProxyResponse::_onCloseLocal() {
  // pass...
}

void
LocalHttpProxyResponse::_onConnected() {
  logDebug() << "HTTP Proxy: Connected to " << _destination << ": Forward request.";
  // Forward request
  switch(_request->method()) {
    case HTTP_GET: _stream->write("GET "); break;
    case HTTP_HEAD: _stream->write("HEAD "); break;
    case HTTP_POST: _stream->write("POST "); break;
    case HTTP_INVALID_METHOD: break; // <- Already handled
  }
  _stream->write(_request->path().toUtf8());
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
    connect(_request->connection()->socket(), SIGNAL(readyRead()),
            this, SLOT(_onLocalReadyRead()));
  }
  // Parse response
  _parserState = PARSE_RESPONSE_CODE;
  connect(_stream, SIGNAL(readyRead()), this, SLOT(_onParseResponse()));
}

void
LocalHttpProxyResponse::_onLocalReadyRead() {
  logDebug() << "HTTP Proxy: Forward request body...";
  QByteArray buffer = _request->connection()->socket()->read(
        std::min(size_t(0xffff), _requestSize));
  _requestSize -= buffer.size();
  _stream->write(buffer);
  if (0 == _requestSize) {
    disconnect(_request->connection()->socket(), SIGNAL(readyRead()),
               this, SLOT(_onLocalReadyRead()));
  }
}

void
LocalHttpProxyResponse::_onParseResponse() {
  while (_stream->bytesAvailable()) {
    if (PARSE_RESPONSE_CODE == _parserState) {
      if (!_stream->canReadLine()) { return; }
      QByteArray line = _stream->readLine();
      _request->connection()->socket()->write(line);
      _parserState = PARSE_RESPONSE_HEADER;
    } else if (PARSE_RESPONSE_HEADER == _parserState) {
      if (!_stream->canReadLine()) { return; }
      QByteArray line = _stream->readLine();
      _request->connection()->socket()->write(line);
      if ("\r\n" == line) {
        _parserState = FORWARD_RESPONSE_BODY;
      } else if (line.startsWith("Content-Length:")) {
        _responseSize = line.mid(15).toUInt();
      }
    } else if (FORWARD_RESPONSE_BODY == _parserState) {
      QByteArray buffer = _stream->read(std::min(size_t(0xffff), _responseSize));
      _request->connection()->socket()->write(buffer);
      _responseSize -= buffer.size();
      if (0 == _responseSize) {
        _parserState = PARSE_RESPONSE_CODE;
        emit completed();
        return;
      }
    }
  }
}
