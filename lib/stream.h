#ifndef STREAM_H
#define STREAM_H

#include "crypto.h"
#include "utils.h"

#include <QTimer>

/** Specifies the maximum number of bytes that can be send with one packet. */
#define DHT_STREAM_MAX_DATA_SIZE (DHT_SEC_MAX_DATA_SIZE-5)


/** A ring buffer of size 64k (65536 bytes). This ring buffer can be implemented efficiently
 * using 2-complement integer arithmetic of 16-bit integers. Hence no modulo operation is needed. */
class FixedBuffer
{
public:
  /** Constructor. */
  FixedBuffer();

  /** Returns the number of bytes available for reading. */
  inline uint32_t available() const {
    return (_full ? 0x10000 : (_inptr-_outptr));
  }

  /** Returns the number of free bytes (available for writing). */
  inline uint32_t free() const {
    return (_full ? 0 : (_outptr-_inptr));
  }

  /** Reads some segement without removing it from the buffer. */
  inline uint32_t peek(uint16_t offset, uint8_t *buffer, uint32_t len) {
    // If empty or no byte requested -> done
    if (((_outptr==_inptr) && (!_full)) || (0 == len)) { return 0; }
    // If offset is larger than the available bytes -> done
    if (offset>=available()) { return 0; }
    // Determine howmany bytes to read
    len = std::min(uint32_t(offset)+len, available())-offset;
    // Get offset in terms of buffer index
    offset += _outptr;
    // read first half (at maximum up to the end of the buffer)
    uint32_t n = std::min(uint32_t(offset)+len, 0x10000U)-offset;
    memcpy(buffer, _buffer+offset, n);
    // read remaining bytes (wrap-around)
    memcpy(buffer+n, _buffer, len-n);
    // does not update outptr
    return len;
  }

  /** Reads from the ring buffer. */
  inline uint32_t read(uint8_t *buffer, uint32_t len) {
    len = peek(0, buffer, len);
    _outptr += len;
    return len;
  }

  /** Drops some data from the ring-buffer. */
  inline uint32_t drop(uint32_t len) {
    // If empty or no byte requested -> done
    if (((_outptr==_inptr) && (!_full)) || (0 == len)) { return 0; }
    len = std::min(uint32_t(_outptr)+len, available())-_outptr;
    _outptr += len;
    return len;
  }

  /** Puts some data in the already available area. */
  inline uint32_t put(uint16_t offset, const uint8_t *data, uint32_t len) {
    // If empty or no byte requested -> done
    if (((_outptr==_inptr) && (!_full)) || (0 == len)) { return 0; }
    // If offset is larger than the available bytes -> done
    if (offset>=available()) { return 0; }
    // Determine howmany bytes to put
    len = std::min(offset+len, available())-offset;
    // Get offset in terms of buffer index
    offset += _outptr;
    // put first half (at maximum up to the end of the buffer)
    uint32_t n = std::min(uint32_t(offset)+len, 0x10000U)-offset;
    memcpy(_buffer+offset, data, n);
    // put remaining bytes (wrap-around)
    memcpy(_buffer, data+n, len-n);
    // done
    return len;
  }

  /** Allocates some space at the end of the ring-buffer. */
  inline uint32_t allocate(uint32_t len) {
    len = std::min(len, free());
    _inptr += len;
    _full = (_inptr == _outptr);
    return len;
  }

  /** Appends some data to the ring buffer. */
  inline uint32_t write(const uint8_t *buffer, uint32_t len) {
    uint32_t offset = available();
    len = allocate(len);
    return put(offset, buffer, len);
  }

protected:
  /** The actual buffer. */
  uint8_t  _buffer[0xffff];
  /** Write pointer. */
  uint16_t _inptr;
  /** Read pointer. */
  uint16_t _outptr;
  /** If true, the buffer is full. */
  bool     _full;
};


/** Implements the input buffer of a TCP like stream. */
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

  /** Returns the number of bytes starting at the next expected sequence number (@c nextSequece)
   * the buffer will accept. */
  uint16_t window() const {
    if (0==_available) { return 0xffff; }
    return (0x10000-_available);
  }

  /** Reads some available data. */
  uint32_t read(uint8_t *buffer, uint32_t len) {
    len = std::min(len, _available);
    len = _buffer.read(buffer, len);
    _available -= len;
    return len;
  }

  /** Updates the internal buffer with the given data at the specified sequence number. */
  uint32_t putPacket(uint32_t seq, const uint8_t *data, uint32_t len) {
    // check if seq fits into window (buffer), if not -> done
    if (!_in_window(seq)) { return 0; }
    // Compute offset w.r.t. buffer-start where to store the data
    uint32_t offset = _available + (seq - _nextSequence);
    // Check if some space must be allocated
    if ((offset+len)>_buffer.available()) {
      // Get as much as possible
      len = _buffer.allocate((offset+len)-_buffer.available());
    }
    // The number of bytes that got available by this packet
    uint32_t newbytes = 0;
    // store in buffer
    len = _buffer.put(offset, data, len);
    // If seq is the expected one -> update
    if (_nextSequence == seq) {
      _nextSequence += len;
      _available    += len;
      newbytes      += len;
    } else if (0 == _packets.size()) {
      _packets.append(QPair<uint32_t, uint32_t>(seq, len));
    } else {
      // Insort according to sequence number
      uint32_t lastSeq = _nextSequence;
      int i=0;
      while ((i<_packets.size()) && (!_in_between(seq, lastSeq, _packets[i].first))) {
        lastSeq = _packets[i].first; i++;
      }
      _packets.insert(i, QPair<uint32_t, uint32_t>(seq, len));
    }
    // Update _nextSequence
    while ( (_packets.size()) && _in_packet(_nextSequence, _packets.first())) {
      _nextSequence = (_packets.first().first+_packets.first().second);
      _available += (_packets.first().second - (_nextSequence-_packets.first().first));
      newbytes   += (_packets.first().second - (_nextSequence-_packets.first().first));
      _packets.pop_front();
    }
    return newbytes;
  }

protected:
  /** Returns @c true if @c seq is within the interval [a,b) modulo 2^32. */
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
  /** The input buffer. */
  FixedBuffer _buffer;
  /** The number of bytes available for reading. */
  uint32_t _available;
  /** The next sequence number. */
  uint32_t _nextSequence;
  /** The received packets (sequence, length). */
  QVector< QPair<uint32_t, uint32_t> > _packets;
};


class StreamOutBuffer
{
public:
  /** Represents a packet that has been send to the remote host. */
  class Packet
  {
  public:
    /** Empty constructor. */
    inline Packet() : _length(0), _timestamp() { }
    /** Constructor.
     * @param seq Specifies the sequence number of the packet.
     * @param len Specifies the length of the packet. */
    inline Packet(size_t len) : _length(len), _timestamp(QDateTime::currentDateTime()) { }
    /** Copy constructor. */
    inline Packet(const Packet &other) : _length(other._length), _timestamp(other._timestamp) { }
    /** Assignement operator. */
    inline Packet &operator=(const Packet &other) {
      _length = other._length;
      _timestamp = other._timestamp;
      return *this;
    }

    /** Returns the length of the packet. */
    inline size_t length() const { return _length; }
    inline size_t leave(size_t len) { return (_length = std::min(len, _length));}

    /** Returns @c true if the packet is older than the specified number of milliseconds. */
    inline bool olderThan(size_t ms) const {
      return (_timestamp.addMSecs(ms)<QDateTime::currentDateTime());
    }
    /** Mark the packet as resend (updates the timestamp). */
    inline void markResend() {
      _timestamp = QDateTime::currentDateTime();
    }
    /** Returns the age of the packet. */
    inline uint32_t age() const {
      qint64 delta = _timestamp.msecsTo(QDateTime::currentDateTime());
      return (delta<0) ? 0 : delta;
    }

  protected:
    /** The length of the packet. */
    size_t    _length;
    /** The timestamp of the packet. */
    QDateTime _timestamp;
  };

public:
  StreamOutBuffer(uint64_t timeout);

  inline uint32_t free() const {
    return _buffer.free();
  }

  inline uint32_t bytesToWrite() const {
    return _buffer.available();
  }

  uint32_t nextSequence() const { return _nextSequence; }

  uint32_t write(const uint8_t *buffer, uint32_t len) {
    len = _buffer.write(buffer, len);
    if (len) {
      _packets.append(Packet(len));
      _nextSequence += len;
    }
    return len;
  }

  /** ACKs the given sequence number and returns the number of bytes removed from the output
   * buffer. */
  uint32_t ack(uint32_t seq) {
    // If the complete packetbuffer is ACKed
    if (_nextSequence == seq) {
      // Drop complete buffer
      _firstSequence = _nextSequence;
      if (_packets.size()) {
        _update_rt(_packets.first().age());
        _packets.clear();
      }
      return _buffer.drop(_buffer.available());
    }
    // Find the ACKed byte
    uint32_t drop = 0; uint32_t maxrt = 0;
    QList<Packet>::iterator item = _packets.begin();
    while (! _in_packet(seq, _firstSequence+drop, item->length())) {
      drop += item->length();
      maxrt = std::max(maxrt, item->age());
    }
    if (_packets.end() == item) { return 0; }
    // Update timeout
    _update_rt(std::max(maxrt, item->age()));
    // Check how much of the last packet was ACKed
    uint32_t left = (_firstSequence+drop+item->length())-seq;
    if (left) {
      // Handle partial ACK of packet
      drop += ( item->length()-left );
      item->leave(left);
    } else {
      // If packet is ACKed completely
      item++;
    }
    // Update first sequence
    _firstSequence = seq;
    // Erase everything upto but not including the given item
    _packets.erase(item);
    // Return number of bytes ACKed
    return _buffer.drop(drop);
  }

  bool resend(uint8_t *buffer, size_t &len, uint32_t &sequence) {
    sequence = _firstSequence; size_t offset = 0;
    QList<Packet>::iterator packet = _packets.begin();
    for (; packet != _packets.end(); packet++) {
      if (packet->olderThan(_timeout)) {
        len = _buffer.peek(offset, buffer, packet->length());
        packet->markResend();
        return true;
      }
      offset += packet->length();
      sequence += packet->length();
    }
    return false;
  }

protected:
  inline bool _in_between(uint32_t x, uint32_t a, uint32_t b) {
    return ( (a<b) ? ((a<x) && (x<=b)) : ((a<x) || (x<=b)) );
  }

  inline bool _in_packet(uint32_t x, uint32_t seq, uint32_t len) {
    return _in_between(x, seq, seq+len);
  }

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
  FixedBuffer   _buffer;
  uint32_t      _firstSequence;
  uint32_t      _nextSequence;
  QList<Packet> _packets;
  uint64_t      _rt_sum;
  uint64_t      _rt_sumsq;
  uint64_t      _rt_count;
  uint64_t      _timeout;
};


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
  StreamInBuffer  _inBuffer;
  /** The output buffer. */
  StreamOutBuffer _outBuffer;
  /** The window size of the remote. */
  uint16_t _window;
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
