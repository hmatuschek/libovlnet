#include "socks.h"
#include <netinet/in.h>
#include <QHostInfo>
#include "dht.h"


/* ******************************************************************************************** *
 * Implementation of SOCKSInStream
 * ******************************************************************************************** */
LocalSocksStream::LocalSocksStream(DHT &dht, QTcpSocket *instream, QObject *parent)
  : SecureStream(dht, parent), _inStream(instream)
{
  // Take ownership of TCP socket.
  _inStream->setParent(this);
}

LocalSocksStream::~LocalSocksStream() {
  _inStream->close();
  if (isOpen()) {
    LocalSocksStream::close();
  }
}

bool
LocalSocksStream::open(OpenMode mode) {
  if (! SecureStream::open(mode)) { return false; }
  logDebug() << "SOCKS in stream started.";
  // Connect to client signals
  connect(_inStream, SIGNAL(readyRead()), this, SLOT(_clientReadyRead()));
  connect(_inStream, SIGNAL(bytesWritten(qint64)), this, SLOT(_clientBytesWritten(qint64)));
  connect(_inStream, SIGNAL(disconnected()), this, SLOT(_clientDisconnected()));
  connect(_inStream, SIGNAL(error(QAbstractSocket::SocketError)),
          this, SLOT(_clientError(QAbstractSocket::SocketError)));
  // Connect to remote signals
  connect(this, SIGNAL(readyRead()), this, SLOT(_remoteReadyRead()));
  connect(this, SIGNAL(bytesWritten(qint64)), this, SLOT(_remoteBytesWritten(qint64)));
  connect(this, SIGNAL(readChannelFinished()), this, SLOT(_remoteClosed()));

  // If there is some data available at the input stream -> start transfer
  _clientReadyRead();

  // done
  return true;
}

void
LocalSocksStream::close() {
  SecureStream::close();
  deleteLater();
}

void
LocalSocksStream::_clientReadyRead() {
  uint8_t buffer[DHT_STREAM_MAX_DATA_SIZE];
  // Keep my output buffer limited to 1Mb.
  while ( _inStream->bytesAvailable() && ((1<<20)>bytesToWrite())) {
    qint64 len = std::min(qint64(DHT_STREAM_MAX_DATA_SIZE),
                          (1<<20)-_inStream->bytesToWrite());
    // Read from client
    if (0 < (len = _inStream->read((char *)buffer, len))) {
      // Forward to SOCKS server
      write((const char *)buffer, len);
    }
  }
}

void
LocalSocksStream::_clientBytesWritten(qint64 bytes) {
  uint8_t buffer[DHT_STREAM_MAX_DATA_SIZE];
  while (bytesAvailable() && ((1<<20)>_inStream->bytesToWrite())) {
    qint64 len = std::min(qint64(DHT_STREAM_MAX_DATA_SIZE),
                          (1<<20)-_inStream->bytesToWrite());
    if (0 < (len = read((char *)buffer, len))) {
      _inStream->write((const char *)buffer, len);
    }
  }
}

void
LocalSocksStream::_clientDisconnected() {
  logDebug() << "Client disconnected -> close SOCKS stream";
  close();
}

void
LocalSocksStream::_clientError(QAbstractSocket::SocketError error) {
  logDebug() << "Client connection error: " << _inStream->errorString()
             << " -> close socks stream";
  _inStream->close();
  close();
}

void
LocalSocksStream::_remoteReadyRead() {
  uint8_t buffer[DHT_STREAM_MAX_DATA_SIZE];
  while (bytesAvailable() && ((1<<20)>_inStream->bytesToWrite())) {
    qint64 len = std::min(qint64(DHT_STREAM_MAX_DATA_SIZE),
                          (1<<20)-_inStream->bytesToWrite());
    // Read some data from the SOCKS server
    if (0 < (len = read((char *)buffer, len))) {
      // Forward it to the client
      _inStream->write((const char *)buffer, len);
    }
  }
}

void
LocalSocksStream::_remoteBytesWritten(qint64 bytes) {
  uint8_t buffer[DHT_STREAM_MAX_DATA_SIZE];
  // Keep my output buffer limited to 1Mb.
  while (_inStream->bytesAvailable() && ((1<<20)>bytesToWrite())) {
    qint64 len = std::min(qint64(DHT_STREAM_MAX_DATA_SIZE),
                          (1<<20)-_inStream->bytesToWrite());
    // Read from client
    if (0 < (len = _inStream->read((char *)buffer, len))) {
      // Forward to SOCKS server
      write((const char *)buffer, len);
    }
  }
}

void
LocalSocksStream::_remoteClosed() {
  if (_inStream && _inStream->isOpen()) {
    logDebug() << "SOCKS connection closed -> close client connection.";
    _inStream->close();
  }
}


/* ********************************************************************************************* *
 * Implementation of LocalSocksService
 * ********************************************************************************************* */
LocalSocksService::LocalSocksService(DHT &dht, const NodeItem &remote, uint16_t port, QObject *parent)
  : QObject(parent), _dht(dht), _remote(remote), _server(), _connectionCount(0)
{
  logDebug() << "Start SOCKS proxy service at localhost:" << port;
  // Bind socket to local port
  if(! _server.listen(QHostAddress::LocalHost, port)) {
    logError() << "SOCKS: Can not bind to localhost:" << port;
  }
  connect(&_server, SIGNAL(newConnection()), this, SLOT(_onNewConnection()));
}

LocalSocksService::~LocalSocksService() {
  _server.close();
}

bool
LocalSocksService::isListening() const {
  return _server.isListening();
}

size_t
LocalSocksService::connectionCount() const {
  return _connectionCount;
}

void
LocalSocksService::_onNewConnection() {
  while (_server.hasPendingConnections()) {
    // Get for each incomming connection -> create a separate stream to the remote
    QTcpSocket *socket = _server.nextPendingConnection();
    logDebug() << "New incomming SOCKS connection...";
    LocalSocksStream *conn = new LocalSocksStream(_dht, socket);
    connect(conn, SIGNAL(destroyed()), this, SLOT(_onConnectionClosed()));
    _connectionCount++;
    emit connectionCountChanged(_connectionCount);
    _dht.startStream(5, _remote, conn);
  }
}

void
LocalSocksService::_onConnectionClosed() {
  if (_connectionCount)
    _connectionCount--;
  emit connectionCountChanged(_connectionCount);
}



/* ******************************************************************************************** *
 * Implementation of SOCKSOutStream
 * ******************************************************************************************** */
SocksOutStream::SocksOutStream(DHT &dht, QObject *parent)
  : SecureStream(dht, parent), _state(RX_VERSION), _outStream(0),
    _nAuthMeth(0), _authMeth(), _addr(), _nHostName(0), _hostName(), _port(0)
{
  // pass...
}

SocksOutStream::~SocksOutStream() {
  if (isOpen()) {
    close();
  }
  if (_outStream) {
    _outStream->close();
    delete _outStream;
    _outStream = 0;
  }
}

bool
SocksOutStream::open(OpenMode mode) {
  if (! SecureStream::open(mode)) {  return false; }

  // Connect to client signals
  connect(this, SIGNAL(readyRead()), this, SLOT(_clientParse()));
  // ...
  connect(this, SIGNAL(readChannelFinished()), this, SLOT(_clientClosed()));

  // If there is some data available at the input stream -> start transfer
  _clientParse();

  // done
  return true;
}


void
SocksOutStream::_clientParse() {
  while (bytesAvailable()) {
    /*
     * Dispatch by state.
     */
    if (RX_VERSION == _state) {
      // Expect version number and length of auth method
      if (2 > bytesAvailable()) { return; }
      uint8_t buffer[2]; read((char *) buffer, 2);
      // Check version number
      if (5 != buffer[0]) {
        logInfo() << "SOCKS: Unknown version number " << int(buffer[0]);
        close(); return;
      }
      // Get length of authentication method
      _nAuthMeth = buffer[1];
      // Update state
      _state = RX_AUTHENTICATION;
    } else if (RX_AUTHENTICATION == _state) {
      if (0 == bytesAvailable()) { return; }
      uint8_t buffer[255];
      size_t len = read((char *)buffer, _nAuthMeth);
      _authMeth = _authMeth + QString::fromUtf8((char *)buffer, len);
      _nAuthMeth -= len;
      if (0 == _nAuthMeth) {
        // Send response (version=5, no auth)
        const uint8_t msg[] = {0x05, 0x00};
        if(2 != write((const char *) msg, 2)) {
          logError() << "SOCKS: Can not send response.";
          close(); return;
        }
        // update state
        _state = RX_REQUEST;
      }
    } else if (RX_REQUEST == _state) {
      if (4 > bytesAvailable()) { return; }
      uint8_t buffer[4]; read((char *) buffer, 4);
      // check version
      if (5 != buffer[0]) {
        logInfo() << "SOCKS: Unknown version number " << int(buffer[0]);
        close(); return;
      }
      // check command (only CONNECT (0x01) is supported).
      if (1 != buffer[1]) {
        logInfo() << "SOCKS: Unsupported command " << int(buffer[1]);
        close(); return;
      }
      // check addr type
      if (1 == buffer[3]) {
        _state = RX_REQUEST_ADDR_IP4;
      } else if (3 == buffer[3]) {
        _state = RX_REQUEST_ADDR_NAME_LEN;
      } else if (4 == buffer[3]) {
        _state = RX_REQUEST_ADDR_IP6;
      } else {
        logInfo() << "Unsupported SOCKS address type " << int(buffer[3]);
        close(); return;
      }
    } else if (RX_REQUEST_ADDR_IP4 == _state) {
      if (4 > bytesAvailable()) { return; }
      uint32_t ip4=0; read((char *) &ip4, 4);
      _addr = QHostAddress(ntohl(ip4));
      _state = RX_REQUEST_PORT;
    } else if (RX_REQUEST_ADDR_IP6 == _state) {
      if (16 > bytesAvailable()) { return; }
      uint8_t buffer[16]; read( (char *) buffer, 16);
      _addr = QHostAddress(buffer);
      _state = RX_REQUEST_PORT;
    } else if (RX_REQUEST_ADDR_NAME_LEN == _state) {
      if (1 > bytesAvailable()) { return; }
        uint8_t len=0; read((char *) &len, 1);
      _nHostName = len; _state = RX_REQUEST_ADDR_NAME;
    } else if (RX_REQUEST_ADDR_NAME == _state) {
      if (1 > bytesAvailable()) { return; }
      uint8_t buffer[255];
      size_t len = read((char *)buffer, _nHostName);
      _hostName = _hostName + QString::fromUtf8((char *)buffer, len);
      _nHostName -= len;
      if (0 == _nHostName) { _state = RX_REQUEST_PORT; }
    } else if (RX_REQUEST_PORT == _state) {
      if (2 > bytesAvailable()) { return; }
      uint16_t port=0; read((char *) &port, 2);
      _port = ntohs(port);
      // Resolve host name if needed
      if (_addr.isNull()) {
        QList<QHostAddress> addrs = QHostInfo::fromName(_hostName).addresses();
        if (addrs.isEmpty()) {
          logInfo() << "Can not resolve host name " << _hostName;
          /// @bug Send a proper error message
          close(); return;
        }
        _addr = addrs.front();
      }
      // disconnect parser
      disconnect(this, SIGNAL(readyRead()), this, SLOT(_clientParse()));
      // Create TCP socket and init connection
      _outStream = new QTcpSocket();
      connect(_outStream, SIGNAL(connected()), this, SLOT(_remoteConnected()));
      connect(_outStream, SIGNAL(readChannelFinished()), this, SLOT(_remoteReadChannelFinished()));
      connect(_outStream, SIGNAL(error(QAbstractSocket::SocketError)),
              this, SLOT(_remoteError(QAbstractSocket::SocketError)));
      // connect to remote host...
      _outStream->connectToHost(_addr, _port);
      _state = CONNECTING;
      return;
    }
  }
}

void
SocksOutStream::_remoteConnected() {
  if (CONNECTING == _state) {
    logDebug() << "SOCKS: Remote " << _addr << ":" << _port << " connected -> start proxy session";
    // connect proxy client
    connect(this, SIGNAL(readyRead()), this, SLOT(_clientReadyRead()));
    connect(this, SIGNAL(bytesWritten(qint64)), this, SLOT(_clientBytesWritten(qint64)));
    // Connect to remote signals
    connect(_outStream, SIGNAL(readyRead()), this, SLOT(_remoteReadyRead()));
    connect(_outStream, SIGNAL(bytesWritten(qint64)), this, SLOT(_remoteBytesWritten(qint64)));
    // Send OK response
    uint8_t msg[DHT_STREAM_MAX_DATA_SIZE];
    uint8_t *ptr = msg; int len=0;
    // Version
    (*ptr) = 0x05; ptr++; len++;
    // success
    (*ptr) = 0x00; ptr++; len++;
    // reserved
    (*ptr) = 0x00; ptr++; len++;
    // address
    if (QAbstractSocket::IPv4Protocol == _outStream->localAddress().protocol()) {
      (*ptr) = 0x01; ptr++; len++; // IPv4
      (*(uint32_t *)ptr) = htonl(_outStream->localAddress().toIPv4Address()); ptr += 4; len += 4;
    } else if (QAbstractSocket::IPv6Protocol == _outStream->localAddress().protocol()) {
      (*ptr) = 0x04; ptr++; len++; // IPv6
      memcpy(ptr, _outStream->localAddress().toIPv6Address().c, 16); ptr += 16; len += 16;
    } else {
      logError() << "SOCKS: Local address of remote connnection is neither IPv4 nor IPv6 -> close.";
      close();
      return;
    }
    // Local port
    (*(uint16_t *)ptr) = htons(_outStream->localPort()); ptr+=2; len+=2;
    if(len != write((const char *)msg, len)) {
      logError() << "SOCKS: Cannot send response to client -> close.";
      close(); return;
    }
    // done.
    _state = CONNECTED;
  }
  // Process possible data at client side
  _clientReadyRead();
  // and remote side
  _remoteReadyRead();
}

void
SocksOutStream::_remoteReadyRead() {
  uint8_t buffer[DHT_STREAM_MAX_DATA_SIZE];
  // As long as there is data to read from the remote socket,
  // the connection is established and there is space left in the output buffer.
  while (_outStream->bytesAvailable() && ((1<<20)>bytesToWrite())) {
    qint64 len = std::min((1<<20)-bytesToWrite(),
                          qint64(DHT_STREAM_MAX_DATA_SIZE));
    // Read from remote
    if (0 < (len = _outStream->read((char *) buffer, len))) {
      // send to client
      this->write((const char *)buffer, len);
    }
  }
}

void
SocksOutStream::_remoteBytesWritten(qint64 bytes) {
  // As long as there is data left and the output buffer is not full
  while (bytesAvailable() && ((1<<20) > _outStream->bytesToWrite())) {
    uint8_t buffer[DHT_STREAM_MAX_DATA_SIZE];
    qint64 len = std::min(qint64(DHT_STREAM_MAX_DATA_SIZE),
                          (1<<20) - _outStream->bytesToWrite());
    // Read from client
    if (0 < (len = read((char *) buffer, DHT_STREAM_MAX_DATA_SIZE))) {
      // forward to remote
      _outStream->write((const char *)buffer, len);
    }
  }
}

void
SocksOutStream::_remoteReadChannelFinished() {
  logDebug() << "Remote transmission finished -> try to finish client data-transmission.";
  logDebug() << " " << (_outStream->bytesAvailable() + bytesToWrite()) << " not send to client yet.";
  logDebug() << " " << (_outStream->bytesToWrite() + bytesAvailable()) << " not send to remote yet.";
  _state = CLOSING;
  // ignore state-changes of and new data from remote connection.
  disconnect(_outStream, SIGNAL(readyRead()), this, SLOT(_remoteReadyRead()));
  disconnect(_outStream, SIGNAL(disconnected()), this, SLOT(_remoteDisconnected()));
  disconnect(_outStream, SIGNAL(error(QAbstractSocket::SocketError)),
             this, SLOT(_remoteError(QAbstractSocket::SocketError)));
}

void
SocksOutStream::_remoteDisconnected() {
  logInfo() << "SOCKS: Remote connection closed -> close proxy stream.";
  if (CLOSING != _state) {
    close();
  }
}

void
SocksOutStream::_remoteError(QAbstractSocket::SocketError error) {
  logInfo() << "SOCKS: Remote connection error: " << _outStream->errorString();
  logDebug() << " " << (_outStream->bytesAvailable() + bytesToWrite())
             << " (" << bytesToWrite() << ") not send to client yet.";
  logDebug() << " " << (_outStream->bytesToWrite() + bytesAvailable()) << " not send to remote yet.";
  if (QAbstractSocket::RemoteHostClosedError == error) {
    _state = CLOSING;
  } else if (isOpen()) {
    close();
  }
}

void
SocksOutStream::_clientReadyRead() {
  // As long as there is data left and the output buffer is not full
  while (bytesAvailable() && ((1<<20) > _outStream->bytesToWrite())) {
    uint8_t buffer[DHT_STREAM_MAX_DATA_SIZE];
    qint64 len = std::min(qint64(DHT_STREAM_MAX_DATA_SIZE),
                          (1<<20) - _outStream->bytesToWrite());
    // Read from client
    if (0 < (len = read((char *) buffer, DHT_STREAM_MAX_DATA_SIZE))) {
      // forward to remote
      _outStream->write((const char *)buffer, len);
    }
  }
}

void
SocksOutStream::_clientBytesWritten(qint64 bytes) {
  uint8_t buffer[DHT_STREAM_MAX_DATA_SIZE];
  // As long as there is data to read from the remote socket,
  // the connection is established and there is space left in the output buffer.
  while (_outStream->bytesAvailable() && ((1<<20)>bytesToWrite())) {
    qint64 len = std::min((1<<20)-bytesToWrite(),
                          qint64(DHT_STREAM_MAX_DATA_SIZE));
    // Read from remote
    if (0 < (len = _outStream->read((char *) buffer, len))) {
      // send to client
      this->write((const char *)buffer, len);
    }
  }
  // If closing and no data needs to be transferred -> close
  if ( (CLOSING == _state) && (0 == bytesToWrite())) {
    logDebug() << "Transmission finished -> close connection.";
    // All send -> done
    close();
  }
}

void
SocksOutStream::_clientClosed() {
  if (_outStream && _outStream->isOpen()) {
    logDebug() << "SOCKS: Connection to client closed: Close remote connection.";
    _outStream->close();
  }
}

