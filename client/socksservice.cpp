#include "socksservice.h"
#include "application.h"
#include <QAbstractSocket>
#include <QHostAddress>


/* ********************************************************************************************* *
 * Implementation of SocksConnection
 * ********************************************************************************************* */
SocksConnection::SocksConnection(DHT &dht, QTcpSocket *instream, QObject *parent)
  : SOCKSInStream(dht, instream, parent)
{
  // pass...
}

void
SocksConnection::close() {
  SOCKSInStream::close();
  this->deleteLater();
}


/* ********************************************************************************************* *
 * Implementation of SocksService
 * ********************************************************************************************* */
SocksService::SocksService(Application &app, const NodeItem &remote, uint16_t port, QObject *parent)
  : QObject(parent), _application(app), _remote(remote), _server()
{
  // Bind socket to local port
  _server.listen(QHostAddress::LocalHost, port);

  connect(&_server, SIGNAL(newConnection()), this, SLOT(_onNewConnection()));
}

SocksService::~SocksService() {
  _server.close();
}

void
SocksService::_onNewConnection() {
  while (_server.hasPendingConnections()) {
    // Get for each incomming connection -> create a separate stream to the remote
    QTcpSocket *socket = _server.nextPendingConnection();
    _application.dht().startStream(5, _remote, new SocksConnection(_application.dht(), socket));
  }
}
