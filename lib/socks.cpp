#include "socks.h"
#include <netinet/in.h>
#include <QHostInfo>


/* ******************************************************************************************** *
 * Implementation of SOCKSInStream
 * ******************************************************************************************** */
SOCKSLocalStream::SOCKSLocalStream(DHT &dht, QTcpSocket *instream, QObject *parent)
  : SecureStream(dht, parent), _inStream(instream)
{
  // Take ownership of TCP socket.
  _inStream->setParent(this);
}

SOCKSLocalStream::~SOCKSLocalStream() {
  _inStream->close();
  if (isOpen()) {
    SOCKSLocalStream::close();
  }
}

bool
SOCKSLocalStream::open(OpenMode mode) {
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
SOCKSLocalStream::_clientReadyRead() {
  uint8_t buffer[DHT_SEC_MAX_DATA_SIZE-5];
  size_t len = DHT_SEC_MAX_DATA_SIZE-5;
  len = std::min(len, size_t(_inStream->bytesAvailable()));
  len = std::min(len, this->outBufferFree());
  if (len) {
    len = _inStream->read((char *)buffer, len);
    this->write((const char *)buffer, len);
  }
}

void
SOCKSLocalStream::_clientBytesWritten(qint64 bytes) {
  uint8_t buffer[DHT_SEC_MAX_DATA_SIZE-5];
  size_t len = DHT_SEC_MAX_DATA_SIZE-5;
  len = std::min(len, size_t(bytesAvailable()));
  if (len) {
    len = read((char *)buffer, len);
    _inStream->write((const char *)buffer, len);
  }
}

void
SOCKSLocalStream::_clientDisconnected() {
  logDebug() << "Client disconnected -> close SOCKS stream";
  close();
}

void
SOCKSLocalStream::_clientError(QAbstractSocket::SocketError error) {
  logDebug() << "Client connection error: " << _inStream->errorString()
             << " -> close socks stream";
  _inStream->close();
  close();
}

void
SOCKSLocalStream::_remoteReadyRead() {
  uint8_t buffer[DHT_SEC_MAX_DATA_SIZE-5];
  size_t len = DHT_SEC_MAX_DATA_SIZE-5;
  len = std::min(len, size_t(bytesAvailable()));
  if (len) {
    len = read((char *)buffer, len);
    _inStream->write((const char *)buffer, len);
  }
}

void
SOCKSLocalStream::_remoteBytesWritten(qint64 bytes) {
  uint8_t buffer[DHT_SEC_MAX_DATA_SIZE-5];
  size_t len = DHT_SEC_MAX_DATA_SIZE-5;
  len = std::min(len, size_t(_inStream->bytesAvailable()));
  len = std::min(len, this->outBufferFree());
  if (len) {
    len = _inStream->read((char *)buffer, len);
    this->write((const char *)buffer, len);
  }
}

void
SOCKSLocalStream::_remoteClosed() {
  if (_inStream && _inStream->isOpen()) {
    logDebug() << "SOCKS connection closed -> close client connection.";
    _inStream->close();
  }
}


/* ******************************************************************************************** *
 * Implementation of SOCKSOutStream
 * ******************************************************************************************** */
SOCKSOutStream::SOCKSOutStream(DHT &dht, QObject *parent)
  : SecureStream(dht, parent), _state(RX_VERSION), _outStream(0),
    _nAuthMeth(0), _authMeth(), _addr(), _nHostName(0), _hostName(), _port(0)
{
  // pass...
}

SOCKSOutStream::~SOCKSOutStream() {
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
SOCKSOutStream::open(OpenMode mode) {
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
SOCKSOutStream::_clientParse() {
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
SOCKSOutStream::_remoteConnected() {
  if (CONNECTING == _state) {
    logDebug() << "SOCKS: Remote " << _addr << ":" << _port << " connected -> start proxy session";
    // connect proxy client
    connect(this, SIGNAL(readyRead()), this, SLOT(_clientReadyRead()));
    connect(this, SIGNAL(bytesWritten(qint64)), this, SLOT(_clientBytesWritten(qint64)));
    // Connect to remote signals
    connect(_outStream, SIGNAL(readyRead()), this, SLOT(_remoteReadyRead()));
    connect(_outStream, SIGNAL(bytesWritten(qint64)), this, SLOT(_remoteBytesWritten(qint64)));
    // Send OK response
    uint8_t msg[DHT_SEC_MAX_DATA_SIZE-5];
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
SOCKSOutStream::_remoteReadyRead() {
  // As long as there is data to read from the remote socket,
  // the connection is established and there is space left in the output buffer.
  while (_outStream->bytesAvailable() && (CONNECTED == _state) && outBufferFree()) {
    uint8_t buffer[DHT_SEC_MAX_DATA_SIZE-5];
    int len = std::min(qint64(outBufferFree()),
                       std::min(qint64(DHT_SEC_MAX_DATA_SIZE-5),
                                _outStream->bytesAvailable()));
    if (0 >= len) { return; }
    len = _outStream->read((char *) buffer, len);
    if (len != this->write((const char *)buffer, len)) {
      logError() << "SOCKS: Dataloss: Remote (" << len
                 <<"b) -> Remote. => Close connection.";
      close(); return;
    }
  }
}

void
SOCKSOutStream::_remoteBytesWritten(qint64 bytes) {
  // As long as there is data left send by the client
  while (this->bytesAvailable()) {
    uint8_t buffer[DHT_SEC_MAX_DATA_SIZE-5];
    int len = this->read((char *) buffer, DHT_SEC_MAX_DATA_SIZE-5);
    if (0 >= len) { return; }
    /// @bug What if _outStream buffer is full?
    if (len != _outStream->write((const char *)buffer, len)) {
      logError() << "SOCKS: Dataloss: Client -> Remote. => Close connection.";
      close(); return;
    }
  }
}

void
SOCKSOutStream::_remoteReadChannelFinished() {
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
SOCKSOutStream::_remoteDisconnected() {
  logInfo() << "SOCKS: Remote connection closed -> close proxy stream.";
  if (CLOSING != _state) {
    close();
  }
}

void
SOCKSOutStream::_remoteError(QAbstractSocket::SocketError error) {
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
SOCKSOutStream::_clientReadyRead() {
  while (this->bytesAvailable() && (CONNECTED == _state)) {
    uint8_t buffer[DHT_SEC_MAX_DATA_SIZE-5];
    int len = this->read((char *) buffer, DHT_SEC_MAX_DATA_SIZE-5);
    if (0 >= len) { return; }
    if (len != _outStream->write((const char *)buffer, len)) {
      logError() << "SOCKS: Dataloss: Client -> Remote. => Close connection.";
      close(); return;
    }
  }
}

void
SOCKSOutStream::_clientBytesWritten(qint64 bytes) {
  while (_outStream->bytesAvailable())  {
    if (CLOSING == _state) {
      logDebug() << "SOCKS: Connection closing -> send "
                 << _outStream->bytesAvailable() << " remaining bytes.";
    }
    uint8_t buffer[DHT_SEC_MAX_DATA_SIZE-5];
    int len = std::min(qint64(outBufferFree()),
                       std::min(qint64(DHT_SEC_MAX_DATA_SIZE-5),
                                _outStream->bytesAvailable()));
    if (0 == len) { return; }
    len = _outStream->read((char *) buffer, len);
    if ( len != write((const char *)buffer, len) ) {
      logError() << "SOCKS: Dataloss: Remot -> Client. => Close connection.";
      close(); return;
    }
  }
  // If closing and no data needs to be transferred -> close
  if ( (CLOSING == _state) && (0 == this->bytesToWrite())) {
    logDebug() << "Transmission finished -> close connection.";
    // All send -> done
    close();
  }
}

void
SOCKSOutStream::_clientClosed() {
  if (_outStream && _outStream->isOpen()) {
    logDebug() << "SOCKS: Connection to client closed: Close remote connection.";
    _outStream->close();
  }
}

