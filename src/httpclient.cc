#include "httpclient.hh"


/* ********************************************************************************************* *
 * Implementation of NetworkConnection
 * ********************************************************************************************* */
HttpClientConnection::HttpClientConnection(Node &node, const NodeItem &remote, const QString &service, QObject *parent)
  : SecureStream(node, parent), _state(CONNECTING), _service(service)
{
  _node.startConnection(_service, remote, this);
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
