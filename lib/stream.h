#ifndef STREAM_H
#define STREAM_H

#include "crypto.h"
#include "utils.h"

#include <QTimer>

/** Specifies the maximum number of bytes that can be send with one packet. */
#define DHT_STREAM_MAX_DATA_SIZE (DHT_SEC_MAX_DATA_SIZE-5)

/** Implements a encrypted stream. While the @c SecureSocket implements encrypted datagrams
 * (UDP like), the secure stream implements an encrypted data stream (TCP like), handling packet
 * loss and maintaining the data order. */
class SecureStream: public QIODevice, public SecureSocket
{
  Q_OBJECT

public:
  /** Constructor.
   * @param dht A weak reference to the DHT instance.
   * @param parent The optional QObject parent. */
  SecureStream(DHT &dht, QObject *parent=0);
  /** Destructor. */
  virtual ~SecureStream();

  /** Returns @c true. */
  bool isSequential() const;
  /** Open the stream, should be called if the connection has been established. */
  bool open(OpenMode mode);
  /** Close the stream. */
  void close();

  /** Returns the number of bytes in the input buffer. */
  qint64 bytesAvailable() const;
  /** Returns the number of bytes that can be send to the remote. The returned value is
   * always <= @c DHT_STREAM_MAX_DATA_SIZE. */
  size_t canSend() const;
  /** Returns the number of bytes in the output buffer. */
  qint64 bytesToWrite() const;

protected:
  /** Gets called for every received decrypted datagram. */
  void handleDatagram(const uint8_t *data, size_t len);
  /** Read some data from the input buffer. */
  qint64 readData(char *data, qint64 maxlen);
  /** Write some data into the output buffer. */
  qint64 writeData(const char *data, qint64 len);

protected slots:
  /** Gets called periodically to keep the connection alive. */
  void _onKeepAlive();
  /** Gets called periodically to check of a packet time-out. */
  void _onCheckPacketTimeout();
  /** Gets called if the connection time-out. */
  void _onTimeOut();

protected:
  /** The input buffer. */
  PacketInBuffer  _inBuffer;
  /** The output buffer. */
  PacketOutBuffer _outBuffer;
  /** The window size of the remote. */
  size_t _window;
  /** If @c true the stream has been closed. */
  bool _closed;
  /** Keep-alive timer. */
  QTimer _keepalive;
  /** Checks for packet timeouts. */
  QTimer _packetTimer;
  /** Signals loss of connection. */
  QTimer _timeout;
};


#endif // STREAM_H
