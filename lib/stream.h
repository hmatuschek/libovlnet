#ifndef STREAM_H
#define STREAM_H

#include "crypto.h"
#include "utils.h"

/** Implements a encrypted stream. While the @c SecureSocket implements encrypted datagrams
 * (UDP like), the secure stream implements an encrypted data stream (TCP like), handling packet
 * loss and maintaining the data order.
 * @bug Implement keep-alive pings. */
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
  virtual bool isSequential() const;
  /** Open the stream, should be called if the connection has been established. */
  virtual bool open(OpenMode mode);
  /** Close the stream. */
  virtual void close();
  /** Retruns the number of bytes in the input buffer. */
  virtual qint64 bytesAvailable() const;
  size_t outBufferFree() const;
  /** Retruns the number of bytes in the output buffer. */
  virtual qint64 bytesToWrite() const;
  size_t inBufferFree() const;

protected:
  /** Gets called for every received decrypted datagram. */
  virtual void handleDatagram(const uint8_t *data, size_t len);
  /** Read some data from the input buffer. */
  virtual qint64 readData(char *data, qint64 maxlen);
  /** Write some data into the output buffer. */
  virtual qint64 writeData(const char *data, qint64 len);

protected:
  /** The input buffer. */
  PacketInBuffer  _inBuffer;
  /** The output buffer. */
  PacketOutBuffer _outBuffer;
  /** If @c true the stream has been closed. */
  bool _closed;
};


#endif // STREAM_H
