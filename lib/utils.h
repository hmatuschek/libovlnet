#ifndef UTILS_H
#define UTILS_H

#include "ntp.h"
#include "pcp.h"
#include "natpmp.h"

#include <inttypes.h>
#include <QByteArray>
#include <QDateTime>
#include <QPair>


/** A simple ring buffer. */
class RingBuffer
{
public:
  /** Empty constructor (size=0). */
  RingBuffer();
  /** Constructs a ring buffer of the specified size. */
  explicit RingBuffer(size_t size);
  /** Copy constructor. */
  RingBuffer(const RingBuffer &other);
  /** Number of bytes that can be read. */
  size_t available() const;
  /** Number of bytes that can be stored in the buffer. */
  size_t free() const;
  /** Total size of the buffer. */
  size_t size() const;
  /** Peek at some data in the buffer without removing it. */
  size_t peek(size_t offset, uint8_t *buffer, size_t len) const;
  /** Read (and remove) some data from the buffer. */
  size_t read(QByteArray &buffer);
  /** Read (and remove) some data from the buffer. */
  size_t read(uint8_t *buffer, size_t len);
  /** Only remove some data from the buffer. */
  size_t drop(size_t len);

  /** Allocates some data in the buffer. */
  size_t allocate(size_t len);
  /** Writes some data to the buffer. */
  size_t write(const QByteArray &buffer);
  /** Writes some data to the buffer. */
  size_t write(const uint8_t *buffer, size_t len);
  /** Puts some data into the allocated area.
   * This method will not allocate more space. */
  size_t put(size_t offset, const uint8_t *buffer, size_t len);

protected:
  /** The buffer. */
  QByteArray _buffer;
  /** Index at which to put data. */
  size_t _inptr;
  /** Index at which to take data. */
  size_t _outptr;
  /** _inptr == _outptr can either mean the buffer is empty or it is full.
   * Hence a flag is needed to distinguish these cases. */
  bool _full;
};


class PacketOutBuffer
{
public:
  class Packet {
  public:
    inline Packet()
      : _sequence(0), _length(0), _timestamp() { }
    inline Packet(uint32_t seq, size_t len)
      : _sequence(seq), _length(len), _timestamp(QDateTime::currentDateTime()) { }
    inline Packet(const Packet &other)
      : _sequence(other._sequence), _length(other._length), _timestamp(other._timestamp) { }
    inline Packet &operator=(const Packet &other) {
      _sequence = other._sequence;
      _length = other._length;
      _timestamp = other._timestamp;
      return *this;
    }

    inline uint32_t sequence() const { return _sequence; }
    inline size_t length() const { return _length; }
    inline bool olderThan(size_t ms) const {
      return (_timestamp.addMSecs(ms)<QDateTime::currentDateTime());
    }
    inline void markResend() {
      _timestamp = QDateTime::currentDateTime();
    }

  protected:
    /** The sequence number of the packet. */
    uint32_t  _sequence;
    /** The length of the packet. */
    size_t    _length;
    /** The timestamp of the packet. */
    QDateTime _timestamp;
  };

public:
  PacketOutBuffer(size_t bufferSize, size_t timeout);

  size_t free() const;
  size_t available() const;
  uint32_t sequence() const;

  size_t write(const QByteArray &buffer);
  size_t write(const uint8_t *buffer, size_t len);

  size_t ack(uint32_t sequence);
  bool resend(uint8_t *buffer, size_t &len, uint32_t &sequence);

protected:
  RingBuffer _buffer;
  uint32_t _nextSequence;
  QList<Packet> _packets;
  size_t _timeout;
};


class PacketInBuffer
{
public:
  PacketInBuffer(size_t bufferSize);

  size_t available() const;
  size_t free() const;

  size_t read(QByteArray &buffer);
  size_t read(uint8_t *buffer, size_t len);

  bool putPacket(uint32_t &seq, const uint8_t *data, size_t len);

protected:
  /** The buffer of received packages. */
  RingBuffer _buffer;
  /** Next sequence number expected. */
  uint32_t   _nextSequence;
  /** The number of bytes in the buffer that have been received in sequence. */
  size_t     _available;
  /** The sequence numbers of packets stored in buffer (not ACKed yet). */
  QList< QPair<uint32_t, size_t> > _packets;
};

#endif // UTILS_H
