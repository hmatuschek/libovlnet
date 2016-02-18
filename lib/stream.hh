#ifndef STREAM_H
#define STREAM_H

#include "crypto.hh"
#include <cmath>
#include <QTimer>


/** Specifies the maximum number of bytes that can be send with one packet. */
#define DHT_STREAM_MAX_DATA_SIZE (OVL_SEC_MAX_DATA_SIZE-5)


/** A ring buffer of size 64k (65535 bytes). This ring buffer can be implemented efficiently
 * using 2-complement integer arithmetic of 16-bit integers. Hence no modulo operation is needed.
 * @ingroup internal */
class FixedRingBuffer
{
public:
  /** Constructor. */
  FixedRingBuffer();

  /** Returns the number of bytes available for reading. */
  uint16_t available() const;

  /** Returns the number of free bytes (available for writing). */
  uint16_t free() const;

  /** Reads some segement without removing it from the buffer. */
  uint16_t peek(uint16_t offset, uint8_t *buffer, uint16_t len) const;

  /** Reads a single char without removing it from the buffer. */
  char peek(uint16_t offset) const;

  /** Reads from the ring buffer. */
  uint16_t read(uint8_t *buffer, uint16_t len);

  /** Drops some data from the ring-buffer. */
  uint16_t drop(uint16_t len);

  /** Puts some data in the already available area. */
  uint16_t put(uint16_t offset, const uint8_t *data, uint16_t len);

  /** Allocates some space at the end of the ring-buffer. */
  uint16_t allocate(uint16_t len);

  /** Appends some data to the ring buffer. */
  uint16_t write(const uint8_t *buffer, uint16_t len);

protected:
  /** The actual buffer. */
  uint8_t _buffer[0x10000];
  /** Read pointer. */
  uint16_t _outptr;
  /** Number of elements in the buffer. */
  uint16_t _size;
};


/** Implements the input buffer of a TCP like stream.
 * This buffer re-assembles the data stream by reordereing the received segments according to
 * their sequence number (call @c putPacket). Whenever a part of the sequence was received,
 * @c available increases and the received data can be @c read.
 * @ingroup internal */
class StreamInBuffer
{
public:
  /** Constructor. */
  StreamInBuffer();

  /** Returns the number of bytes available for reading. */
  uint16_t available() const;

  /** Returns the next expected sequence number. */
  uint32_t nextSequence() const;

  /** Returns the number of bytes starting at the next expected sequence number (@c nextSequence)
   * the buffer will accept. */
  uint16_t window() const;

  /** Searches for the given char in the available data. */
  bool contains(char c) const;

  /** Reads some ACKed data . */
  uint16_t read(uint8_t *buffer, uint16_t len);

  /** Updates the internal buffer with the given data at the specified sequence number. */
  uint32_t putPacket(uint32_t seq, const uint8_t *data, uint16_t len);

protected:
  /** Returns @c true if @c seq is within the interval [@c a, @c b) modulo 2^32. */
  static bool _in_between(uint32_t seq, uint32_t a, uint32_t b);

  /** Returns @c true if the sequence number is within the reception window. */
  bool _in_window(uint32_t seq) const;

  /** Returns @c true if the sequence number is within the given packet (sequence, len). */
  static inline bool _in_packet(uint32_t seq, const QPair<uint32_t, uint32_t> &packet);

protected:
  /** The input buffer (64kb). */
  FixedRingBuffer _buffer;
  /** The number of bytes available for reading. */
  uint16_t _available;
  /** The next sequence number. */
  uint32_t _nextSequence;
  /** The received packets (sequence, length). */
  QVector< QPair<uint32_t, uint32_t> > _packets;
};


/** Implements the output buffer (64k) of a TCP like data stream.
 * This buffer keeps track of the timeout of the first tranmitted but unACKed packet. It also
 * re-computes the timeout based on the time between sending a packet and receiving its ACK.
 * @ingroup internal */
class StreamOutBuffer
{
public:
  /** Constructor.
   * @param timeout Specifies the intial packet timeout in ms.*/
  StreamOutBuffer(uint64_t timeout);

  /** Returns the number of bytes that can be added to the buffer without exceeding the
   * reception window of the remote. */
  uint16_t free() const;

  /** Returns the number of bytes that are not ACKed yet. */
  uint16_t bytesToWrite() const;

  /** Sequence number of the first unACKed byte. */
  uint32_t firstSequence() const;

  /** Sequence number of the first byte of a segement that will be added to the buffer.
   * I.e. the sequence number of the last unACKed byte in buffer + 1. */
  uint32_t nextSequence() const;

  /** Writes some data to the buffer. */
  uint16_t write(const uint8_t *buffer, uint16_t len);

  /** ACKs the given sequence number and returns the number of bytes removed from the output
   * buffer. */
  uint32_t ack(uint32_t seq, uint16_t window);

  /** Returns the age of the oldest byte in the buffer. */
  uint64_t age() const;

  /** Returns @c true if the oldest byte in the buffer is older than the timeout. */
  bool timeout() const;

  /** Get the oldes bytes.
   * @param buffer The buffer, the data will be stored into.
   * @param len Length of the buffer, specifies the maximum number of bytes returned.
   * @param sequence On exit, holds the sequence number of the first byte in @c buffer.
   * @returns The number of bytes stored in @c buffer. */
  uint16_t resend(uint8_t *buffer, uint16_t len, uint32_t &sequence);

protected:
  /** Returns @c true if @c x is in (@c a, @c b]. */
  bool _in_between(uint32_t x, uint32_t a, uint32_t b) const;

  /** Updates the round trip time statistics. Every 64 samples, the timeout is updated. */
  inline void _update_rt(size_t ms);

protected:
  /** The ring buffer. */
  FixedRingBuffer   _buffer;
  /** The sequence number of the first byte in buffer. */
  uint32_t      _firstSequence;
  /** The sequence number of the next byte added to the buffer. */
  uint32_t      _nextSequence;
  /** Window sequence. */
  uint32_t      _window;
  /** Timestamp of the "oldest" byte in buffer. */
  QDateTime     _timestamp;
  /** Sum of round trip times. */
  uint64_t      _rt_sum;
  /** Sum of squares of round trip times. */
  uint64_t      _rt_sumsq;
  /** Number of round trip times in the sums. */
  uint64_t      _rt_count;
  /** Current timeout. */
  uint64_t      _timeout;
};


/** Implements a encrypted stream. While the @c SecureSocket implements encrypted datagrams
 * (UDP like), the secure stream implements an encrypted data stream (TCP like), handling packet
 * loss and maintaining the data order.
 * @ingroup core */
class SecureStream: public QIODevice, public SecureSocket
{
  Q_OBJECT

public:
  /** Possible states of the stream. */
  typedef enum {
    INITIALIZED,  ///< Stream is initialized, data may be received but and stored in the buffer,
                  ///  but no event is emitted.
    OPEN,         ///< Stream is open and data can be send or received. IO events are emitted.
    FIN_RECEIVED, ///< No data is received anymore but remaining data will be send. The state will
                  ///  change to CLOSED once all data has been send. This state can be reached if
                  ///  either a FIN packet is received or if @c close is called.
    CLOSED        ///< Stream closed, either by receiving a RST message or by having send all data
                  ///  remained at a call to @c close.
  } State;

public:
  /** Constructor.
   * @param dht A weak reference to the DHT instance.
   * @param parent The optional QObject parent. */
  SecureStream(Node &dht, QObject *parent=0);
  /** Destructor. */
  virtual ~SecureStream();

  /** Returns @c true. */
  bool isSequential() const;
  /** Open the stream, should be called if the connection has been established. */
  bool open(OpenMode mode);
  /** Close the stream. */
  void close();
  /** Reset the connection. */
  void abort();

  /** Returns the number of bytes in the input buffer. */
  qint64 bytesAvailable() const;
  /** Returns the number of bytes in the output buffer. */
  qint64 bytesToWrite() const;
  /** Returns @c true if the buffer contains "LF". */
  bool canReadLine() const;

signals:
  /** Gets emitted once the stream is established. */
  void established();
  /** Gets emiited if the connection fails. */
  void error();

protected:
  /** Starts the stream. Opens the @c QIODevice and emit @c established on success. */
  bool start(const Identifier &streamId, const PeerItem &peer);
  /** Gets called if the connection fails. */
  void failed();
  /** Gets called for every received decrypted datagram. */
  void handleDatagram(const uint8_t *data, size_t len);
  /** Read some data from the input buffer. */
  qint64 readData(char *data, qint64 maxlen);
  /** Write some data into the output buffer. */
  qint64 writeData(const char *data, qint64 len);

private slots:
  /** Gets called periodically to keep the connection alive. */
  void _onKeepAlive();
  /** Gets called periodically to check of segment timeout. */
  void _onCheckPacketTimeout();
  /** Gets called if the connection time-out. */
  void _onTimeOut();

private:
  /** The input buffer. */
  StreamInBuffer  _inBuffer;
  /** The output buffer. */
  StreamOutBuffer _outBuffer;
  /** The internal state of the connection. */
  State _state;
  /** Keep-alive timer. */
  QTimer _keepalive;
  /** Checks for packet timeouts. */
  QTimer _packetTimer;
  /** Signals loss of connection. */
  QTimer _timeout;
};


#endif // STREAM_H
