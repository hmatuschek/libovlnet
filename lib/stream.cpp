#include "stream.h"


/** The format of the stream messages. */
struct __attribute__((packed)) Message {
  /** Possible stream message types. */
  typedef enum {
    DATA = 0, ACK, RESET
  } Type;

  /** The message type. */
  uint8_t  type;
  /** The sequential number. */
  uint32_t seq;
  /** Payload. */
  uint8_t  data[DHT_SEC_MAX_DATA_SIZE-5];

  inline Message(Type type) { memset(this, 0, sizeof(Message)); this->type = type; }
};


/* ******************************************************************************************** *
 * Implementation of SecureStream
 * ******************************************************************************************** */
SecureStream::SecureStream(DHT &dht, QObject *parent)
  : QIODevice(parent), SecureSocket(dht), _inBuffer(1<<16), _outBuffer(1<<16, 2000), _closed(false)
{
  // pass...
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
  return (!_closed) && ok;
}

void
SecureStream::close() {
  _closed = true;
  Message msg(Message::RESET);
  sendDatagram((uint8_t *) &msg, 1);
}

qint64
SecureStream::bytesAvailable() const {
  return _inBuffer.available();
}

size_t
SecureStream::outBufferFree() const {
  return _outBuffer.free();
}

qint64
SecureStream::bytesToWrite() const {
  return _outBuffer.available();
}

size_t
SecureStream::inBufferFree() const {
  return _inBuffer.free();
}

qint64
SecureStream::writeData(const char *data, qint64 len) {
  // Determine maximum data length
  len = std::max(len, qint64(DHT_SEC_MAX_DATA_SIZE-5));
  // Pack message
  Message msg(Message::DATA);
  msg.seq = htonl(_outBuffer.sequence());
  // put in output buffer
  len = _outBuffer.write((const uint8_t *)data, len);
  memcpy(msg.data, data, len);
  // send message
  sendDatagram((const uint8_t *)&msg, len+5);
  return len;
}

qint64
SecureStream::readData(char *data, qint64 maxlen) {
  return _inBuffer.read((uint8_t *)data, maxlen);
}

void
SecureStream::handleDatagram(const uint8_t *data, size_t len) {
  // Check size
  if (len<1) { return; }
  // Unpack message
  const Message *msg = (const Message *)data;
  // dispatch by type
  if (Message::DATA == msg->type) {
    if (len<5) { return; }
    uint32_t seq = ntohl(msg->seq);
    if (_inBuffer.putPacket(seq, (const uint8_t *)msg->data, len-5)) {
      // send ACK
      Message resp(Message::ACK);
      resp.seq = htonl(seq);
      sendDatagram((const uint8_t*) &resp, 5);
      // Signal data available
      emit readyRead();
    }
  } else if (Message::ACK == msg->type) {
    if (len!=5) { return; }
    size_t send = _outBuffer.ack(msg->seq);
    if (send) {
      emit bytesWritten(send);
    }
  } else if (Message::RESET == msg->type) {
    if (len!=1) { return; }
    _closed = true;
    emit readChannelFinished();
    _dht.streamClosed(id());
  } else {
    qDebug() << "Unknown datagram received.";
  }
}
