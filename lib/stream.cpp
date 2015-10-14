#include "stream.h"

#include "dht.h"
#include <netinet/in.h>


/** The format of the stream messages. */
struct __attribute__((packed)) Message {
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
  union {
    /** The number of bytes the receiver is willing to accept. */
    uint32_t window;
    /** Payload. */
    uint8_t  data[DHT_SEC_MAX_DATA_SIZE-5];
  } payload;

  /** Constructor. */
  inline Message(Flags type) {
    memset(this, 0, sizeof(Message)); this->type = type;
  }
};


/* ******************************************************************************************** *
 * Implementation of SecureStream
 * ******************************************************************************************** */
SecureStream::SecureStream(DHT &dht, QObject *parent)
  : QIODevice(parent), SecureSocket(dht), _inBuffer(16<<16), _outBuffer(16<<16, 2000), _closed(false),
    _keepalive(), _packetTimer(), _timeout()
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
  // send "keep-alive" ping
  sendNull();
}

void
SecureStream::_onCheckPacketTimeout() {
  // Resent messages
  Message msg(Message::DATA); size_t len; uint32_t seq=0;
  if (_outBuffer.resend(msg.payload.data, len, seq)) {
    /* logDebug() << "SecureStream: Resend packet SEQ=" << seq
               << " (" << len <<"b)."; */
    msg.seq = htonl(seq);
    sendDatagram((const uint8_t *) &msg, len+5);
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
  // Make sure the stream does not get notified anymore.
  _dht.socketClosed(id());
  // close QIODevice
  QIODevice::close();
  // Stop keep alive timer
  _keepalive.stop();
  // Stop packet timer.
  _packetTimer.stop();
  // Stop timeout timer
  _timeout.stop();
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
SecureStream::outBufferFree() const {
  return _outBuffer.free();
}

qint64
SecureStream::bytesToWrite() const {
  // IO device buffer + internal packet-buffer
  return _outBuffer.available() + QIODevice::bytesToWrite();
}

size_t
SecureStream::inBufferFree() const {
  return _inBuffer.free();
}

qint64
SecureStream::writeData(const char *data, qint64 len) {
  // Determine maximum data length
  len = std::min(len, qint64(DHT_SEC_MAX_DATA_SIZE-5));
  // Pack message
  Message msg(Message::DATA);
  msg.seq = htonl(_outBuffer.sequence());
  // put in output buffer
  len = _outBuffer.write((const uint8_t *)data, len);
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
  if (0 == len) { return; }
  // Unpack message
  const Message *msg = (const Message *)data;
  /*
   * dispatch by type
   */
  if (Message::DATA == msg->type) {
    if (len<5) { return; }
    uint32_t seq = ntohl(msg->seq);
    bool ack = false;
    if (_inBuffer.putPacket(seq, (const uint8_t *)msg->payload.data, len-5, ack)) {
      // send ACK
      if (ack) {
        //logDebug() << "SecureSocket: Send ACK=" << ack_seq;
        Message resp(Message::ACK);
        resp.seq = htonl(seq);
        if (! sendDatagram((const uint8_t*) &resp, 5)) {
          logWarning() << "SecureStream: Failed to send ACK.";
        }
      }
      // Signal data available
      emit readyRead();
    }
  } else if (Message::ACK == msg->type) {
    if (len!=5) { return; }
    size_t send = _outBuffer.ack(ntohl(msg->seq));
    if (0 != send) {
      emit bytesWritten(send);
    }
  } else if (Message::RESET == msg->type) {
    if (len!=1) { return; }
    _closed = true;
    emit readChannelFinished();
    close();
  } else {
    logError() << "Unknown datagram received: type=" << msg->type << ".";
  }
}
