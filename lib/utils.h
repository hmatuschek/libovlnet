#ifndef UTILS_H
#define UTILS_H

#include <inttypes.h>
#include <QByteArray>
#include <QDateTime>
#include <QPair>


class RingBuffer
{
public:
  RingBuffer();
  explicit RingBuffer(size_t size);
  RingBuffer(const RingBuffer &other);

  size_t available() const;
  size_t free() const;
  size_t size() const;

  size_t peek(size_t offset, uint8_t *buffer, size_t len) const;
  size_t read(QByteArray &buffer);
  size_t read(uint8_t *buffer, size_t len);
  size_t drop(size_t len);

  size_t allocate(size_t len);
  size_t write(const QByteArray &buffer);
  size_t write(const uint8_t *buffer, size_t len);
  size_t put(size_t offset, uint8_t *buffer, size_t len);

protected:
  /** The buffer. */
  QByteArray _buffer;
  /** Index at which to put data. */
  size_t _inptr;
  /** Index at which to take data. */
  size_t _outptr;
  /** _inptr == _outptr can either mean the buffer is empty or it is full. Hence a flag is needed
   * to distinguish these cases. */
  bool _full;
};


class PacketOutBuffer
{
public:
  class Packet {
  public:
    Packet()
      : _sequence(0), _length(0), _timestamp() { }
    Packet(uint32_t seq, size_t len)
      : _sequence(seq), _length(len), _timestamp(QDateTime::currentDateTime()) { }
    Packet(const Packet &other)
      : _sequence(other._sequence), _length(other._length), _timestamp(other._timestamp) { }
    Packet &operator=(const Packet &other) {
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

  size_t read(QByteArray &buffer);
  size_t read(uint8_t *buffer, size_t len);

  bool putPacket(uint32_t &seq, uint8_t *data, size_t len);

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
