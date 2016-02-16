#include "pcp.h"
#include "string.h"
#include "logger.h"
#include <netinet/in.h>


typedef enum {
  SUCCESS = 0,
  UNSUPP_VERSION,
  NOT_AUTHORIZED,
  MALFORMED_REQUEST,
  UNSUPP_OPCODE,
  UNSUPP_OPTION,
  MALFORMED_OPTION,
  NETWORK_FAILURE,
  NO_RESOURCES,
  UNSUPP_PROTOCOL,
  USER_EX_QUOTA,
  CANNOT_PROVIDE_EXTERNAL,
  ADDRESS_MISMATCH,
  EXCESSIVE_REMOTE_PEERS
} PCPResultCodes;

/** Structure of the PCP MAP request and response opcode. */
struct __attribute__((packed)) PCPMap {
  uint8_t  nonce[12]; // Random number
  uint8_t  protocol;  // 6=TCP, 17=UDP, 0=ANY
  uint8_t  dummy[3];  // must be 0
  uint16_t iport;     // internal port
  uint16_t eport;     // external port // 0 == ANY
  uint8_t  eipv6[16]; // external address
};

struct __attribute__((packed)) PCPRequest {
  uint8_t version;      // == 2
  uint8_t response : 1; // request==0
  uint8_t opcode   : 7;
  uint16_t dummy;       // must be 0
  uint32_t lifetime;
  uint8_t  ipv6[16];
  PCPMap   mapRequest;
};

struct __attribute__((packed)) PCPResponse {
  uint8_t  version;       // == 2
  uint8_t  response : 1;  // response==1
  uint8_t  opcode   : 7;
  uint8_t  dummy1;        // must be 0
  uint8_t  result;        // result code
  uint32_t lifetime;
  uint32_t epoch;
  uint8_t  clientip[12];  // last 96 bits of client ip
  PCPMap   mapResponse;
};


PCPClient::PCPClient(QObject *parent)
  : QObject(parent), _socket()
{
  _socket.bind(QHostAddress::Any, 5351);
  for (size_t i=0; i<12; i++) {
    _nonce[i] = qrand() & 0xff;
  }

  QObject::connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onDatagramReceived()));
}

void
PCPClient::requestMap(uint16_t iport, const QHostAddress &addr, uint16_t port)
{
  // Figure out the client IP address used to reach the PCP service at the specified address.
  _socket.connectToHost(addr, port);
  if (! _socket.waitForConnected(1000)) {
    logError() << "Failed to connect PCP server " << addr << ":" << port;
    return;
  }
  QHostAddress local = _socket.localAddress();
  logInfo() << "Got local address" << local;

  // Assemble request
  PCPRequest request;
  memset(&request, 0, sizeof(PCPRequest));
  request.version  = 2;
  request.response = 0;
  request.opcode   = 1;
  request.lifetime = htonl(60*60); // request 1h
  memcpy(request.ipv6, local.toIPv6Address().c, 16);
  memcpy(request.mapRequest.nonce, _nonce, 12);
  request.mapRequest.protocol = 17;
  request.mapRequest.iport = htons(iport);
  // eport & eipv6 are left 0 -> means any mapping possible

  logDebug() << "Send MAP request.";
  if (int(sizeof(PCPRequest)) > _socket.writeDatagram((char *)&request, addr, port)) {
    logError() << "Can not send PCP request to" << addr << ":" << port;
  }
  _socket.disconnectFromHost();
}

void
PCPClient::_onDatagramReceived() {
  while (_socket.hasPendingDatagrams()) {
    PCPResponse response;
    memset(&response, 0, sizeof(PCPResponse));
    QHostAddress addr; uint16_t port;
    size_t size = _socket.readDatagram((char *)&response, sizeof(PCPResponse), &addr, &port);
    logDebug() << "Response received from" << addr << ":" << port;
    if (size!=sizeof(PCPResponse)) {
      logError() << "Invalid response received from" << addr << ":" << port;
      return;
    }
    if (memcmp(response.mapResponse.nonce, _nonce, 12)) {
      logError() << "Invalid response nonce received from" << addr << ":" << port;
      return;
    }
    if (SUCCESS == response.result) {
      emit mapping(ntohs(response.mapResponse.iport),
                   QHostAddress(response.mapResponse.eipv6),
                   ntohs(response.mapResponse.eport));
    }
  }
}


