#ifndef __VLF_DHT_H__
#define __VLF_DHT_H__

#include "buckets.h"

#include <inttypes.h>

#include <QObject>
#include <QUdpSocket>
#include <QPair>
#include <QVector>
#include <QSet>
#include <QTimer>

// Forward declarations
struct Message;
class Request;
class FindNodeQuery;
class FindValueQuery;
class Request;
class PingRequest;
class FindNodeRequest;
class FindValueRequest;
class StartStreamRequest;

class Identity;
class SocketHandler;
class SecureSocket;


/** Implements a node in the DHT. */
class DHT: public QObject
{
  Q_OBJECT

public:
  /** Constructor.
   * @param id Specifies the identity of the node.
   * @param addr Specifies the network address the node will bind to.
   * @param port Specifies the network port the node will listen on.
   * @param parent Optional pararent object. */
  explicit DHT(Identity &id, SocketHandler *streamHandler=0,
               const QHostAddress &addr=QHostAddress::Any, quint16 port=7741, QObject *parent=0);

  /** Destructor. */
  virtual ~DHT();

  /** Returns a weak reference it the identity of the node. */
  Identity &identity();
  /** Returns a weak reference it the identity of the node. */
  const Identity &identity() const;

  /** Returns the identifier of the DHT node. */
  const Identifier &id() const;

  /** Returns the number of nodes in the buckets. */
  size_t numNodes() const;
  /** Returns the list of all nodes in the buckets. */
  void nodes(QList<NodeItem> &lst);

  /** Returns the number of keys held by this DHT node. */
  size_t numKeys() const;
  /** Retunrs the number of data items provided by this node. */
  size_t numData() const;

  /** Retunrs the number of active streams. */
  size_t numStreams() const;

  /** Returns the number of bytes send. */
  size_t bytesSend() const;
  /** Returns the number of bytes received. */
  size_t bytesReceived() const;
  /** Returns the download rate. */
  double inRate() const;
  /** Returns the upload rate. */
  double outRate() const;

  /** Sends a ping request to the given peer. */
  void ping(const QString &addr, uint16_t port);
  /** Sends a ping request to the given peer. */
  void ping(const QHostAddress &addr, uint16_t port);
  /** Sends a ping request to the given peer. */
  void ping(const PeerItem &peer);

  /** Starts the search for a node with the given identifier. */
  void findNode(const Identifier &id);

  /** Starts the search for a value with the given identifier. */
  void findValue(const Identifier &id);
  /** Announces a value. */
  void announce(const Identifier &id);

  /** Starts a secure connection.
   * The ownership of the @c SecureSocket instance is passed to the DHT. */
  bool startStream(uint16_t service, const NodeItem &node, SecureSocket *stream);
  /** Unregister the socket with the DHT instance. */
  void socketClosed(const Identifier &id);

signals:
  /** Gets emitted if a ping was replied. */
  void nodeReachable(const NodeItem &node);
  /** Gets emitted if the given node has been found. */
  void nodeFound(const NodeItem &node);
  /** Gets emitted if the given node was not found. */
  void nodeNotFound(const Identifier &id, const QList<NodeItem> &best);
  /** Gets emitted if the given value was found. */
  void valueFound(const Identifier &id, const QList<NodeItem> &nodes);
  /** Gets emitted if the given value was not found. */
  void valueNotFound(const Identifier &id);

protected:
  /** Sends a FindNode message to the node @c to to search for the node specified by @c id.
   * Any response to that request will be forwarded to the specified @c query. */
  void sendFindNode(const NodeItem &to, FindNodeQuery *query);
  /** Sends a FindValue message to the node @c to to search for the node specified by @c id.
   * Any response to that request will be forwarded to the specified @c query. */
  void sendFindValue(const NodeItem &to, FindValueQuery *query);
  /** Returns @c true if the given identifier belongs to a value being announced. */
  bool isPendingAnnouncement(const Identifier &id) const;
  /** Sends an Annouce message to the given node. */
  void sendAnnouncement(const NodeItem &to, const Identifier &what);

private:
  /** Processes a Ping response. */
  void _processPingResponse(const Message &msg, size_t size, PingRequest *req,
                            const QHostAddress &addr, uint16_t port);
  /** Processes a FindNode response. */
  void _processFindNodeResponse(const Message &msg, size_t size, FindNodeRequest *req,
                                const QHostAddress &addr, uint16_t port);
  /** Processes a FindValue response. */
  void _processFindValueResponse(const Message &msg, size_t size, FindValueRequest *req,
                                const QHostAddress &addr, uint16_t port);
  /** Processes a StartStream response. */
  void _processStartStreamResponse(const Message &msg, size_t size, StartStreamRequest *req,
                                   const QHostAddress &addr, uint16_t port);
  /** Processes a Ping request. */
  void _processPingRequest(const Message &msg, size_t size,
                           const QHostAddress &addr, uint16_t port);
  /** Processes a FindNode request. */
  void _processFindNodeRequest(const Message &msg, size_t size,
                               const QHostAddress &addr, uint16_t port);
  /** Processes a FindValue request. */
  void _processFindValueRequest(const Message &msg, size_t size,
                                const QHostAddress &addr, uint16_t port);
  /** Processes an Announce request. */
  void _processAnnounceRequest(const Message &msg, size_t size,
                               const QHostAddress &addr, uint16_t port);
  /** Processes a StartStream request. */
  void _processStartStreamRequest(const Message &msg, size_t size,
                                  const QHostAddress &addr, uint16_t port);

private slots:
  /** Gets called on the reception of a UDP package. */
  void _onReadyRead();
  /** Gets called regulary to check the request timeouts. */
  void _onCheckRequestTimeout();
  /** Gets called regulary to check the timeout of the node in the buckets. */
  void _onCheckNodeTimeout();
  /** Gets called regulary to check the announcement timeouts. */
  void _onCheckAnnouncementTimeout();  
  /** Gets called regulary to update the statistics. */
  void _onUpdateStatistics();
  /** Gets called when some data has been send. */
  void _onBytesWritten(qint64 n);

protected:
  /** The identifier of the node. */
  Identity &_self;
  /** The network socket. */
  QUdpSocket _socket;

  /** The number of bytes received. */
  size_t _bytesReceived;
  /** The number of bytes received at the last update. */
  size_t _lastBytesReceived;
  /** The input rate. */
  double _inRate;

  /** The number of bytes send. */
  size_t _bytesSend;
  /** The number of bytes send at the last update. */
  size_t _lastBytesSend;
  /** The output rate. */
  double _outRate;

  /** The routing table. */
  Buckets _buckets;
  /** A list of candidate peers to join the buckets. */
  QList<PeerItem> _candidates;

  /** The key->value map of the received announcements. */
  QHash<Identifier, QHash<Identifier, AnnouncementItem> > _announcements;
  /** The kay->timestamp table of the data this node provides. */
  QHash<Identifier, QDateTime> _announcedData;
  /** The list of pending requests. */
  QHash<Identifier, Request *> _pendingRequests;

  /** Socket handler instance. */
  SocketHandler *_streamHandler;
  /** The list of open connection. */
  QHash<Identifier, SecureSocket *> _streams;

  /** Timer to check timeouts of requests. */
  QTimer _requestTimer;
  /** Timer to check nodes in buckets. */
  QTimer _nodeTimer;
  /** Timer to keep announcements up-to-date. */
  QTimer _announcementTimer;
  /** Timer to update i/o statistics every 5 seconds. */
  QTimer _statisticsTimer;
};



#endif // __VLF_DHT_H__
