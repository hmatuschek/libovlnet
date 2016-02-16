#include "stream.h"

#include "dht.h"
#include <netinet/in.h>

/** The format of the stream messages. */
struct __attribute__((packed)) Message
{
  /** Possible stream message types. */
  typedef enum {
    DATA = 0,  ///< A data packet.
    ACK,       ///< Acknowledgement of some received data.
    RESET,     ///< (Hard) Reset the connection.
    FIN        ///< No further data will be transmitted.
  } Flags;

  /** The message type. */
  uint8_t  type;
  /** The sequential number. */
  uint32_t seq;
  /** The message payload, either some data if type=DATA or the window size if type=ACK. */
  union __attribute__ ((packed)) {
    /** The number of bytes the receiver is willing to accept. */
    uint16_t window;
    /** Payload. */
    uint8_t  data[DHT_STREAM_MAX_DATA_SIZE];
  } payload;

  /** Constructor. */
  inline Message(Flags type) {
    memset(this, 0, sizeof(Message));
    this->type = type;
  }
};


/* ********************************************************************************************* *
 * Implementation of FixedBuffer
 * ********************************************************************************************* */
FixedRingBuffer::FixedRingBuffer()
  : _outptr(0), _size(0)
{
  // pass...
}

uint16_t
FixedRingBuffer::available() const {
  return _size;
}

uint16_t
FixedRingBuffer::free() const {
  return 0xffff-_size;
}

uint16_t
FixedRingBuffer::peek(uint16_t offset, uint8_t *buffer, uint16_t len) const {
  // If offset is larger than the available bytes -> done
  if (offset>=available()) { return 0; }
  // Determine howmany bytes to read
  len = std::min(uint32_t(offset)+len, uint32_t(available()))-offset;
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

uint16_t
FixedRingBuffer::read(uint8_t *buffer, uint16_t len) {
  // Read some data from the buffer
  len = peek(0, buffer, len);
  // Drop some data
  return drop(len);
}

uint16_t
FixedRingBuffer::drop(uint16_t len) {
  // Get bytes to drop
  len = std::min(len, available());
  // drop data
  _outptr += len;
  _size -= len;
  return len;
}

uint16_t
FixedRingBuffer::put(uint16_t offset, const uint8_t *data, uint16_t len) {
  // If offset is larger than the available bytes -> done
  if (offset>=available()) { return 0; }
  // Determine how many bytes to put
  len = std::min(uint32_t(offset)+len, uint32_t(available()))-offset;
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

uint16_t
FixedRingBuffer::allocate(uint16_t len) {
  // Howmany bytes can be allocated
  len = std::min(len, free());
  // Allocate data
  _size += len;
  // done.
  return len;
}

uint16_t
FixedRingBuffer::write(const uint8_t *buffer, uint16_t len) {
  // Where to put the data
  uint32_t offset = available();
  // Allocate some space
  len = allocate(len);
  // store data
  return put(offset, buffer, len);
}


/* ********************************************************************************************* *
 * Implementation of StreamInBuffer
 * ********************************************************************************************* */
StreamInBuffer::StreamInBuffer()
  : _buffer(), _available(0), _nextSequence(0), _packets()
{
  // pass...
}

uint16_t
StreamInBuffer::read(uint8_t *buffer, uint16_t len) {
  len = std::min(len, _available);
  len = _buffer.read(buffer, len);
  _available -= len;
  return len;
}

uint32_t
StreamInBuffer::putPacket(uint32_t seq, const uint8_t *data, uint16_t len) {
  // check if sequence matches expected sequence
  if (_nextSequence != seq) {
    logDebug() << "StreamInBuffer: Ignore packet seq=" << seq
               << ": Not matching expected seq=" << _nextSequence;
    return 0;
  }
  // store in buffer
  len = _buffer.write(data, len);
  // Update sequence
  _nextSequence += len;
  _available += len;
  return len;

  // check if seq fits into window [_nextSequence, _nextSequence-_available+window()), if not -> done
  /*if (! _in_window(seq)) {
    logDebug() << "StreamInBuffer: Ignore packet seq=" << seq
               << ", len=" << len << ": Not in window: ["
               << _nextSequence << ", " << (_nextSequence+window()) << "].";
    return 0;
  }
  // Compute offset w.r.t. buffer-start, where to store the data
  uint32_t offset = _available + uint32_t(seq - _nextSequence);
  // If offset >= buffer size -> done
  if (offset >= 0xffff) {
    logError() << "StreamInBuffer: Ignore packet out of buffer range. (This should not happen!)";
    return 0;
  }
  // Check if some space must be allocated
  if ((offset+len)>_buffer.available()) {
    // Get as much as possible
    _buffer.allocate((offset+len)-_buffer.available());
  }
  // store in buffer, data will be truncated if not enougth space was allocated
  if (0 == (len = _buffer.put(offset, data, len)) ) {
    return 0;
  }
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
  while ( _packets.size() && _in_packet(_nextSequence, _packets.first())) {
    uint32_t acked = ((_packets.first().first+_packets.first().second)-_nextSequence);
    _nextSequence  = (_packets.first().first+_packets.first().second);
    /// @bug Ohh, this can be wrong!
    _available    += acked;
    newbytes      += acked;
    _packets.pop_front();
  }
  return newbytes;*/
}


/* ********************************************************************************************* *
 * Implementation of StreamOutBuffer
 * ********************************************************************************************* */
StreamOutBuffer::StreamOutBuffer(uint64_t timeout)
  : _buffer(), _firstSequence(0), _nextSequence(0), _window(0xffff), _timestamp(),
    _rt_sum(0), _rt_sumsq(0), _rt_count(0), _timeout(timeout)
{
  // pass...
}

uint16_t
StreamOutBuffer::free() const {
  return _window-_firstSequence;
}

uint16_t
StreamOutBuffer::bytesToWrite() const {
  return _nextSequence - _firstSequence;
}

uint16_t
StreamOutBuffer::write(const uint8_t *buffer, uint16_t len) {
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

uint32_t
StreamOutBuffer::ack(uint32_t seq, uint16_t window) {
  // Find the ACKed byte
  uint32_t drop = 0;
  if (_in_between(seq, _firstSequence, _nextSequence)) {
    // howmany bytes dropped
    drop = seq-_firstSequence;
    logDebug() << "ACKed seq=" << seq << " -> dropped " << drop << "b.";
    // update round-trip time
    _update_rt(age());
    // Update timestamp of "oldest" bytes
    _timestamp = QDateTime::currentDateTime();
    // Update first sequence
    _firstSequence = seq;
    // update window
    _window = _firstSequence+window;
  } else {
    logDebug() << "Ignore ACK seq=" << seq << ", window=" << window
               << ": not in range [" << _firstSequence << ", " << _nextSequence << "].";
  }
  // Return number of bytes ACKed
  return _buffer.drop(drop);
}

uint64_t
StreamOutBuffer::age() const {
  int64_t age = _timestamp.msecsTo(QDateTime::currentDateTime());
  return ((age>0) ? age : 0);
}

uint16_t
StreamOutBuffer::resend(uint8_t *buffer, uint16_t len, uint32_t &sequence) {
  // Set sequence
  sequence = _firstSequence;
  len = _buffer.peek(0, buffer, len);
  // update the timestamp of the oldest byte
  _timestamp = QDateTime::currentDateTime();
  // Return the number of bytes stored in the buffer
  return len;
}

void
StreamOutBuffer::_update_rt(size_t ms) {
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


/* ******************************************************************************************** *
 * Implementation of SecureStream
 * ******************************************************************************************** */
SecureStream::SecureStream(DHT &dht, QObject *parent)
  : QIODevice(parent), SecureSocket(dht), _inBuffer(), _outBuffer(2000),
    _state(INITIALIZED), _keepalive(), _packetTimer(), _timeout()
{
  // Setup keep-alive timer, gets started by open();
  _keepalive.setInterval(1000);
  _keepalive.setSingleShot(false);
  // Setup packet timeout timer
  _packetTimer.setInterval(100);
  _packetTimer.setSingleShot(false);
  // Setup connection timeout timer.
  _timeout.setInterval(10000);
  _timeout.setSingleShot(true);

  // connect signals
  connect(&_keepalive, SIGNAL(timeout()), this, SLOT(_onKeepAlive()));
  connect(&_packetTimer, SIGNAL(timeout()), this, SLOT(_onCheckPacketTimeout()));
  connect(&_timeout, SIGNAL(timeout()), this, SLOT(_onTimeOut()));
}

SecureStream::~SecureStream() {
  // pass...
}

bool
SecureStream::isSequential() const {
  return true;
}

bool
SecureStream::open(OpenMode mode) {
  bool ok = QIODevice::open(mode);
  if ((INITIALIZED == _state) && ok) {
    _keepalive.start();
    _timeout.start();
    _state = OPEN;
  }
  return (OPEN == _state) && ok;
}

void
SecureStream::_onKeepAlive() {
  if (CLOSED == _state) {
    _keepalive.stop();
    return;
  }
  // send "keep-alive" ping (ACK=next expected sequence)
  Message resp(Message::ACK);
  // Set sequence
  resp.seq = htonl(_inBuffer.nextSequence());
  // Set window size
  resp.payload.window = htons(_inBuffer.window());
  if (! sendDatagram((const uint8_t*) &resp, 7)) {
    logWarning() << "SecureStream: Failed to send ACK.";
  }
}

void
SecureStream::_onCheckPacketTimeout() {
  if ((! _outBuffer.bytesToWrite()) || (! _outBuffer.timeout())) { return; }
  // Resent some data
  Message msg(Message::DATA); uint32_t seq=0;
  uint32_t len = _outBuffer.resend(msg.payload.data, DHT_STREAM_MAX_DATA_SIZE, seq);
  msg.seq = htonl(seq);
  if (sendDatagram((const uint8_t *) &msg, len+5)) {
    logDebug() << "Resend seq=" << seq << ", len=" << len << ".";
    _keepalive.start();
  } else {
    logWarning() << "SecureStream: Failed to resend data: seq=" << seq << ", len=" << len << ".";
  }
}

void
SecureStream::_onTimeOut() {
  logInfo() << "SecureStream: Connection timeout -> reset connection.";
  // abort connection
  cancel();
}

void
SecureStream::close() {
  // If already closed -> done
  if (CLOSED == _state) { return; }

  // close QIODevice
  QIODevice::close();

  // If state is open
  if (OPEN == _state) {
    // readcanel finished
    emit readChannelFinished();
    _state = FIN_RECEIVED;
    // If all data has been send -> closed
    if (0 == bytesToWrite()) {
      // sends RST & closes the connection
      cancel();
    }
  }
}

void
SecureStream::cancel() {
  // Stop keep alive timer
  _keepalive.stop();
  // Stop packet timer.
  _packetTimer.stop();
  // Stop timeout timer
  _timeout.stop();

  // Make sure the stream does not get notified anymore.
  _dht.socketClosed(id());

  // close QIODevice
  QIODevice::close();

  // Send reset packet
  if (CLOSED != _state) {
    logDebug() << "SecureStream: Close connection, send RST.";
    _state = CLOSED;
    Message msg(Message::RESET);
    if(! sendDatagram((uint8_t *) &msg, 1)) {
      logError() << "SecureConnection: Can not send RST packet.";
    }
  }
}

qint64
SecureStream::bytesAvailable() const {
  // IO device buffer + internal packet-buffer
  return _inBuffer.available() + QIODevice::bytesAvailable();
}

qint64
SecureStream::bytesToWrite() const {
  // IO device buffer + internal packet-buffer
  return _outBuffer.bytesToWrite() + QIODevice::bytesToWrite();
}

qint64
SecureStream::writeData(const char *data, qint64 len) {
  // shortcut
  if (0 == len) { return 0; }
  //qint64 inlen = len;
  // Determine maximum data length as the minimum of
  // maximum length (len), space in output buffer, window-size of the remote,
  // and maximum payload length
  len = std::min(len, qint64(_outBuffer.free()));
  len = std::min(len, qint64(DHT_STREAM_MAX_DATA_SIZE));
  if (0 == len) {
    //logDebug() << "Do not send data len=" << inlen << ": window=" << _outBuffer.free() << ".";
    return 0;
  }

  // Assemble message
  Message msg(Message::DATA);
  // store seq number
  msg.seq = htonl(_outBuffer.nextSequence());

  // put in output buffer, updates sequence number
  if(0 == (len = _outBuffer.write((const uint8_t *)data, len))) {
    logDebug() << "Cannot write to outBuffer. Full?";
    return 0;
  }
  logDebug() << "Send seq=" << ntohl(msg.seq) << ", len=" << len << ".";

  // If some data was added to the buffer
  // and the packet timer is not started -> start it
  if (! _packetTimer.isActive()) { _packetTimer.start(); }

  // store data in message
  memcpy(msg.payload.data, data, len);

  // send message
  if( sendDatagram((const uint8_t *)&msg, len+5) ) {
    // reset keep-alive timer
    _keepalive.start();
    return len;
  }
  logError() << "Can not send datagram!";
  // on error
  return -1;
}

qint64
SecureStream::readData(char *data, qint64 maxlen) {
  return _inBuffer.read((uint8_t *)data, std::min(maxlen, qint64(0x10000)));
}

bool
SecureStream::start(const Identifier &streamId, const PeerItem &peer) {
  if (SecureSocket::start(streamId, peer)) {
    bool res = open(QIODevice::ReadWrite);
    if (res) { emit established(); }
    return res;
  }
  return false;
}

void
SecureStream::handleDatagram(const uint8_t *data, size_t len) {
  // Restart time-out timer
  _timeout.start();

  // Ignore null-packets
  if (0 == len) { return; }

  // Check size and unpack message
  if (len > sizeof(Message)) { return; }
  const Message *msg = (const Message *)data;

  /* Dispatch by type */
  if (Message::DATA == msg->type) {
    // check message size
    if (len<5) { return; }
    // Get sequence number of data packet
    uint32_t seq = ntohl(msg->seq);
    // update input buffer, returns the number of bytes ACKed
    uint32_t rxlen = _inBuffer.putPacket(seq, (const uint8_t *)msg->payload.data, len-5);
    // One may also send an ACK if the received sequence number is outside the reception window.
    // Here, however, I only send ACKs if some data has been received, that was expeced
    if (rxlen) {
      Message resp(Message::ACK);
      // Set sequence
      resp.seq = htonl(_inBuffer.nextSequence());
      // Set window size
      resp.payload.window = htons(_inBuffer.window());
      // Send ACK & reset keep-alive timer
      if (sendDatagram((const uint8_t*) &resp, 7)) {
        logDebug() << "Send ACK seq=" << _inBuffer.nextSequence() << ", win=" <<_inBuffer.window();
        _keepalive.start();
      } else {
        logError() << "Failed to send ACK seq=" << _inBuffer.nextSequence() << ", win=" << _inBuffer.window();
      }
      // Signal new data got available if stream is open
      if (OPEN == _state) {
        emit readyRead();
      }
    }
    // done
    return;
  }

  if (Message::ACK == msg->type) {
    // check message size
    if (len!=7) {
      logInfo() << "SecureStream: Malformed ACK received.";
      return;
    }
    // Get sequence number
    uint32_t seq = ntohl(msg->seq);
    // ACK data in output buffer
    if (uint32_t send = _outBuffer.ack(seq, ntohs(msg->payload.window))) {
      // If some data in the output buffer has been ACKed
      // -> Signal data send if stream is open
      if (OPEN == _state) {
        emit bytesWritten(send);
      }
      // If the last byte in the output buffer was ACKed and the _packettimer is
      // runnning -> stop it.
      if ((0 == _outBuffer.bytesToWrite()) && _packetTimer.isActive()) {
        _packetTimer.stop();
      }
      if ((FIN_RECEIVED == _state) && (0 == bytesToWrite())) {
        // close connection
        cancel();
      }
      // done.
      return;
    }
    // If nothing has been ACKed and ACK seq == first byte of output buffer
    // -> resend requested packet.
    if (_outBuffer.firstSequence() == ntohl(msg->seq)) {
      // -> resent requested message
      Message resp(Message::DATA);
      uint16_t len=DHT_STREAM_MAX_DATA_SIZE; uint32_t seq=0;
      _outBuffer.resend(resp.payload.data, len, seq);
      resp.seq = htonl(seq);
      if (sendDatagram((const uint8_t *) &resp, len+5)) {
        _keepalive.start();
      }
    }
    // done.
    return;
  }

  if (Message::RESET == msg->type) {
    if (len!=1) { return; }
    emit readChannelFinished();
    cancel();
    // done.
    return;
  }

  // Unknown packet type
  logError() << "Unknown datagram received: type=" << msg->type << ".";
}
