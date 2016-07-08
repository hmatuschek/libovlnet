#include "httpclient.hh"
#include <QJsonDocument>


/* ********************************************************************************************* *
 * Implementation of HttpClientConnection
 * ********************************************************************************************* */
HttpClientConnection::HttpClientConnection(Network &net, const NodeItem &remote, const QString &service, QObject *parent)
  : SecureStream(net, parent), _state(CONNECTING), _service(service), _remote(remote)
{
  _network.root().startConnection(_network.prefix()+"::"+_service, remote, this);
}

bool
HttpClientConnection::start(const Identifier &streamId, const PeerItem &peer) {
  _state = IDLE;
  if (! SecureStream::start(streamId, peer)) {
    // connection failed
    _state = ERROR;
    return false;
  }
  return true;
}

void
HttpClientConnection::failed() {
  _state = ERROR;
  SecureStream::failed();
}

HttpClientResponse *
HttpClientConnection::get(const QString &path) {
  if (IDLE != _state) {
    logInfo() << "Cannot send GET " << path << " request. Connection state (" << _state
              << ") is not idle.";
    return 0;
  }

  _state = PROCESS_REQUEST;
  // Construct request
  HttpClientResponse *resp = new HttpClientResponse(this, HTTP_GET, path);
  connect(resp, SIGNAL(destroyed(QObject*)), this, SLOT(_onRequestFinished(QObject *)));
  // No body : close instream immediately
  resp->close();
  return resp;
}

const NodeItem &
HttpClientConnection::remote() const {
  return _remote;
}

void
HttpClientConnection::_onRequestFinished(QObject *req) {
  if (PROCESS_REQUEST == _state)
    _state = IDLE;
}


/* ********************************************************************************************* *
 * Implementation of NetworkResponse
 * ********************************************************************************************* */
HttpClientResponse::HttpClientResponse(HttpClientConnection *connection, HttpMethod method, const QString &path)
  : QIODevice((QObject *)connection), _connection(connection), _state(SEND_HEADER), _method(method),
    _path(path), _resCode(HTTP_RESP_INCOMPLETE)
{
  switch (_method) {
    case HTTP_GET: _connection->write("GET "); break;
    case HTTP_HEAD: _connection->write("HEAD "); break;
    case HTTP_POST: _connection->write("POST "); break;
    default:
      _state = ERROR;
      return;
  }
  _connection->write(path.toUtf8());
  _connection->write(" HTTP/1.1\r\n");
  _connection->write("Host: ");
  _connection->write(_connection->peerId().toBase32().toUtf8());
  _connection->write(".ovl\r\n");
  _connection->write("\r\n");

  _state = SEND_BODY;
  if (! open(QIODevice::WriteOnly)) {
    _state = ERROR;
  }
  connect(_connection, SIGNAL(bytesWritten(qint64)), this, SIGNAL(bytesWritten(qint64)));
}

void
HttpClientResponse::_onDataAvailable() {
  while (_connection->canReadLine()) {
    QByteArray line = _connection->readLine();
    if (RECV_RESPONSE_CODE == _state) {
      if (line.size() < 12) { // HTTP/1.1 XXX
        logError() << "Invalid response line " << line;
        disconnect(_connection, SIGNAL(readyRead()), this, SLOT(_onDataAvailable()));
        _state = ERROR;
        return;
      }
      bool ok; uint code = line.mid(9,3).toUInt(&ok);
      if (! ok) {
        logError() << "Invalid response code " << line;
        disconnect(_connection, SIGNAL(readyRead()), this, SLOT(_onDataAvailable()));
        _state = ERROR;
        return;
      }
      switch (code) {
        case HTTP_OK: _resCode = HTTP_OK; break;
        case HTTP_BAD_REQUEST: _resCode = HTTP_BAD_REQUEST; break;
        case HTTP_FORBIDDEN: _resCode = HTTP_FORBIDDEN; break;
        case HTTP_NOT_FOUND: _resCode = HTTP_NOT_FOUND; break;
        case HTTP_SERVER_ERROR: _resCode = HTTP_SERVER_ERROR; break;
        case HTTP_BAD_GATEWAY: _resCode = HTTP_BAD_GATEWAY; break;
        default:
          logError() << "Invalid response code " << code;
          disconnect(_connection, SIGNAL(readyRead()), this, SLOT(_onDataAvailable()));
          _state = ERROR;
          return;
      }
      _state = RECV_HEADER;
    } else if (RECV_HEADER == _state) {
      line = line.simplified();
      if ("" == line.simplified()) {
        disconnect(_connection, SIGNAL(readyRead()), this, SLOT(_onDataAvailable()));
        connect(_connection, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
        _state = RECV_BODY;
        if (! open(QIODevice::ReadOnly)) {
          logError() << "Cannot reopen response for reading response body.";
          _state = ERROR;
          return;
        }
        emit finished();
        return;
      }
      int idx = line.indexOf(':');
      if (0 > idx) {
        disconnect(_connection, SIGNAL(readyRead()), this, SLOT(_onDataAvailable()));
        _state = ERROR;
        return;
      }
      QByteArray name = line.left(idx);
      QByteArray value = line.mid(idx+1).simplified();
      _resHeaders.insert(name, value);
    }
  }
}

bool
HttpClientResponse::isSequential() const {
  return true;
}

qint64
HttpClientResponse::bytesAvailable() const {
  return _connection->bytesAvailable();
}

qint64
HttpClientResponse::bytesToWrite() const {
  return _connection->bytesToWrite();
}

void
HttpClientResponse::close() {
  QIODevice::close();
  if (SEND_BODY == _state) {
    _state = RECV_RESPONSE_CODE;
    disconnect(_connection, SIGNAL(bytesWritten(qint64)), this, SIGNAL(bytesWritten(qint64)));
    connect(_connection, SIGNAL(readyRead()), this, SLOT(_onDataAvailable()));
  } else if (RECV_BODY == _state) {
    disconnect(_connection, SIGNAL(readyRead()), this, SIGNAL(readyRead()));
    _state = FINISHED;
  }
}

HttpResponseCode
HttpClientResponse::responseCode() const {
  return _resCode;
}

bool
HttpClientResponse::hasResponseHeader(const QByteArray &header) const {
  return _resHeaders.contains(header);
}

QByteArray
HttpClientResponse::responseHeader(const QByteArray &header) const {
  return _resHeaders[header];
}

qint64
HttpClientResponse::readData(char *data, qint64 maxlen) {
  if (RECV_BODY != _state) { return -1; }
  return _connection->read(data, maxlen);
}

qint64
HttpClientResponse::writeData(const char *data, qint64 len) {
  if (SEND_BODY != _state) { return -1; }
  return _connection->write(data, len);
}


/* ********************************************************************************************* *
 * Implementation of JsonQuery
 * ********************************************************************************************* */
JsonQuery::JsonQuery(const QString &service, const QString &path, Network &net, const Identifier &remote)
  : QObject(0), _service(service), _query(path), _network(net), _remoteId(remote),
    _method(HTTP_GET), _data()
{
  FindNodeQuery *query = new FindNodeQuery(remote);
  connect(query, SIGNAL(found(NodeItem)), this, SLOT(nodeFound(NodeItem)));
  connect(query, SIGNAL(failed(Identifier,QList<NodeItem>)), this, SLOT(error()));
  _network.search(query);
}

JsonQuery::JsonQuery(const QString &service, const QString &path, Network &net, const NodeItem &remote)
  : QObject(0), _service(service), _query(path), _network(net), _remoteId(remote.id()),
    _method(HTTP_GET), _data()
{
  nodeFound(remote);
}

JsonQuery::JsonQuery(const QString &service, const QString &path, const QJsonDocument &data, Network &net, const Identifier &remote)
  : QObject(0), _service(service), _query(path), _network(net), _remoteId(remote),
    _method(HTTP_POST), _data(data)
{
  FindNodeQuery *query = new FindNodeQuery(remote);
  connect(query, SIGNAL(found(NodeItem)), this, SLOT(nodeFound(NodeItem)));
  connect(query, SIGNAL(failed(Identifier,QList<NodeItem>)), this, SLOT(error()));
  _network.search(query);
}

JsonQuery::JsonQuery(const QString &service, const QString &path, const QJsonDocument &data, Network &net, const NodeItem &remote)
  : QObject(0), _service(service), _query(path), _network(net), _remoteId(remote.id()),
    _method(HTTP_POST), _data(data)
{
  nodeFound(remote);
}

void
JsonQuery::nodeFound(const NodeItem &node) {
  /*logDebug() << "Try to connect station '" << node.id().toBase32()
             << "' for '" << _query << "'."; */
  _connection = new HttpClientConnection(_network, node, _service, this);
  connect(_connection, SIGNAL(established()), this, SLOT(connectionEstablished()));
  connect(_connection, SIGNAL(error()), this, SLOT(error()));
}

void
JsonQuery::connectionEstablished() {
  /*logDebug() << "Try to query '" << _query
             << "' from station '" << _connection->peerId() << "'."; */
  _response = _connection->get(_query);
  if (_response) {
    connect(_response, SIGNAL(finished()), this, SLOT(responseReceived()));
    connect(_response, SIGNAL(error()), this, SLOT(error()));
  } else {
    error();
  }
}

bool
JsonQuery::accept() {
  if (HTTP_OK != _response->responseCode()) {
    logError() << "Cannot query '" << _query << "': Node returned " << _response->responseCode();
    return false;
  }
  if (! _response->hasResponseHeader("Content-Length")) {
    logError() << "Node response has no length!";
    return false;
  }
  if (! _response->hasResponseHeader("Content-Type")) {
    logError() << "Node response has no content type!";
    return false;
  }
  if ("application/json" != _response->responseHeader("Content-Type")) {
    logError() << "Response content type '" << _response->responseHeader("Content-Type")
               << " is not 'application/json'!";
    return false;
  }
  return true;
}

void
JsonQuery::responseReceived() {
  if (! this->accept()) {
    logDebug() << "Response rejected.";
    error(); return;
  }
  _responseLength = _response->responseHeader("Content-Length").toUInt();
  connect(_response, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
}

void
JsonQuery::error() {
  logError() << "Failed to access " << _query << " at " << _remoteId.toBase32() << ".";
  emit failed();
  deleteLater();
}

void
JsonQuery::_onReadyRead() {
  if (_responseLength) {
    // Read as much as possible
    QByteArray tmp = _response->read(_responseLength);
    // add to response buffer
    _buffer.append(tmp);
    // update response length
    _responseLength -= tmp.size();
  }
  if (0 == _responseLength) {
    // If finished
    QJsonParseError jerror;
    // parse JSON
    QJsonDocument doc = QJsonDocument::fromJson(_buffer, &jerror);
    if (QJsonParseError::NoError != jerror.error) {
      // On error
      logInfo() << "Station returned invalid JSON document as result: "
                << jerror.errorString() << ".";
      error(); return;
    }
    // signal success
    this->finished(doc);
  }
}

void
JsonQuery::finished(const QJsonDocument &doc) {
  emit success(_connection->remote(), doc);
  this->deleteLater();
}
