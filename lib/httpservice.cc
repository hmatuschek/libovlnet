#include "httpservice.hh"
#include "logger.hh"
#include "stream.hh"
#include "buckets.hh"
#include "dht.hh"

#include <QTcpSocket>


/* ********************************************************************************************* *
 * Implementation of HTTPRequest
 * ********************************************************************************************* */
HttpRequest::HttpRequest(HttpConnection *connection)
  : QObject(connection), _connection(connection), _parserState(READ_REQUEST), _headers()
{
  // pass..
}

void
HttpRequest::parse() {
  connect(_connection->socket(), SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
  _onReadyRead();
}

void
HttpRequest::_onReadyRead() {
  // Check if a line can be read from the device:
  while (_connection->socket()->canReadLine()) {
    if (READ_REQUEST == _parserState) {
      QByteArray line = _connection->socket()->readLine();
      if (! line.endsWith("\r\n")) {
        // Invalid format
        emit badRequest();
        return;
      }
      // drop last two chars ("\r\n")
      line.chop(2);

      // Parse HTTP request
      int offset = 0, idx = 0;
      // Read method
      if (0 > (idx = line.indexOf(' ', offset))) {
        // Invalid format
        emit badRequest();
        return;
      }
      _method = _getMethod(line, idx);
      // check method
      if (HTTP_INVALID_METHOD == _method) {
        // Invalid method
        emit badRequest();
        return;
      }
      // Update offset
      offset = idx+1;

      // Read path
      if (0 > (idx = line.indexOf(' ', offset))) {
        // Invalid format
        emit badRequest();
        return;
      }
      _path = QString::fromLatin1(line.constData()+offset, idx-offset);
      offset = idx+1;

      // Extract version
      _version = _getVersion(line.constData()+offset, line.size()-offset);
      // Check version
      if (HTTP_INVALID_VERSION == _version) {
        // Invalid format
        emit badRequest();
        return;
      }
      // done
      _parserState = READ_HEADER;
    } else if (READ_HEADER == _parserState) {
      QByteArray line = _connection->socket()->readLine();
      if (!line.endsWith("\r\n")) {
        // Invalid format
        emit badRequest();
        return;
      }
      // drop last two chars ("\r\n")
      line.chop(2);

      // Check if end-of-headers is reached
      if (0 == line.size()) {
        // done, disconnect from readyRead signal. Any additional data will be processed by the
        // request handler or by the next request
        disconnect(_connection->socket(), SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
        emit headerRead();
        return;
      }
      // Seach for first colon
      int idx=0;
      if (0 > (idx = line.indexOf(':'))) {
        // Invalid format
        emit badRequest();
        return;
      }
      _headers.insert(QString::fromLatin1(line.constData(), idx),
                      QString::fromLatin1(line.constData()+idx+1, line.length()-idx-1));
    } else {
      return;
    }
  }
}


HttpMethod
HttpRequest::_getMethod(const char *str, int len) {
  if ((3 == len) && (0 == strncmp(str, "GET", 3))) { return HTTP_GET; }
  if ((4 == len) && (0 == strncmp(str, "HEAD", 4))) { return HTTP_HEAD; }
  if ((4 == len) && (0 == strncmp(str, "POST", 4))) { return HTTP_POST; }
  return HTTP_INVALID_METHOD;
}

HttpVersion
HttpRequest::_getVersion(const char *str, int len) {
  if ((8==len) && strncmp(str, "HTTP/1.0", 8)) { return HTTP_1_0; }
  if ((8==len) && strncmp(str, "HTTP/1.1", 8)) { return HTTP_1_1; }
  return HTTP_INVALID_VERSION;
}


/* ********************************************************************************************* *
 * Implementation of HttpResponse
 * ********************************************************************************************* */
HttpResponse::HttpResponse(HttpVersion version, HttpResponseCode resp, HttpConnection *connection)
  : QObject(connection), _connection(connection), _version(version), _code(resp),
    _headersSend(false), _headerSendIdx(0), _headers()
{
  // Connect to bytesWritten signal
  connect(_connection->socket(), SIGNAL(bytesWritten(qint64)),
          this, SLOT(_onBytesWritten(qint64)));
}

void
HttpResponse::sendHeaders() {
  // Skip if headers are serialized already or even has been send
  if (_headersSend || (0 != _headerBuffer.size()) || (HTTP_RESP_INCOMPLETE == _code)) { return; }
  // Send version
  switch (_version) {
    case HTTP_1_0: _headerBuffer.append("HTTP/1.0 "); break;
    case HTTP_1_1: _headerBuffer.append("HTTP/1.1 "); break;
    case HTTP_INVALID_VERSION: break;
  }
  // Dispatch by response type
  switch (_code) {
    case HTTP_RESP_INCOMPLETE: return;
    case HTTP_OK: _headerBuffer.append("200 OK\r\n"); break;
    case HTTP_BAD_REQUEST: _headerBuffer.append("400 BAD REQUEST\r\n"); break;
    case HTTP_FORBIDDEN: _headerBuffer.append("403 FORBIDDEN\r\n"); break;
    case HTTP_NOT_FOUND: _headerBuffer.append("404 NOT FOUND\r\n"); break;
    case HTTP_SERVER_ERROR: _headerBuffer.append("500 Internal Server error\r\n"); break;
    case HTTP_BAD_GATEWAY: _headerBuffer.append("502 Bad Gateway\r\n"); break;
  }
  // Serialize headers
  QHash<QString, QString>::iterator header = _headers.begin();
  for (; header != _headers.end(); header++) {
    _headerBuffer.append(header.key());
    _headerBuffer.append(": ");
    _headerBuffer.append(header.value());
    _headerBuffer.append("\r\n");
  }
  _headerBuffer.append("\r\n");

  // Try to send some part of the serialized response
  qint64 len = _connection->socket()->write(_headerBuffer);
  if (len > 0) { _headerSendIdx = len; }
}

void
HttpResponse::_onBytesWritten(qint64 bytes) {
  logDebug() << "Continue send headers.";
  // If headers already send -> done
  if (_headersSend) { return; }
  // If headers are just send
  if (_headerBuffer.size() == _headerSendIdx) {
    // mark headers send
    _headersSend = true;
    // disconnect from bytesWritten() signal
    disconnect(_connection->socket(), SIGNAL(bytesWritten(qint64)),
               this, SLOT(_onBytesWritten(qint64)));
    logDebug() << "HTTPResponse: ... headers send.";
    // signal headers send
    emit headersSend();
    // done
    return;
  }
  // Try to write some more data to the socket
  qint64 len = _connection->socket()->write(_headerBuffer.constData()+_headerSendIdx,
                                            _headerBuffer.size()-_headerSendIdx);
  if (0 < len) { _headerSendIdx += len; }
}


/* ********************************************************************************************* *
 * Implementation of HttpResponse
 * ********************************************************************************************* */
HttpStringResponse::HttpStringResponse(HttpVersion version, HttpResponseCode resp, const QString &text,
                                       HttpConnection *connection, const QString contentType)
  : HttpResponse(version, resp, connection), _textIdx(0), _text(text.toUtf8())
{
  // set content length
  setHeader("Content-Length", QString::number(_text.size()));
  // set content type
  setHeader("Content-Type", contentType);
  // Connect to headersSend signal
  connect(this, SIGNAL(headersSend()), this, SLOT(_onHeadersSend()));
}

void
HttpStringResponse::_onHeadersSend() {
  logDebug() << "HttpStringResponse: Headers send: Send content.";
  connect(_connection->socket(), SIGNAL(bytesWritten(qint64)),
          this, SLOT(_onBytesWritten(qint64)));
  _onBytesWritten(0);
}

void
HttpStringResponse::_onBytesWritten(qint64 bytes) {
  if (! _headersSend) { return; }
  if (_textIdx == _text.size()) {
    disconnect(_connection->socket(), SIGNAL(bytesWritten(qint64)),
               this, SLOT(_onBytesWritten(qint64)));
    emit completed();
    return;
  }
  // Try to write some data to the socket
  qint64 len = _connection->socket()->write(_text.constData()+_textIdx,
                                            _text.size()-_textIdx);
  if (0 < len) { _textIdx += len; }
}


/* ********************************************************************************************* *
 * Implementation of HttpConnection
 * ********************************************************************************************* */
HttpConnection::HttpConnection(HttpRequestHandler *service, const NodeItem &remote, QIODevice *socket)
  : QObject(service), _service(service), _remote(remote), _socket(socket)
{
  // Create new request parser (request)
  _currentRequest = new HttpRequest(this);
  // no response yet
  _currentResponse = 0;
  // get notified on fishy requests
  connect(_currentRequest, SIGNAL(badRequest()), this, SLOT(_badRequest()));
  // get notified if the request headers has been read
  connect(_currentRequest, SIGNAL(headerRead()), this, SLOT(_requestHeadersRead()));
  // start parsing HTTP request
  _currentRequest->parse();
}

HttpConnection::~HttpConnection() {
  if (_socket) {
    // close socket
    _socket->close();
    // free it
    delete _socket;
    // done.
    _socket = 0;
  }
  // Do not need to free _currentRequest or _currentResponse.
  // They are children of this QObject.
}

void
HttpConnection::_requestHeadersRead() {
  // this should not happen
  if (! _currentRequest) { return; }
  // disconnect from signel
  disconnect(_currentRequest, SIGNAL(headerRead()), this, SLOT(_requestHeadersRead()));
  // If request is not accepted -> response with "Forbidden"
  if (! _service->acceptReqest(_currentRequest)) {
    // If not accepted send forbidden
    _currentResponse = new HttpStringResponse(_currentRequest->version(), HTTP_FORBIDDEN, "Forbidden", this);
  } else if (0 == (_currentResponse = _service->processRequest(_currentRequest))) {
    // If not processed -> not found
    _currentResponse = new HttpStringResponse(_currentRequest->version(), HTTP_NOT_FOUND, "Not found", this);
  }
  // Get notified if the response has been completed
  connect(_currentResponse, SIGNAL(completed()), this, SLOT(_responseCompleted()));
  // start response
  _currentResponse->sendHeaders();
}

void
HttpConnection::_badRequest() {
  // Do not further process request.
  disconnect(_currentRequest, SIGNAL(headerRead()), this, SLOT(_requestHeadersRead()));
  // Now we are in a undefined state for the connection -> better we close it
  deleteLater();
}

void
HttpConnection::_responseCompleted() {
  bool keepAlive = _currentRequest->isKeepAlive();
  // Free "old" request & response
  delete _currentRequest;  _currentRequest = 0;
  delete _currentResponse; _currentResponse = 0;
  // If not keep alive
  if (! keepAlive) {
    deleteLater();
  } else {
    // Get a new request
    _currentRequest = new HttpRequest(this);
    // get notified on fishy requests
    connect(_currentRequest, SIGNAL(badRequest()), this, SLOT(_badRequest()));
    // get notified if the request headers has been read
    connect(_currentRequest, SIGNAL(headerRead()), this, SLOT(_requestHeadersRead()));
    // start parsing HTTP request
    _currentRequest->parse();
  }
}


/* ********************************************************************************************* *
 * Implementation of HttpService
 * ********************************************************************************************* */
HttpRequestHandler::HttpRequestHandler(QObject *parent)
  : QObject(parent)
{
  // pass...
}

HttpRequestHandler::~HttpRequestHandler() {
  // pass...
}


/* ********************************************************************************************* *
 * Implementation of LocalHttpServer
 * ********************************************************************************************* */
LocalHttpServer::LocalHttpServer(HttpRequestHandler *dispatcher, uint16_t port)
  : QObject(dispatcher), _dispatcher(dispatcher), _server()
{
  if (_server.listen(QHostAddress::LocalHost, port)) {
    logDebug() << "Started LocalHttpService @localhost:" << port;
  } else {
    logError() << "Failed to start LocalHttpService @localhost:" << port;
  }

  connect(&_server, SIGNAL(newConnection()), this, SLOT(_onNewConnection()));
}

LocalHttpServer::~LocalHttpServer() {
  // close the socket
  _server.close();
}

void
LocalHttpServer::_onNewConnection() {
  // trivial, create a HttpConnection instance for every incomming TCP connection
  while (_server.hasPendingConnections()) {
    QTcpSocket *socket = _server.nextPendingConnection();
    new HttpConnection(
          _dispatcher, NodeItem(Identifier(), socket->peerAddress(), socket->peerPort()), socket);
  }
}


/* ********************************************************************************************* *
 * Implementation of HttpService
 * ********************************************************************************************* */
HttpService::HttpService(DHT &dht, HttpRequestHandler *handler, QObject *parent)
  : QObject(parent), _dht(dht), _handler(handler)
{
  // pass...
}

HttpService::~HttpService() {
  // pass...
}

SecureSocket *
HttpService::newSocket() {
  return new SecureStream(_dht, _handler);
}

void
HttpService::connectionStarted(SecureSocket *socket) {
  SecureStream *connection = dynamic_cast<SecureStream *>(socket);
  if (! connection) {
    logError() << "Invalid connection type.";
    delete socket;
  }
  new HttpConnection(_handler, NodeItem(socket->peerId(), socket->peer()), connection);
}

bool
HttpService::allowConnection(const NodeItem &peer) {
  return true;
}

void
HttpService::connectionFailed(SecureSocket *socket) {
  delete socket;
}
