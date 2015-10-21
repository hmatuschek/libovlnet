#include "httpservice.h"


/* ********************************************************************************************* *
 * Implementation of HTTPRequest
 * ********************************************************************************************* */
HttpRequest::HttpRequest(HttpConnection *connection)
  : QObject(connection), _connection(connection), _parserState(READ_REQUEST), _headers()
{
  QObject::connect(_connection->socket(), SIGNAL(readyRead()),
                   this, SLOT(_onReadyRead()));
}

void
HttpRequest::_onReadyRead() {
  // Check if a line can be read from the device:
  while (_connection->socket()->canReadLine()) {
    if (READ_REQUEST == _parserState) {
      QByteArray line = _connection->socket()->readLine();
      if (!line.endsWith("\r\n")) {
        // Invalid format
      }
      // drop last two chars ("\r\n")
      line.chop(2);

      // Parse HTTP request
      int offset = 0, idx = 0;
      // Read method
      if (0 > (idx = line.indexOf(' ', offset))) {
        // MEHTOD not found
      }
      _method = _getMethod(line, idx);
      // check method
      if (HTTP_INVALID_METHOD == _method) {
        // INVALID METHOD
      }
      // Update offset
      offset = idx+1;

      // Read path
      if (0 > (idx = line.indexOf(' ', offset))) {
        // path not set!!!
      }
      _path = QString::fromLatin1(line.constData()+offset, idx-offset);
      offset = idx+1;

      // Extract version
      _version = _getVersion(line.constData()+offset, line.size()-offset);
      // Check version
      if (HTTP_INVALID_VERSION == _version) {
        // Invalid version
      }
      // done
      _parserState = READ_HEADER;
    } else if (READ_HEADER == _parserState) {
      QByteArray line = _connection->socket()->readLine();
      if (!line.endsWith("\r\n")) {
        // Invalid format
      }
      // drop last two chars ("\r\n")
      line.chop(2);

      // Check if end-of-headers is reached
      if (0 == line.size()) {
        // done, disconnect from readyRead signal. Any additional data will be processed by the
        // request handler or by the next request
        disconnect(_connection->socket(), SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
        return;
      }
      // Seach for first colon
      int idx=0;
      if (0 > (idx = line.indexOf(':'))) {
        // Invalid header format
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
HttpResponse::HttpResponse(HttpResponseCode resp, HttpConnection *connection)
  : QObject(connection), _connection(connection), _code(resp),
    _headersSend(false), _headerSendIdx(0), _headers()
{
  // Connect to bytesWritten signal
  connect(_connection->socket(), SIGNAL(bytesWritten(qint64)),
          this, SLOT(onBytesWritten(qint64)));
}

void
HttpResponse::sendHeaders() {
  // Skip if headers are serialized already
  if (0 != _headerBuffer.size()) { return; }
  // Dispatch by response type
  switch (_code) {
    case HTTP_OK: _headerBuffer.append("200 OK\r\n"); break;
    case HTTP_SERVER_ERROR: _headerBuffer.append("500 Internal Server error\r\n"); break;
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
HttpResponse::onBytesWritten(qint64 bytes) {
  if (_headersSend) { return; }
  if (_headerBuffer.size() == _headerSendIdx) {
    // Headers send
    _headersSend = true;
    // Disconnect from bytesWritten() signal
    disconnect(_connection->socket(), SIGNAL(bytesWritten(qint64)),
               this, SLOT(onBytesWritten(qint64)));
    return;
  }
  // Try to write some data to the socket
  qint64 len = _connection->socket()->write(_headerBuffer.constData()+_headerSendIdx,
                                            _headerBuffer.size()-_headerSendIdx);
  if (0 < len) { _headerSendIdx += len; }
}


