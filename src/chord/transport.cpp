#include "transport.h"
#include <QDebug>
#include <netinet/in.h>


/* ******************************************************************************************** *
 * Implementation of ConnectionTable
 * ******************************************************************************************** */
ConnectionTable::ConnectionTable()
  : _table()
{
  // pass...
}

ConnectionTable::~ConnectionTable() {
  QHash<FullAddress, Connection *>::iterator item = _table.begin();
  for (; item != _table.end(); item++) {
    delete item.value();
  }
  _table.clear();
}

bool
ConnectionTable::hasConnection(const QHostAddress &addr, uint16_t port) {
  return _table.contains(FullAddress(addr, port));
}

bool
ConnectionTable::hasConnection(Connection *conn) {
  return _table.contains(FullAddress(conn->address(), conn->port()));
}

Connection *
ConnectionTable::getConnection(const QHostAddress &addr, uint16_t port) {
  return _table[FullAddress(addr, port)];
}

void
ConnectionTable::addConnection(Connection *con) {
  _table.insert(FullAddress(con->address(), con->port()), con);
}

void
ConnectionTable::deleteConnection(Connection *con) {
  if (! _table.contains(FullAddress(con->address(), con->port()))) { return; }
  _table.remove(FullAddress(con->address(), con->port()));
  delete con;
}



/* ******************************************************************************************** *
 * Implementation of Connection
 * ******************************************************************************************** */
Connection::Connection(QUdpSocket *socket, const QHostAddress &addr, quint16 port, QObject *parent)
  : QObject(parent), _state(CLOSED), _socket(socket), _addr(addr), _port(port)
{
  // pass...
}

Connection::~Connection() {
  // pass...
}

Connection *
Connection::incomming(QUdpSocket *socket, const QHostAddress &addr, uint16_t port, uint32_t seq) {
  Connection *conn = new Connection(socket, addr, port);
  conn->_ack = seq+1; conn->_seq = rand();
  conn->_state = SYN_ACK_SEND;
  return conn;
}

Connection *
Connection::outgoing(QUdpSocket *socket, const QHostAddress &addr, uint16_t port) {
  Connection *conn = new Connection(socket, addr, port);
  conn->_seq = rand(); conn->_ack = 0;
  conn->_state = SYN_SEND;
  return conn;
}

void
Connection::process(const Message &msg, size_t size)
{
  // Dispatch by state
  switch (_state) {

  case CLOSED: {
    return;
  }

  case SYN_SEND: {
    // SYN has been send. Expect SYN/ACK
    if ( (!msg.syn_flag) || (!msg.ack_flag) || msg.fin_flag ) { return; }
    // check ack field
    if (msg.ack != (_seq+1)) { return; }
    // Store senders seq number
    _ack = msg.seq;
    _state = SYN_ACK_RCVD;
    return;
  }

  case SYN_ACK_SEND: {

  }
  }

  if (msg.ack_flag) {

  }
}


/* ******************************************************************************************** *
 * Implementation of Socket
 * ******************************************************************************************** */
Socket::Socket(const QHostAddress &addr, quint16 port, QObject *parent)
  : QObject(parent), _socket()
{
  if (!_socket.bind(addr, port)) {
    qDebug() << "Cannot bind to port" << addr << ":" << port;
    return;
  }

  QObject::connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onReadyRead()));
}

Socket::~Socket() {
  // pass...
}

bool
Socket::connect(const QHostAddress &addr, uint16_t port) {
  // Allocate connection object
  Connection *conn = Connection::outgoing(&_socket, addr, port);
  // add to table
  _connections.addConnection(conn);
  // assemble SYN message
  Message msg; memset(&msg, 0, 9);
  msg.ack_flag = 0;
  msg.fin_flag = 0;
  msg.syn_flag = 1;
  msg.seq = htonl(conn->seq());
  // send message
  //  on error -> "close" connection immediately
  if (9 != _socket.writeDatagram((const char *) &msg, 9, addr, port)) {
    _connections.deleteConnection(conn);
    return false;
  }
  return true;
}

void
Socket::_onReadyRead() {
  while (_socket.hasPendingDatagrams()) {
    if ( (_socket.pendingDatagramSize() > TRANSPORT_MAX_MSG_SIZE) ||
         (_socket.pendingDatagramSize() < 9)) {
      // Cannot be a message -> drop silently
      _socket.readDatagram(0,0);
    }

    // Read message
    Message msg; QHostAddress addr; uint16_t port;
    int64_t size = _socket.readDatagram((char *) &msg, TRANSPORT_MAX_MSG_SIZE, &addr, &port);
    if (0 > size) { continue; }

    // On incomming connection
    if (msg.syn_flag && (! msg.ack_flag) && (! msg.fin_flag)) {
      // If we are connected allready -> ignore
      if (_connections.hasConnection(addr, port)) { continue; }
      // Create new connection object
      Connection *conn = Connection::incomming(&_socket, addr, port, ntohl(msg.seq));
      // Add to table
      _connections.addConnection(conn);
      // Assemble SYN/ACK
      Message msg; memset(&msg, 0, 9);
      msg.ack_flag = 1;
      msg.syn_flag = 1;
      msg.fin_flag = 0;
      msg.ack = htonl(conn->ack());
      msg.seq = htonl(conn->seq());
      // send message
      //  on error -> "close" connection immediately
      if (9 != _socket.writeDatagram((const char *) &msg, 9, addr, port)) {
        _connections.deleteConnection(conn);
      }
    } else if (_connections.hasConnection(addr, port)) {
      _connections.getConnection(addr, port)->process(msg, size);
    }
  }
}

