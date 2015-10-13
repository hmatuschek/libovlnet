#include "socksservice.h"
#include "application.h"
#include <QAbstractSocket>
#include <QHostAddress>


/* ********************************************************************************************* *
 * Implementation of SocksConnection
 * ********************************************************************************************* */
SocksConnection::SocksConnection(DHT &dht, QTcpSocket *instream, QObject *parent)
  : SOCKSLocalStream(dht, instream, parent)
{
  // pass...
}

void
SocksConnection::close() {

  SOCKSLocalStream::close();
  this->deleteLater();
}


/* ********************************************************************************************* *
 * Implementation of SocksService
 * ********************************************************************************************* */
SocksService::SocksService(Application &app, const NodeItem &remote, uint16_t port, QObject *parent)
  : QObject(parent), _application(app), _remote(remote), _server()
{
  logDebug() << "Start SOCKS proxy service at localhost:" << port;
  // Bind socket to local port
  if(! _server.listen(QHostAddress::LocalHost, port)) {
    logError() << "SOCKS: Can not bind to localhost:" << port;
  }
  connect(&_server, SIGNAL(newConnection()), this, SLOT(_onNewConnection()));
}

SocksService::~SocksService() {
  _server.close();
}

bool
SocksService::isListening() const {
  return _server.isListening();
}

size_t
SocksService::connectionCount() const {
  return _connectionCount;
}


void
SocksService::_onNewConnection() {
  while (_server.hasPendingConnections()) {
    // Get for each incomming connection -> create a separate stream to the remote
    QTcpSocket *socket = _server.nextPendingConnection();
    logDebug() << "New incomming SOCKS connection...";
    SocksConnection *conn = new SocksConnection(_application.dht(), socket);
    connect(conn, SIGNAL(destroyed()), this, SLOT(_onConnectionClosed()));
    _connectionCount++;
    emit connectionCountChanged(_connectionCount);
    _application.dht().startStream(5, _remote, conn);
  }
}


void
SocksService::_onConnectionClosed() {
  if (_connectionCount)
    _connectionCount--;
  emit connectionCountChanged(_connectionCount);
}
