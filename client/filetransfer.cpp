#include "filetransfer.h"
#include "application.h"
#include <QtEndian>
#include <QFileInfo>

#define FILETRANSFER_MAX_FILENAME_LEN (DHT_SEC_MAX_DATA_SIZE-9UL)

struct __attribute__((packed)) FileTransferMessage {
  uint8_t type;
  union __attribute__((packed)) {
    struct __attribute__((packed)) {
      uint64_t fileSize;
      uint8_t  filename[FILETRANSFER_MAX_FILENAME_LEN];
    } request;
    struct __attribute__((packed)){
      uint32_t seq;
      uint8_t  data[FILETRANSFER_MAX_DATA_LEN];
    } data;
    struct __attribute__((packed)){
      uint32_t seq;
    } ack;
  } payload;

  FileTransferMessage() {
    memset(this, 0, sizeof(FileTransferMessage));
  }
};

typedef enum {
  REQUEST=0, DATA, ACK, RESET
} FileTransferMessageType;


/* ********************************************************************************************* *
 * Implementation of FileUpload
 * ********************************************************************************************* */
FileUpload::FileUpload(Application &app, const QString &filename, size_t fileSize, QObject *parent)
  : QObject(parent), SecureSocket(app.dht()), _application(app), _state(INITIALIZED),
    _packetBuffer(1<<16, 2000), _fileName(filename), _fileSize(fileSize)
{
  // pass...
}

FileUpload::~FileUpload() {
  if (TERMINATED != _state) {
    stop();
  }
}

void
FileUpload::handleDatagram(const uint8_t *data, size_t len) {
  // ignore null & empty messages
  if ((0 == data) || (0 == len)) { return; }
  // Unpack message
  FileTransferMessage *msg = (FileTransferMessage *) data;

  /*
   *  Dispatch by state and message type
   */

  // If we receive a "reset" message -> close upload
  if (RESET == msg->type) {
    if (TERMINATED != _state) {
      _state = TERMINATED;
      _application.dht().streamClosed(_streamId);
      emit closed();
    }
    return;
  }

  // We do not expect any messages in the "initialized" or "terminated" state:
  if ((INITIALIZED == _state) || (TERMINATED == _state)) { return; }

  // If we have send a request -> handle ACKs
  if ((REQUEST_SEND == _state) && (ACK == msg->type)) {
    _state = STARTED;
    emit accepted();
    return;
  }

  // Handle ACKs during tranmission
  if ((STARTED == _state) && (ACK == msg->type)) {
    // Require at least 4 bytes + type byte
    if (len<5) { return; }
    // Get sequence number
    uint32_t seq = qFromBigEndian(msg->payload.ack.seq);
    // ack some packets.
    size_t send = 0;
    if (0 != (send = _packetBuffer.ack(seq))) {
      emit bytesWritten(send);
    }
  }
}

const QString &
FileUpload::fileName() const {
  return _fileName;
}

size_t
FileUpload::fileSize() const {
  return _fileSize;
}

FileUpload::State
FileUpload::state() const {
  return _state;
}

size_t
FileUpload::free() const {
  return _packetBuffer.free();
}

bool
FileUpload::sendRequest() {
  QFileInfo fileinfo(_fileName);
  QByteArray fname = fileinfo.baseName().toUtf8();
  // truncate filename at maximum length
  size_t fnameLen = std::min(
        size_t(fname.size()), FILETRANSFER_MAX_FILENAME_LEN);

  // Assemble "upload file request"
  FileTransferMessage msg;
  msg.type = REQUEST;
  msg.payload.request.fileSize = qToBigEndian(quint64(_fileSize));
  memcpy(msg.payload.request.filename, fname.constData(), fnameLen);
  // send it
  if (sendDatagram((const uint8_t *) &msg, 9+fnameLen)) {
    _state = REQUEST_SEND;
    return true;
  }

  return false;
}

void
FileUpload::stop() {
  if (_socket) {
    // (re-) send RESET message
    FileTransferMessage msg;
    msg.type = RESET;
    sendDatagram((const uint8_t *) &msg, 1);
  }
  // Emit "closed" only once.
  if (TERMINATED != _state) {
    _state = TERMINATED;
    emit closed();
    // signal DHT to close stream
    _application.dht().streamClosed(_streamId);
  }
}

size_t
FileUpload::write(const QByteArray &data) {
  return write((const uint8_t *)data.constData(), data.size());
}

size_t
FileUpload::write(const uint8_t *buffer, size_t size) {
  if (0 == (size = std::min(size, FILETRANSFER_MAX_DATA_LEN))) {
    logDebug() << "Skip empty data package, size" << size
               << "max size" << FILETRANSFER_MAX_DATA_LEN;
    return 0;
  }
  // get current sequence number
  uint32_t sequence = _packetBuffer.sequence();
  // put into packet buffer
  size = _packetBuffer.write(buffer, size);
  // Assemble message
  FileTransferMessage msg;
  msg.type = DATA;
  msg.payload.data.seq = qToBigEndian(quint32(sequence));
  memcpy(msg.payload.data.data, buffer, size);
  logDebug() << "Send" << size << "bytes data.";
  sendDatagram((uint8_t *)&msg, size+5);
  return size;
}


/* ********************************************************************************************* *
 * Implementation of FileDownload
 * ********************************************************************************************* */
FileDownload::FileDownload(Application &app, QObject *parent)
  : QObject(parent), SecureSocket(app.dht()), _application(app),
    _state(INITIALIZED), _fileSize(0), _packetBuffer(1<<16)
{

}

FileDownload::~FileDownload() {
  if (TERMINATED != _state) { stop(); }
}

FileDownload::State
FileDownload::state() const {
  return _state;
}

size_t
FileDownload::fileSize() const {
  return _fileSize;
}

size_t
FileDownload::available() const {
  return _packetBuffer.available();
}

void
FileDownload::handleDatagram(const uint8_t *data, size_t len) {
  // ignore null & empty messages
  if ((0 == data) || (0 == len)) { return; }

  // Unpack message
  FileTransferMessage *msg = (FileTransferMessage *) data;

  /*
   *  Dispatch by state and message type
   */

  // If we receive a "reset" message -> close upload
  if (RESET == msg->type) {
    if (TERMINATED != _state) {
      _state = TERMINATED;
      _application.dht().streamClosed(_streamId);
      emit closed();
    }
    return;
  }

  // We do not expect any messages in the "terminated" state:
  if ((TERMINATED == _state)) { return; }

  // If initialized, expect a REQUEST message
  if ((INITIALIZED == _state) && (REQUEST == msg->type)) {
    // check length
    if (len<9) { return; }
    // verify filename length
    _fileSize = qFromBigEndian(quint64(msg->payload.request.fileSize));
    QString filename = QString::fromUtf8((char *)msg->payload.request.filename, len-9);
    _state = REQUEST_RECEIVED;
    emit request(filename, _fileSize);
    return;
  }

  // If request was already accepted -> resend ACK.
  if ((ACCEPTED == _state) && (REQUEST == msg->type)) {
    // check length
    if (len<9) { return; }
    // resend ACK
    FileTransferMessage resp;
    resp.type = ACK; resp.payload.ack.seq = 0;
    sendDatagram((uint8_t *) &resp, 5);
    return;
  }

  // Just accepted request and received data -> switch to STARTED stage.
  if ((ACCEPTED == _state) && (DATA == msg->type)) {
    logDebug() << "Received DATA in ACCEPTED state -> switch into STARTED state.";
    _state = STARTED;
    // pass through;
  }

  // If started, expect data
  if ((STARTED == _state) && (DATA == msg->type)) {
    logDebug() << "Received DATA package in STARTED stage.";
    // check length
    if (len<5) { return; }
    uint32_t seq = qFromBigEndian(msg->payload.data.seq);
    logDebug() << "Received" << (len-5) << "bytes data with seq" << seq;
    if (_packetBuffer.putPacket(seq, msg->payload.data.data, len-5)) {
      // Send ACK for returned seq number
      FileTransferMessage resp;
      resp.type = ACK; resp.payload.ack.seq = qToBigEndian(quint32(seq));
      sendDatagram((uint8_t *) &resp, 5);
      logDebug() << "Send ACK for seq" << seq;
      emit readyRead();
    }
    return;
  }
}

size_t
FileDownload::read(QByteArray &buffer) {
  return read((uint8_t *)buffer.data(), buffer.size());
}

size_t
FileDownload::read(uint8_t *buffer, size_t len) {
  return _packetBuffer.read(buffer, len);
}

void
FileDownload::accept() {
  if (REQUEST_RECEIVED == _state) {
    logDebug() << "Send ACK to accept the file-transfer request.";
    _state = ACCEPTED;
    FileTransferMessage resp;
    resp.type = ACK; resp.payload.ack.seq = 0;
    sendDatagram((uint8_t *) &resp, 5);
  }
}

void
FileDownload::stop() {
  if (_socket) {
    FileTransferMessage msg;
    msg.type = RESET;
    sendDatagram((uint8_t *) &msg, 1);
  }
  // Emit "closed" only once.
  if (TERMINATED != _state) {
    _state = TERMINATED;
    // signal DHT to close stream
    _application.dht().streamClosed(_streamId);
    emit closed();
  }
}
