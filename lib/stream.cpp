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


/* ******************************************************************************************** *
 * Implementation of SecureStream
 * ******************************************************************************************** */
SecureStream::SecureStream(DHT &dht, QObject *parent)
  : QIODevice(parent), SecureSocket(dht), _inBuffer(), _outBuffer(2000),
    _closed(false), _keepalive(), _packetTimer(), _timeout()
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
  if ((! _closed) && ok) {
    _keepalive.start();
    _timeout.start();
    _packetTimer.start();
  }
  return (!_closed) && ok;
}

void
SecureStream::_onKeepAlive() {
  // send "keep-alive" ping (ACK _inbuffer.nextSequence)
  Message resp(Message::ACK);
  // Set sequence
  resp.seq = htonl(_inBuffer.nextSequence());
  // Send window size
  resp.payload.window = htons(_inBuffer.window());
  if (! sendDatagram((const uint8_t*) &resp, 7)) {
    logWarning() << "SecureStream: Failed to send ACK.";
  } else {
    logDebug() << "SecureStream: Send ACK, SEQ=" << _inBuffer.nextSequence()
               << ", WIN=" << _inBuffer.window();
  }
}

void
SecureStream::_onCheckPacketTimeout() {
  if (! _outBuffer.timeout()) { return; }
  // Resent messages
  Message msg(Message::DATA);
  uint16_t len=sizeof(msg.payload.data); uint32_t seq=0;
  _outBuffer.resend(msg.payload.data, len, seq);
  logDebug() << "SecureStream: Resend packet SEQ=" << seq << ", LEN=" << len;
  msg.seq = htonl(seq);
  if (!sendDatagram((const uint8_t *) &msg, len+5)) {
    logWarning() << "SecureStream: Cannot resend packet SEQ=" << seq;
  } else {
    _keepalive.start();
  }
}

void
SecureStream::_onTimeOut() {
  logDebug() << "SecureStream: Connection timeout -> close().";
  // close connection
  close();
}

void
SecureStream::close() {
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
  if (! _closed) {
    logDebug() << "SecureStream: Close connection, send RST.";
    _closed = true;
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

size_t
SecureStream::canSend() const {
  logDebug() << "SecureStream: Can send (buffer=" << _outBuffer.free() << ")";
  return std::min(_outBuffer.free(),
                  uint32_t(DHT_STREAM_MAX_DATA_SIZE));
}

qint64
SecureStream::bytesToWrite() const {
  // IO device buffer + internal packet-buffer
  return _outBuffer.bytesToWrite() + QIODevice::bytesToWrite();
}

qint64
SecureStream::writeData(const char *data, qint64 len) {
  // Determine maximum data length as the minimum of
  // maximum length (len), space in output buffer, window-size of the remote,
  // and maximum payload length
  len = std::min(len,
                 std::min(qint64(_outBuffer.free()),
                          qint64(DHT_STREAM_MAX_DATA_SIZE)));
  // Pack message
  Message msg(Message::DATA);
  msg.seq = htonl(_outBuffer.nextSequence());
  // put in output buffer
  len = _outBuffer.write((const uint8_t *)data, len);
  logDebug() << "SecureStream: Send DATA, SEQ=" << ntohl(msg.seq)
             << ", LEN=" << len;
  if (0 >= len) { return len; }
  // store in message
  memcpy(msg.payload.data, data, len);
  // send message
  if( sendDatagram((const uint8_t *)&msg, len+5) ) {
    // reset keep-alive timer
    _keepalive.start();
    return len;
  }
  return -1;
}

qint64
SecureStream::readData(char *data, qint64 maxlen) {
  return _inBuffer.read((uint8_t *)data, maxlen);
}

void
SecureStream::handleDatagram(const uint8_t *data, size_t len) {
  // Restart time-out timer
  _timeout.start();

  // Handle null-packets
  if (0 == len) {
    logDebug() << "Received null.";
    return;
  }

  if (len > sizeof(Message)) {
    logDebug() << "Received invalid packet, LEN=" << len;
    return;
  }

  // Unpack message
  const Message *msg = (const Message *)data;

  /*
   * dispatch by type
   */
  if (Message::DATA == msg->type) {
    if (len<5) {
      logWarning() << "Received invalid DATA packet LEN=" << len << ".";
      return;
    }
    // Get sequence number of data packet
    uint32_t seq = ntohl(msg->seq);
    uint32_t rxlen = _inBuffer.putPacket(seq, (const uint8_t *)msg->payload.data, len-5);
    logDebug() << "SecureStream: Got data SEQ=" << seq << ", LEN=" << (len-5)
               << ", RX=" << rxlen;
    if (rxlen) {
      Message resp(Message::ACK);
      // Set sequence
      resp.seq = htonl(_inBuffer.nextSequence());
      // Send window size
      resp.payload.window = htons(_inBuffer.window());
      if (! sendDatagram((const uint8_t*) &resp, 7)) {
        logWarning() << "SecureStream: Failed to send ACK.";
      } else {
        logDebug() << "SecureStream: Send ACK, SEQ=" << _inBuffer.nextSequence()
                   << ", WIN=" << _inBuffer.window();
        _keepalive.start();
      }
      // Signal new data available (if any)
      emit readyRead();
    }
  } else if (Message::ACK == msg->type) {
    if (len!=7) {
      logWarning() << "Received invalid ACK packet LEN=" << len << ".";
      return;
    }
    uint32_t seq = ntohl(msg->seq);
    logDebug() << "SecureStream: Got ACK SEQ=" << seq
               << ", WIN=" << ntohs(msg->payload.window);
    uint32_t send = _outBuffer.ack(seq, ntohs(msg->payload.window));
    logDebug() << "SecureStream: ACKed " << send << "b.";
    if (send) {
      // Signal data send
      emit bytesWritten(send);
    } else {
      // If nothing has been ACKed and ACK seq == first byte of output buffer
      // -> resend requested packet.
      if (_outBuffer.firstSequence() == ntohl(msg->seq)) {
        // -> resent requested message
        Message resp(Message::DATA);
        uint16_t len=DHT_STREAM_MAX_DATA_SIZE; uint32_t seq=0;
        _outBuffer.resend(resp.payload.data, len, seq);
        logDebug() << "SecureStream: Resend requested packet SEQ=" << seq << ", LEN=" << len;
        resp.seq = htonl(seq);
        if (!sendDatagram((const uint8_t *) &resp, len+5)) {
          logWarning() << "SecureStream: Cannot resend packet SEQ=" << seq;
        } else {
          _keepalive.start();
        }
      }
    }
  } else if (Message::RESET == msg->type) {
    if (len!=1) { return; }
    logDebug() << "SecureStream: Got RST.";
    _closed = true;
    emit readChannelFinished();
    close();
  } else {
    logError() << "Unknown datagram received: type=" << msg->type << ".";
  }
}


/* ********************************************************************************************* *
 * Implementation of FixedBuffer
 * ********************************************************************************************* */
FixedBuffer::FixedBuffer()
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

