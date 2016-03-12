#include "httpservice.hh"
#include "logger.hh"
#include "stream.hh"
#include "buckets.hh"
#include "node.hh"
#include <QUrl>
#include <QTcpSocket>
#include <QJsonDocument>


/* ********************************************************************************************* *
 * Implementation of HostName
 * ********************************************************************************************* */
HostName::HostName(const QString &name, uint16_t defaultPort)
  : _name(name), _port(defaultPort)
{
  // Split at ':'
  if (_name.contains(':')) {
    int idx = _name.indexOf(':');
    _port = _name.mid(idx+1).toUInt();
    _name = _name.left(idx);
  }
}

HostName::HostName(const HostName &other)
  : _name(other._name), _port(other._port)
{
  // pass...
}

HostName &
HostName::operator =(const HostName &other) {
  _name = other._name;
  _port = other._port;
  return *this;
}

const QString &
HostName::name() const {
  return _name;
}

uint16_t
HostName::port() const {
  return _port;
}

bool
HostName::isOvlNode() const {
  return _name.endsWith(".ovl");
}

Identifier
HostName::ovlId() const {
  return Identifier::fromBase32(_name.left(_name.size()-4));
}


/* ********************************************************************************************* *
 * Implementation of URI
 * ********************************************************************************************* */
URI::URI()
  : _proto(), _host(""), _path(), _query()
{

}

URI::URI(const QString &uri)
  : _proto(), _host(""), _path(), _query()
{
  QUrl url(uri);
  _proto = url.scheme();
  _host  = HostName(url.host(), url.port());
  _path  = url.path();
  _query = url.query();
}

URI::URI(const URI &other)
  : _proto(other._proto), _host(other._host), _path(other._path), _query(other._query)
{
  // pass...
}

URI &
URI::operator =(const URI &other) {
  _proto = other._proto;
  _host = other._host;
  _path = other._path;
  _query = other._query;
  return *this;
}


/* ********************************************************************************************* *
 * Implementation of HTTPRequest
 * ********************************************************************************************* */
HttpRequest::HttpRequest(QIODevice *socket, const NodeItem &remote)
  : QObject(socket), _remote(remote), _socket(socket), _parserState(READ_REQUEST), _uri(), _headers()
{
  // pass..
}

void
HttpRequest::parse() {
  connect(_socket, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
  _onReadyRead();
}

const NodeItem &
HttpRequest::remote() const {
  return _remote;
}

void
HttpRequest::_onReadyRead() {  
  // Check if a line can be read from the device:
  while (_socket->canReadLine()) {
    if (READ_REQUEST == _parserState) {
      QByteArray line = _socket->readLine();
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
      _uri = QString::fromLatin1(line.constData()+offset, idx-offset);
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
      QByteArray line = _socket->readLine();
      if (! line.endsWith("\r\n")) {
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
        disconnect(_socket, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
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
                      QString::fromLatin1(line.constData()+idx+1, line.length()-idx-1).simplified());
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
HttpResponse::HttpResponse(HttpVersion version, HttpResponseCode resp, QIODevice *socket)
  : QObject(socket), _socket(socket), _version(version), _code(resp),
    _headersSend(false), _headerSendIdx(0), _headers()
{
}

void
HttpResponse::sendHeaders() {
  // Skip if headers are serialized already or even has been send
  if (_headersSend || (0 != _headerBuffer.size()) || (HTTP_RESP_INCOMPLETE == _code)) { return; }
  logDebug() << "Serialize headers...";
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

  // Connect to bytesWritten signal
  connect(_socket, SIGNAL(bytesWritten(qint64)),
          this, SLOT(_onBytesWritten(qint64)));

  // Try to send some part of the serialized response
  qint64 len = _socket->write(_headerBuffer);
  if (len > 0) { _headerSendIdx = len; }
}

void
HttpResponse::_onBytesWritten(qint64 bytes) {
  logDebug() << "HttpResponse: Continue send headers.";
  // If headers already send -> done
  if (_headersSend) { return; }
  // If headers are just send
  if (_headerBuffer.size() == _headerSendIdx) {
    // mark headers send
    _headersSend = true;
    // disconnect from bytesWritten() signal
    disconnect(_socket, SIGNAL(bytesWritten(qint64)),
               this, SLOT(_onBytesWritten(qint64)));
    logDebug() << "HTTPResponse: ... headers send.";
    // signal headers send
    emit headersSend();
    // done
    return;
  }
  // Try to write some more data to the socket
  qint64 len = _socket->write(_headerBuffer.constData()+_headerSendIdx,
                              _headerBuffer.size()-_headerSendIdx);
  if (0 < len) { _headerSendIdx += len; }
}


/* ********************************************************************************************* *
 * Implementation of HttpResponse
 * ********************************************************************************************* */
HttpStringResponse::HttpStringResponse(HttpVersion version, HttpResponseCode resp, const QString &text,
                                       QIODevice *socket, const QString contentType)
  : HttpResponse(version, resp, socket), _textIdx(0), _text(text.toUtf8())
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
  connect(_socket, SIGNAL(bytesWritten(qint64)),
          this, SLOT(_bytesWritten(qint64)));
  // Start body transmission
  _bytesWritten(0);
}

void
HttpStringResponse::_bytesWritten(qint64 bytes) {
  if (! _headersSend) { return; }
  if (_textIdx == size_t(_text.size())) {
    disconnect(_socket, SIGNAL(bytesWritten(qint64)),
               this, SLOT(_onBytesWritten(qint64)));
    logDebug() << "HttpStringResponse: Content send.";
    emit completed();
    return;
  }
  // Try to write some data to the socket
  qint64 len = _socket->write(_text.constData()+_textIdx,
                              _text.size()-_textIdx);
  if (0 < len) { _textIdx += len; }
}


/* ********************************************************************************************* *
 * Implementation of HttpJsonResponse
 * ********************************************************************************************* */
HttpJsonResponse::HttpJsonResponse(const QJsonDocument &document, HttpRequest *request)
  : HttpStringResponse(
      request->version(), HTTP_OK, document.toJson(), request->socket(), "application/json")
{
  // pass
}


/* ********************************************************************************************* *
 * Implementation of HttpFileResponse
 * ********************************************************************************************* */
HttpFileResponse::HttpFileResponse(const QString &filename, HttpRequest *request)
  : HttpResponse(request->version(), HTTP_RESP_INCOMPLETE, request->socket()),
    _file(filename), _offset(0)
{
  if (! _file.open(QIODevice::ReadOnly)) {
    this->setResponseCode(HTTP_FORBIDDEN);
    this->setHeader("Content-Length", "0");
    return;
  }

  this->setResponseCode(HTTP_OK);
  QFileInfo fi(_file.fileName());
  this->setHeader("Content-Type", guessMimeType(fi.suffix()));
  this->setHeader("Content-Length", QString::number(_file.size()));
  logDebug() << "Send file of size " << _file.size() << "b";
  connect(this, SIGNAL(headersSend()), this, SLOT(_onHeadersSend()));
}

void
HttpFileResponse::_onHeadersSend() {
  connect(_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(_bytesWritten(qint64)));
  _onBytesWritten(0);
}

void
HttpFileResponse::_bytesWritten(qint64 bytes)
{
  if (_offset == _file.size()) {
    logDebug() << "File send... Wait for completion.";
    // If all has be read from file
    if (0 == _socket->bytesToWrite()) {
      logDebug() << "Transfer completed.";
      // If all has been send to the device
      emit completed();
    }
    return;
  }

  char buffer[0xffff];
  _file.seek(_offset);
  qint64 len = _file.read(buffer, 0xffff);
  logDebug() << "Read " << len << "b";
  if (len>0) {
    len = _socket->write(buffer, len);
    logDebug() << "Send " << len << "b";
    if (len>0) {
      _offset += len;
    }
  }
}

QString
HttpFileResponse::guessMimeType(const QString &ext) {
  // Try to determine content type by file extension
  if (("html"==ext) | ("htm"==ext)) { return "text/html"; }
  if ("xml" == ext) { return "application/xml"; }
  if ("png" == ext) { return "image/png"; }
  if ("jpeg" == ext) { return "image/jpeg"; }
  if ("js" == ext) { return "text/javascript"; }
  if ("jpeg" == ext) { return "image/jpeg"; }

  return "application/octet-stream";
}


/* ********************************************************************************************* *
 * Implementation of HttpDirectoryResponse
 * ********************************************************************************************* */
HttpDirectoryResponse::HttpDirectoryResponse(const QString &dirname, HttpRequest *request)
  : HttpResponse(request->version(), HTTP_OK, request->socket()), _buffer(), _offset(0)
{
  QDir dir(dirname);
  QStringList elements = dir.entryList();
  _buffer.append("<html><head></head><body><table>");
  foreach (QString element, elements) {
    QFileInfo einfo(element);
    _buffer.append("<tr><td><a href=\"");
    _buffer.append(element.toUtf8());
    if (einfo.isDir()) { _buffer.append("/"); }
    _buffer.append("\">");
    _buffer.append(element.toUtf8());
    _buffer.append("</a></td></tr>");
  }
  _buffer.append("</table></body></html>");
  this->setHeader("Content-Type", "text/html");
  this->setHeader("Content-Length", QString::number(_buffer.size()));

  connect(this, SIGNAL(headersSend()), this, SLOT(_onHeadersSend()));
}

void
HttpDirectoryResponse::_onHeadersSend() {
  connect(_socket, SIGNAL(bytesWritten(qint64)), this, SLOT(_bytesWritten(qint64)));
  _bytesWritten(0);
}

void
HttpDirectoryResponse::_bytesWritten(qint64 bytes) {
  if (_offset == _buffer.size()) {
    // If all has be send to the buffer
    if (0 == _socket->bytesToWrite()) {
      // If all has been send to the device
      emit completed();
    }
    return;
  }
  qint64 len = _socket->write(_buffer.constData()+_offset,
                              _buffer.size()-_offset);
  if (len>0) { _offset += len; }
}


/* ********************************************************************************************* *
 * Implementation of HttpConnection
 * ********************************************************************************************* */
HttpConnection::HttpConnection(HttpRequestHandler *service, const NodeItem &remote, QIODevice *socket)
  : QObject(), _service(service), _remote(remote), _socket(socket)
{
  logDebug() << "New HTTP connection...";
  // Create new request parser (request)
  _currentRequest = new HttpRequest(this->_socket, _remote);
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
  logDebug() << "Request headers read.";
  // this should not happen
  if (! _currentRequest) { return; }
  // disconnect from signel
  disconnect(_currentRequest, SIGNAL(headerRead()), this, SLOT(_requestHeadersRead()));
  // If request is not accepted -> response with "Forbidden"
  if (! _service->acceptReqest(_currentRequest)) {
    // If not accepted send forbidden
    _currentResponse = new HttpStringResponse(_currentRequest->version(), HTTP_FORBIDDEN, "Forbidden",
                                              this->_socket);
  } else if (0 == (_currentResponse = _service->processRequest(_currentRequest))) {
    // If not processed -> not found
    _currentResponse = new HttpStringResponse(_currentRequest->version(), HTTP_NOT_FOUND, "Not found",
                                              this->_socket);
  }
  // Take ownership
  _currentResponse->setParent(this);
  // get notified if the response has been completed
  connect(_currentResponse, SIGNAL(completed()), this, SLOT(_responseCompleted()));
  // start response if ready
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
    _currentRequest = new HttpRequest(this->_socket);
    // get notified on fishy requests
    connect(_currentRequest, SIGNAL(badRequest()), this, SLOT(_badRequest()));
    // get notified if the request headers has been read
    connect(_currentRequest, SIGNAL(headerRead()), this, SLOT(_requestHeadersRead()));
    // start parsing HTTP request
    _currentRequest->parse();
  }
}

/* ********************************************************************************************* *
 * Implementation of HttpDirectoryHandler
 * ********************************************************************************************* */
HttpDirectoryHandler::HttpDirectoryHandler(const QDir &directory)
  : HttpRequestHandler(), _directory(directory)
{
  // pass...
}

bool
HttpDirectoryHandler::acceptReqest(HttpRequest *request) {
  if (HTTP_GET==request->method()) {
    return true;
  }
  return false;
}

HttpResponse *
HttpDirectoryHandler::processRequest(HttpRequest *request) {
  QString path = request->uri().path();
  QFileInfo file(_directory.absolutePath() + "/" + path);
  if (! file.exists()) {
    logDebug() << "Path " << file.path() << " does not exist.";
    // 404
    return new HttpStringResponse(request->version(), HTTP_NOT_FOUND,
                                  "Not found", request->socket(), "text/plain");
  }
  if (! file.canonicalFilePath().startsWith(_directory.canonicalPath())) {
    logDebug() << "Path " << file.path() << " is not located below " << _directory.path();
    // 404
    return new HttpStringResponse(request->version(), HTTP_NOT_FOUND,
                                  "Not found", request->socket(), "text/plain");
  }
  if (file.isFile()) {
    logDebug() << "Serve file " << file.canonicalPath();
    return new HttpFileResponse(file.canonicalFilePath(), request);
  } else {
    logDebug() << "Serve directory " << file.canonicalPath();
    return new HttpDirectoryResponse(file.canonicalFilePath(), request);
  }
}


/* ********************************************************************************************* *
 * Implementation of HttpService
 * ********************************************************************************************* */
HttpRequestHandler::HttpRequestHandler()
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
  : QObject(), _dispatcher(dispatcher), _server()
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

bool
LocalHttpServer::started() const {
  return _server.isListening();
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
HttpService::HttpService(Node &dht, HttpRequestHandler *handler, QObject *parent)
  : QObject(parent), _dht(dht), _handler(handler)
{
  // pass...
}

HttpService::~HttpService() {
  // pass...
}

SecureSocket *
HttpService::newSocket() {
  return new SecureStream(_dht, this);
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
