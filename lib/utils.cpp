#include "utils.h"

/* ********************************************************************************************* *
 * Implementation of RingBuffer
 * ********************************************************************************************* */
RingBuffer::RingBuffer()
  : _buffer(), _inptr(0), _outptr(0), _full(false)
{
  // pass...
}

RingBuffer::RingBuffer(size_t size)
  : _buffer(size, 0), _inptr(0), _outptr(0), _full(false)
{
  // pass...
}

RingBuffer::RingBuffer(const RingBuffer &other)
  : _buffer(other._buffer), _inptr(other._inptr), _outptr(other._outptr), _full(other._full)
{
  // pass...
}

size_t
RingBuffer::available() const {
  if ((_outptr<=_inptr) && (!_full)) {
    return _inptr-_outptr;
  }
  return _inptr + (_buffer.size() - _outptr);
}

size_t
RingBuffer::free() const {
  if ((_outptr<=_inptr) && (!_full)) {
    return _outptr+(_buffer.size()-_inptr);
  }
  return (_outptr-_inptr);
}

size_t
RingBuffer::size() const {
  return _buffer.size();
}

size_t
RingBuffer::read(QByteArray &buffer) {
  return read((uint8_t *)buffer.data(), buffer.size());
}

size_t
RingBuffer::read(uint8_t *buffer, size_t len) {
  // If empty or no byte requested -> done
  if (((_inptr == _outptr) && (!_full)) || (0 == len)) { return 0; }
  // Determine how many bytes to read
  size_t nread = std::min(available(), len);
  if ((_outptr < _inptr) || (int(_outptr+nread)<=_buffer.size())) {
    // w/o wrap-around
    memcpy(buffer, _buffer.data()+_outptr, nread);
    _outptr = ( (_outptr + nread) % _buffer.size() );
    _full = false;
    return nread;
  }
  // with wrap around:
  // -> read first half up to buffer end
  size_t n = _buffer.size()-_outptr;
  memcpy(buffer, _buffer.data()+_outptr, n);
  _outptr=0; _full=false;
  // then read second half
  return read(buffer+n, nread-n)+n;
}

size_t
RingBuffer::drop(size_t len) {
  if (((_inptr == _outptr) && (!_full)) || (0 == len)) { return 0; }
  // Determine how many bytes to read
  len = std::min(available(), len);
  if ((_outptr < _inptr) || (int(_outptr+len)<=_buffer.size())) {
    _outptr = ((_outptr + len) % _buffer.size());
    return len;
  }
  // with wrap around:
  size_t n= _buffer.size()-_outptr;
  _outptr = len-n;
  return len;
}

size_t
RingBuffer::peek(size_t offset, uint8_t *buffer, size_t len) const {
  // If empty or no byte requested -> done
  if (((_inptr == _outptr) && (!_full)) || (0 == len)) { return 0; }
  // If offset is larger than available data -> done
  if (offset>=available()) { return 0; }
  // Get howmany bytes to read
  len = std::min(available(), offset+len)-offset;
  // Compute offset w.r.t buffer index
  offset = (_outptr + offset) % _buffer.size();
  if (offset < _inptr) {
    memcpy(buffer, _buffer.constData()+offset, len);
    return len;
  }
  // with wrap around
  size_t n = (_buffer.size()-offset);
  memcpy(buffer, _buffer.constData()+offset, n);
  memcpy(buffer+n, _buffer.constData(), len-n);
  return len;
}

size_t
RingBuffer::write(const QByteArray &buffer) {
  return write((const uint8_t *)buffer.constData(), buffer.size());
}

size_t
RingBuffer::write(const uint8_t *buffer, size_t len) {
  // Determine number of bytes to write
  size_t nwrite = std::min(free(), len);
  if (0 == nwrite) { return 0; }
  if (int(_inptr+nwrite) <= _buffer.size()) {
    // no wrap around
    memcpy(_buffer.data(), buffer, nwrite);
    _inptr = (_inptr + nwrite) % _buffer.size();
    _full = (_inptr == _outptr);
    return nwrite;
  }
  // with wrap around
  size_t n = (_buffer.size()-_inptr);
  // fill-up buffer to end
  memcpy(_buffer.data(), buffer, n);
  _inptr = 0; _full = (_inptr == _outptr);
  return write(buffer+n, nwrite-n) + n;
}

size_t
RingBuffer::allocate(size_t len) {
  len = std::min(free(), len);
  if (0 == len) { return 0; }
  _inptr = (_inptr+len) % _buffer.size();
  _full = (_inptr == _outptr);
  return len;
}

size_t
RingBuffer::put(size_t offset, const uint8_t *buffer, size_t len) {
  // If empty or no byte requested -> done
  if (((_inptr == _outptr) && (!_full)) || (0 == len)) { return 0; }
  // If offset is larger than available data -> done
  if (offset>=available()) { return 0; }
  // Get howmany bytes to write
  len = std::min(available(), offset+len)-offset;
  // Compute offset w.r.t buffer index
  offset = (_outptr + offset) % _buffer.size();
  if (offset < _inptr) {
    memcpy(_buffer.data()+offset, buffer, len);
    return len;
  }
  // with wrap around
  size_t n = (_buffer.size()-offset);
  memcpy(_buffer.data()+offset, buffer, n);
  memcpy(_buffer.data(), buffer+n, len-n);
  return len;
}


/* ********************************************************************************************* *
 * Implementation of PacketOutBuffer
 * ********************************************************************************************* */
PacketOutBuffer::PacketOutBuffer(size_t bufferSize, size_t timeout)
  : _buffer(bufferSize), _nextSequence(0), _packets()
{
  // pass...
}

size_t
PacketOutBuffer::free() const {
  return _buffer.free();
}

size_t
PacketOutBuffer::available() const {
  return _buffer.available();
}

uint32_t
PacketOutBuffer::sequence() const {
  return _nextSequence;
}

size_t
PacketOutBuffer::write(const QByteArray &buffer) {
  return write((const uint8_t *)buffer.constData(), buffer.size());
}

size_t
PacketOutBuffer::write(const uint8_t *buffer, size_t len) {
  len = _buffer.write(buffer, len);
  _packets.append(Packet(_nextSequence, len)); _nextSequence += len;
  return len;
}

size_t
PacketOutBuffer::ack(uint32_t sequence) {
  size_t drop=0;
  QList<Packet>::iterator packet = _packets.begin();
  for (; packet != _packets.end(); packet++) {
    // If sequence matches -> done
    if (packet->sequence() == sequence) {
      drop += packet->length();
      _buffer.drop(drop);
      _packets.erase(_packets.begin(), ++packet);
      return drop;
    }
    drop += packet->length();
  }
  return 0;
}

bool
PacketOutBuffer::resend(uint8_t *buffer, size_t &len, uint32_t &sequence) {
  size_t offset = 0;
  QList<Packet>::iterator packet = _packets.begin();
  for (; packet != _packets.end(); packet++) {
    if (packet->olderThan(_timeout)) {
      _buffer.peek(offset, buffer, packet->length());
      len = packet->length(); sequence = packet->sequence();
      packet->markResend();
      return true;
    }
    offset += packet->length();
  }
  return false;
}


/* ********************************************************************************************* *
 * Implementation of PacketInBuffer
 * ********************************************************************************************* */
PacketInBuffer::PacketInBuffer(size_t bufferSize)
  : _buffer(bufferSize), _nextSequence(0), _available(0), _packets()
{
  // pass..
}

size_t
PacketInBuffer::available() const {
  return _available;
}

size_t
PacketInBuffer::read(QByteArray &buffer) {
  return read((uint8_t *)buffer.data(), buffer.size());
}

size_t
PacketInBuffer::read(uint8_t *buffer, size_t len) {
  len = std::min(_available, len);
  if (0 == len) { return 0; }
  len = _buffer.read(buffer, len);
  _available -= len;
  return len;
}

// Retruns @c true if x is [a,b] % 2 ** 32
inline bool
__inBetweenSeq(uint32_t x, uint32_t a, uint32_t b) {
  return ( (a < b) ?
             ( (a<=x) && (x<=b) ) :
             ( ((a<=x) && (x<=std::numeric_limits<uint32_t>::max())) || (x<=b) ) );
}

bool
PacketInBuffer::putPacket(uint32_t &seq, const uint8_t *data, size_t len) {
  // Compute the offset of where the packet should be stored in the buffer
  size_t offset = (seq >= _nextSequence) ?
        (seq - _nextSequence) :
        (seq + (size_t(std::numeric_limits<uint32_t>::max())-_nextSequence+1));
  // Check if packet fits into buffer (somehow)
  if ((_available+offset+len)>_buffer.size()) { return false; }
  // Allocate space in buffer if needed
  if ((_available+offset+len)>_buffer.available()) {
    _buffer.allocate((_available+offset+len)-_buffer.available());
  }
  // put packet
  _buffer.put(offset, data, len);
  if (0 == _packets.size()) {
    // Simply append
    _packets.append(QPair<uint32_t, size_t>(seq, len));
  } else {
    // insort package sequence number
    uint32_t lastSeq = _nextSequence;
    QList< QPair<uint32_t, size_t> >::iterator item = _packets.begin();
    for (; item != _packets.end(); item++) {
      if (__inBetweenSeq(seq, lastSeq, item->first)) { break; }
    }
    _packets.insert(item, QPair<uint32_t, size_t>(seq, len));
  }
  // ACK continous data (there is at least one element in the list)
  while ((_packets.size()) && (_nextSequence == _packets.front().first)) {
    // Get sequence number of next expected packet
    seq = _packets.front().first;
    // mark payload as available
    _available += _packets.front().second;
    // compute next expected sequence number
    _nextSequence += _packets.front().second;
    // remove processed packet
    _packets.pop_front();
  }
  return true;
}


