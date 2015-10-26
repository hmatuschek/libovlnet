#ifndef STREAM_H
#define STREAM_H

#include "crypto.h"
#include <cmath>
#include <QTimer>


/** Specifies the maximum number of bytes that can be send with one packet. */
#define DHT_STREAM_MAX_DATA_SIZE (DHT_SEC_MAX_DATA_SIZE-5)


/** A ring buffer of size 64k (65536 bytes). This ring buffer can be implemented efficiently
 * using 2-complement integer arithmetic of 16-bit integers. Hence no modulo operation is needed.
 * @ingroup internal */
class FixedRingBuffer
{
public:
  /** Constructor. */
  FixedRingBuffer();

  /** Returns the number of bytes available for reading. */
  inline uint32_t available() const {
    // If full -> 2^16, else (inptr-outptr) modulo 2^16.
    // The latter works even if outptr > inptr.
    return (_full ? 0x10000 : uint16_t(_inptr-_outptr));
  }

  /** Returns the number of free bytes (available for writing). */
  inline uint32_t free() const {
    // If empty -> 2^16, else (outptr-inptr) modulo 2^16.
    // The latter works even if inptr > outptr.
    if ((!_full) && (_outptr==_inptr)) { return 0x10000; }
    return uint16_t(_outptr-_inptr);
  }

  /** Reads some segement without removing it from the buffer. */
  inline uint32_t peek(uint16_t offset, uint8_t *buffer, uint32_t len) {
    // If empty or no byte requested -> done
    if (((_outptr==_inptr) && (!_full)) || (0 == len)) { return 0; }
    // If offset is larger than the available bytes -> done
    if (offset>=available()) { return 0; }
    // Determine howmany bytes to read
    len = std::min(uint32_t(offset)+len, available())-offset;
    // Get offset in terms of buffer index == (offset+_outptr) modulo 2^16
    offset += _outptr;
    // read first half (at maximum up to the end of the buffer)
    uint32_t n = ( std::min(uint32_t(offset)+len, 0x10000U) - offset );
    memcpy(buffer, _buffer+offset, n);
    // read remaining bytes (wrap-around)
    memcpy(buffer+n, _buffer, len-n);
    // does not update outptr
    return len;
  }

  /** Reads from the ring buffer. */
  inline uint32_t read(uint8_t *buffer, uint32_t len) {
    // Read some data from the buffer
    len = peek(0, buffer, len);
    // Drop some data
    return drop(len);
  }

  /** Drops some data from the ring-buffer. */
  inline uint32_t drop(uint32_t len) {
    // If empty or no byte requested -> done
    if (((_outptr==_inptr) && (!_full)) || (0 == len)) { return 0; }
    // Get bytes to drop
    len = std::min(len, available());
    // drop data
    _outptr += len;
    // Update _full flag
    if (len) { _full = false; }
    return len;
  }

  /** Puts some data in the already available area. */
  inline uint32_t put(uint16_t offset, const uint8_t *data, uint32_t len) {
    // If empty or no byte requested -> done
    if (((_outptr==_inptr) && (!_full)) || (0 == len)) { return 0; }
    // If offset is larger than the available bytes -> done
    if (offset>=available()) { return 0; }
    // Determine howmany bytes to put
    len = std::min(uint32_t(offset)+len, available())-offset;
    // Get offset in terms of buffer index
    offset += _outptr;
    // put first half (at maximum up to the end of the buffer)
    uint32_t n = ( std::min(uint32_t(offset)+len, 0x10000U) - offset );
    memcpy(_buffer+offset, data, n);
    // put remaining bytes (wrap-around)
    memcpy(_buffer, data+n, len-n);
    // done
    return len;
  }

  /** Allocates some space at the end of the ring-buffer. */
  inline uint32_t allocate(uint32_t len) {
    // Howmany bytes can be allocated
    len = std::min(len, free());
    // Allocate data
    _inptr += len;
    // Update full ptr;
    _full = (_inptr == _outptr);
    // done.
    return len;
  }

  /** Appends some data to the ring buffer. */
  inline uint32_t write(const uint8_t *buffer, uint32_t len) {
    // Where to put the data
    uint32_t offset = available();
    // Allocate some space
    len = allocate(len);
    // store data
    return put(offset, buffer, len);
  }

protected:
  /** The actual buffer. */
  uint8_t _buffer[0x10000];
  /** Write pointer. */
  uint16_t _inptr;
  /** Read pointer. */
  uint16_t _outptr;
  /** If true, the buffer is full. */
  bool _full;
};


/** Implements the input buffer of a TCP like stream.
 * This buffer re-assembles the data stream by reordereing the received segments according to
 * their sequence number (call @c putPacket). Whenever a part of the sequence was received,
 * @c availabl() increases and the received data can be @c read.
 * @ingroup internal */
class StreamInBuffer
{
public:
  /** Constructor. */
  StreamInBuffer();

  /** Returns the number of bytes available for reading. */
  inline uint32_t available() const {
    return _available;
  }

  /** Returns the next expected sequence number. */
  uint32_t nextSequence() const {
    return _nextSequence;
  }

  /** Returns the number of bytes starting at the next expected sequence number (@c nextSequence)
   * the buffer will accept. */
  uint16_t window() const {
    if (0xffff <= _available) { return 0; }
    return (0xffff-_available);
  }

  /** Reads some ACKed data . */
  uint32_t read(uint8_t *buffer, uint32_t len) {
    len = std::min(len, _available);
    len = _buffer.read(buffer, len);
    _available -= len;
    return len;
  }

  /** Updates the internal buffer with the given data at the specified sequence number. */
  uint32_t putPacket(uint32_t seq, const uint8_t *data, uint32_t len) {
    // check if seq fits into window [_nextSequence, _nextSequence+window()), if not -> done
    if (!_in_window(seq)) { return 0; }
    // Compute offset w.r.t. buffer-start, where to store the data
    uint32_t offset = _available + uint32_t(seq - _nextSequence);
    // If offset >= buffer size -> done
    if (offset >= 0x10000) { return 0; }
    // Check if some space must be allocated
    if ((offset+len)>_buffer.available()) {
      // Get as much as possible
      _buffer.allocate((offset+len)-_buffer.available());
    }
    // store in buffer, data will be truncated if not enougth space was allocated
    if (0 == (len = _buffer.put(offset, data, len)) ) { return 0; }
    // Store packet in queue
    if (0 == _packets.size()) {
      // if queue is empty -> append
      _packets.append(QPair<uint32_t, uint32_t>(seq, len));
    } else {
      // Insort according to sequence number
      uint32_t lastSeq = _nextSequence; int i=0;
      while ((i<_packets.size()) && (!_in_between(seq, lastSeq, _packets[i].first))) {
        lastSeq = _packets[i].first; i++;
      }
      _packets.insert(i, QPair<uint32_t, uint32_t>(seq, len));
    }
    // Get number of bytes that got available by this packet
    uint32_t newbytes = 0;
    while ( (_packets.size()) && _in_packet(_nextSequence, _packets.first())) {
      uint32_t acked = ((_packets.first().first+_packets.first().second)-_nextSequence);
      _nextSequence  = (_packets.first().first+_packets.first().second);
      _available    += acked;
      newbytes      += acked;
      _packets.pop_front();
    }
    return newbytes;
  }

protected:
  /** Returns @c true if @c seq is within the interval [@c a, @c b) modulo 2^32. */
  static inline bool _in_between(uint32_t seq, uint32_t a, uint32_t b) {
    return ( (a<b) ? ((a<=seq) && (seq<b)) : ((a<=seq) || (seq<b)) );
  }

  /** Returns @c true if the sequence number is within the reception window. */
  bool _in_window(uint32_t seq) {
    uint32_t a = _nextSequence;
    uint32_t b = (_nextSequence+window());
    return _in_between(seq, a, b);
  }

  /** Returns @c true if the sequence number is within the given packet (sequence, len). */
  static inline bool _in_packet(uint32_t seq, const QPair<uint32_t, uint32_t> &packet) {
    return _in_between(seq, packet.first, packet.first+packet.second);
  }

protected:
  /** The input buffer (64kb). */
  FixedRingBuffer _buffer;
  /** The number of bytes available for reading. */
  uint32_t _available;
  /** The next sequence number. */
  uint32_t _nextSequence;
  /** The received packets (sequence, length). */
  QVector< QPair<uint32_t, uint32_t> > _packets;
};


/** Implements the output buffer (64k) of a TCP like data stream.
 * This buffer keeps track of the timeout of the first tranmitted but unACKed packet. It also
 * re-computes the timeout bases on the time between sending a packet and receiving its ACK.
 * @ingroup internal */
class StreamOutBuffer
{
public:
  /** Constructor.
   * @param timeout Specifies the intial packet timeout in ms.*/
  StreamOutBuffer(uint64_t timeout);

  /** Returns the number of bytes that can be added to the buffer without exceeding the
   * reception window of the remote. */
  inline uint32_t free() const {
    // Do not exceed the reception window of the remote
    if (_buffer.available() > _window) { return 0; }
    // Do not exceed the buffer size.
    return std::min(_buffer.free(), (uint32_t(_window)-_buffer.available()));
  }

  /** Returns the number of bytes that are not ACKed yet. */
  inline uint32_t bytesToWrite() const {
    return _buffer.available();
  }

  /** Sequence number of the first unACKed byte. */
  uint32_t firstSequence() const { return _firstSequence; }
  /** Sequence number of the first byte of a segement that will be added to the buffer,
   * i.e. the sequence number of the last unACKed byte in buffer + 1. */
  uint32_t nextSequence() const { return _nextSequence; }

  /** Writes some data to the buffer. */
  uint32_t write(const uint8_t *buffer, uint32_t len) {
    // store in ring-buffer
    if ( (len = _buffer.write(buffer, std::min(free(), len))) ) {
      // Update timestamp if buffer was empty
      if (_firstSequence == _nextSequence) {
        _timestamp = QDateTime::currentDateTime();
      }
      // update next sequence number.
      _nextSequence += len;
    }
    // return length of bytes added.
    return len;
  }

  /** ACKs the given sequence number and returns the number of bytes removed from the output
   * buffer. */
  uint32_t ack(uint32_t seq, uint16_t window) {
    // Find the ACKed byte
    uint32_t drop = 0;
    if (_in_between(seq, _firstSequence, _nextSequence)) {
      // howmany bytes dropped
      drop = uint32_t(seq-_firstSequence);
      // update round-trip time
      _update_rt(age());
      // Update timestamp of "oldest" bytes
      _timestamp = QDateTime::currentDateTime();
      // Update first sequence
      _firstSequence = seq;
      // update window
      _window = window;
    }
    // Return number of bytes ACKed
    return _buffer.drop(drop);
  }

  /** Returns the age of the oldest byte in the buffer. */
  inline uint64_t age() const {
    int64_t age = _timestamp.msecsTo(QDateTime::currentDateTime());
    return ((age>0) ? age : 0);
  }

  /** Returns @c true if the oldest byte in the buffer is older than the timeout. */
  inline bool timeout() const {
    return (age() > _timeout);
  }

  /** Get the oldes bytes.
   * @param buffer The buffer, the data will be stored into.
   * @param len Length of the buffer, specifies the maximum number of bytes returned.
   * @param sequence On exit, holds the sequence number of the first byte in @c buffer.
   * @returns The number of bytes stored in @c buffer. */
  uint32_t resend(uint8_t *buffer, uint32_t len, uint32_t &sequence) {
    // Set sequence
    sequence = _firstSequence;
    len = _buffer.peek(0, buffer, len);
    // update the timestamp of the oldest byte
    _timestamp = QDateTime::currentDateTime();
    // Return the number of bytes stored in the buffer
    return len;
  }

protected:
  /** Returns @c true if @c x is in (@c a, @c b]. */
  inline bool _in_between(uint32_t x, uint32_t a, uint32_t b) {
    return ( (a<b) ? ((a<x) && (x<=b)) : ((a<x) || (x<=b)) );
  }

  /** Updates the round trip time statistics. Every 64 samples, the timeout is updated. */
  inline void _update_rt(size_t ms) {
    // Update sums
    _rt_sum += ms; _rt_sumsq  += ms*ms; _rt_count++;
    // If sufficient data is available
    //  -> update timeout as mean + 3*sd (99% quantile)
    if ((1<<6) == _rt_count) {
      // Integer division by (1<<6)=64
      _rt_sum >>= 6; _rt_sumsq  >>= 6;
      // compute new timeout
      _timeout = _rt_sum + 3*std::sqrt(_rt_sumsq -_rt_sum*_rt_sum);
      // reset counts and sums
      _rt_sum = 0; _rt_sumsq = 0; _rt_count = 0;
    }
  }

protected:
  /** The ring buffer. */
  FixedRingBuffer   _buffer;
  /** The sequence number of the first byte in buffer. */
  uint32_t      _firstSequence;
  /** The sequence number of the next byte added to the buffer. */
  uint32_t      _nextSequence;
  /** Window w.r.t @c _firstSequence. */
  uint16_t      _window;
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
  SecureStream(DHT &dht, QObject *parent=0);
  /** Destructor. */
  virtual ~SecureStream();

  /** Returns @c true. */
  bool isSequential() const;
  /** Open the stream, should be called if the connection has been established. */
  bool open(OpenMode mode);
  /** Close the stream. */
  void close();
  /** Reset the connection. */
  void cancel();

  /** Returns the number of bytes in the input buffer. */
  qint64 bytesAvailable() const;
  /** Returns the number of bytes in the output buffer. */
  qint64 bytesToWrite() const;

protected:
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
