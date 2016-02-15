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
  : _inptr(0), _outptr(0), _full(false)
{
  // pass...
}


/* ********************************************************************************************* *
 * Implementation of StreamInBuffer
 * ********************************************************************************************* */
StreamInBuffer::StreamInBuffer()
  : _buffer(), _available(0), _nextSequence(0), _packets()
{
  // pass...
}


/* ********************************************************************************************* *
 * Implementation of StreamInBuffer
 * ********************************************************************************************* */
StreamOutBuffer::StreamOutBuffer(uint64_t timeout)
  : _buffer(), _firstSequence(0), _nextSequence(0), _window(0xffff), _timestamp(),
    _rt_sum(0), _rt_sumsq(0), _rt_count(0), _timeout(timeout)
{
  // pass...
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
  qint64 inlen=len;
  // Determine maximum data length as the minimum of
  // maximum length (len), space in output buffer, window-size of the remote,
  // and maximum payload length
  len = std::min(len, qint64(_outBuffer.free()));
  len = std::min(len, qint64(DHT_STREAM_MAX_DATA_SIZE));
  if (0 == len) {
    logDebug() << "Do not send data len=" << inlen << ": window=" << _outBuffer.free() << ".";
    return 0;
  }

  // Assemble message
  Message msg(Message::DATA);
  // store seq number
  msg.seq = htonl(_outBuffer.nextSequence());

  // put in output buffer, updates sequence number
  if(0 == (len = _outBuffer.write((const uint8_t *)data, len))) {
    return 0;
  }

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
    return open(QIODevice::ReadWrite);
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
      if (sendDatagram((const uint8_t*) &resp, 7)) { _keepalive.start(); }
      // Signal new data got available if stream is open
      if (OPEN == _state) {
        emit readyRead();
      }
    } else {
      logDebug() << "Unexpected data: Drop seq=" << seq << ", len=" << (len-5) << ".";
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
