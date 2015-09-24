#include "natpmp.h"

typedef enum {
  SUCCESS = 0,
  UNSP_VERSION,
  DISABLED,
  NET_FAILIURE,
  OUT_OF_RESOUCES,
  UNSP_OPCODE
} PMPResultCode;


struct __attribute__((packed)) PMPMapRequest {
  uint8_t version;   // must be 0
  uint8_t opcode;    // =1 map UDP; =2 map TCP
  uint16_t reserved; // must be 0
  uint16_t iport;    // local port
  uint16_t eport;    // external port
  uint32_t lifetime; // lifetime in seconds
};

struct __attribute__((packed)) PMPMapResponse {
  uint8_t  version;   // must be 0
  uint8_t  opcode;    // =129 UDP, =130 TCP
  uint16_t result;    // result code
  uint32_t epoch;     // Seconds since table was initialized
  uint16_t iport;     // local port
  uint16_t eport;     // external port
  uint32_t lifetime;  // lifetime in seconds
};


NATPMPClient::NATPMPClient(QObject *parent) :
  QObject(parent), _socket()
{
  _socket.bind(QHostAddress::Any);
  QObject::connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onDatagramReceived()));
}

void
NATPMPClient::requestMap(uint16_t iport, const QHostAddress &addr, uint16_t port)
{
  PMPMapRequest request;
  memset(&request, 0, sizeof(PMPMapRequest));

  // Assemble request
  request.version  = 0;
  request.opcode   = 1;
  request.iport    = htons(iport);
  request.eport    = 0;
  request.lifetime = htonl(60*60);

  // Send request
  if (sizeof(PMPMapRequest) > _socket.writeDatagram((char *)&request, sizeof(PMPMapRequest), addr, port)) {
    qDebug() << "Failed to send NAT-PMP Map request.";
  }
}

void
NATPMPClient::_onDatagramReceived() {
  while (_socket.hasPendingDatagrams()) {
    PMPMapResponse response;
    memset(&response, 0, sizeof(PMPMapResponse));

    // Read data
    QHostAddress addr; uint16_t port;
    size_t size = _socket.readDatagram((char *) &response, sizeof(PMPMapResponse), &addr, &port);
    qDebug() << "Received message from" << addr << ":" << port;

    // Check size
    if (size != sizeof(PMPMapResponse)) {
      qDebug() << "Invalid response received (size).";
      return;
    }

    // Check opcode & response code
    if (129 != response.opcode) {
      qDebug() << "Invalid response received (opcode:" << response.opcode << ")";
      return;
    }

    if (response.result) {
      qDebug() << "NAT-PMP returned error:" << ntohs(response.result);
      return;
    }
  }
}
