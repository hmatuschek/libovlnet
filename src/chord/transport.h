#ifndef __VLF_CHORD_TRANSPORT_H__
#define __VLF_CHORD_TRANSPORT_H__

#include <QObject>
#include <QHostAddress>
#include <QUdpSocket>
#include <inttypes.h>
#include <QHash>

#define TRANSPORT_MAX_MSG_SIZE 512
#define TRANSPORT_HEADER_SIZE  11
#define TRANSPORT_MAX_PAYLOAD (TRANSPORT_MAX_MSG_SIZE - TRANSPORT_HEADER_SIZE)


// Forward declarations
class Connection;

/** Simplified TCP message. */
typedef struct {
  /** SYN flag. */
  uint8_t  syn_flag : 1;
  /** ACK flag. */
  uint8_t  ack_flag : 1;
  /** FIN flag. */
  uint8_t  fin_flag : 1;
  uint32_t ack;
  uint32_t seq;
  uint16_t window;
  char     payload[TRANSPORT_MAX_PAYLOAD];
} Message;


class MessageQueue {
  class Item {

  };
};


class Connection: public QObject
{
  Q_OBJECT

public:
  typedef enum {
    CLOSED,
    SYN_SEND,
    SYN_ACK_RCVD,
    SYN_ACK_SEND,
    CONNECTED,
  } State;

protected:
  Connection(QUdpSocket *socket, const QHostAddress &addr, quint16 port, QObject *parent=0);

public:
  virtual ~Connection();

  inline const QHostAddress &address() const { return _addr; }
  inline uint16_t port() const { return _port; }

protected:
  static Connection *incomming(QUdpSocket *socket, const QHostAddress &addr, uint16_t port, uint32_t seq);
  static Connection *outgoing(QUdpSocket *socket, const QHostAddress &addr, uint16_t port);

  inline uint32_t seq() const { return _seq; }
  inline uint32_t ack() const { return _ack; }

  void process(const Message &msg, size_t size);

protected:
  State _state;
  QUdpSocket *_socket;
  QHostAddress _addr;
  uint16_t _port;

  QByteArray _inbuffer;
  QByteArray _outbuffer;

  /** The next message to request. */
  uint32_t _ack;
  /** The last message send. */
  uint32_t _seq;

  friend class Socket;
};


class ConnectionTable
{
public:
  typedef QPair<QHostAddress, uint16_t> FullAddress;

public:
  ConnectionTable();
  virtual ~ConnectionTable();

  bool hasConnection(const QHostAddress &addr, uint16_t port);
  bool hasConnection(Connection *connection);
  Connection *getConnection(const QHostAddress &addr, uint16_t port);
  void addConnection(Connection *con);

  void deleteConnection(Connection *con);
  void deleteConnection(const QHostAddress &addr, uint16_t port);

protected:
  QHash<FullAddress, Connection *> _table;
};


class Socket: public QObject
{
  Q_OBJECT

public:
  explicit Socket(const QHostAddress &addr, quint16 port, QObject *parent=0);
  virtual ~Socket();

public slots:
  bool connect(const QHostAddress &addr, uint16_t port);

signals:
  void connected(Connection *connection);
  void connectionFailed(Connection *connection);

protected slots:
  void _onReadyRead();

protected:
  QUdpSocket _socket;
  ConnectionTable _connections;
};


#endif // __VLF_CHORD_TRANSPORT_H__
