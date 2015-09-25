#include "natpmp.h"

/** Possible result codes. */
typedef enum {
  SUCCESS = 0,     ///< Success.
  UNSP_VERSION,    ///< Unsupported version.
  DISABLED,        ///< Port forwarding disabled.
  NET_FAILIURE,    ///< Network failiure.
  OUT_OF_RESOUCES, ///< Out of resources.
  UNSP_OPCODE      ///< Unsupported opcode.
} PMPResultCode;


/** A PMP map request. */
struct __attribute__((packed)) PMPMapRequest {
  /** Specifies the protocol version, must be 0. */
  uint8_t version;
  /** Specifies the opcode, =1 means MAP UDP and
   * =2 MAP TCP. */
  uint8_t opcode;
  /** Reserved, need to be zero on transmission. */
  uint16_t reserved;
  /** The local port to map. */
  uint16_t iport;
  /** The suggested external port, =0 means any. */
  uint16_t eport;
  /** Suggested lifetime of the mapping. */
  uint32_t lifetime;
  /** Empty constructor. */
  PMPMapRequest();
};

PMPMapRequest::PMPMapRequest() {
  memset(this, 0, sizeof(PMPMapRequest));
}


/** A PMP map response. */
struct __attribute__((packed)) PMPMapResponse {
  /** Specifies the protocol verion, must be 0. */
  uint8_t  version;
  /** Response opcode, (128+REQUEST_OPCODE, i.e. =129 UDP, and =130 TCP). */
  uint8_t  opcode;
  /** The result code, see @c PMPResultCode for details. */
  uint16_t result;
  /** The epoch of the PMP service (e.g. uptime in seconds). */
  uint32_t epoch;
  /** Mapped internal port. */
  uint16_t iport;
  /** Assigned external port. */
  uint16_t eport;
  /** Assigned lifetime of the mapping. */
  uint32_t lifetime;
  /** Empty constructor. */
  PMPMapResponse();
};

PMPMapResponse::PMPMapResponse() {
  memset(this, 0, sizeof(PMPMapResponse));
}


/* ********************************************************************************************* *
 * Implementation of PMPClient::MapItem
 * ********************************************************************************************* */
PMPClient::MapItem::MapItem()
  : _iport(0), _eport(0), _lifetime(0), _addr(), _port(0)
{
  // pass..
}

PMPClient::MapItem::MapItem(uint16_t iport, uint16_t eport, uint32_t lifetime, const QHostAddress &addr, uint16_t port)
  : _iport(iport), _eport(eport), _lifetime(lifetime), _addr(addr), _port(port),
    _timestamp(QDateTime::currentDateTime())
{
  // pass...
}

PMPClient::MapItem::MapItem(const MapItem &other)
  : _iport(other._iport), _eport(other._eport), _lifetime(other._lifetime),
    _addr(other._addr), _port(other._port), _timestamp(other._timestamp)
{
  // Pass...
}

PMPClient::MapItem &
PMPClient::MapItem::operator =(const PMPClient::MapItem &other) {
  _iport = other._iport;
  _eport = other._eport;
  _lifetime = other._lifetime;
  _addr = other._addr;
  _port = other._port;
  _timestamp = other._timestamp;
  return *this;
}


/* ********************************************************************************************* *
 * Implementation of PMPClient
 * ********************************************************************************************* */
PMPClient::PMPClient(QObject *parent) :
  QObject(parent), _socket(), _reqTimestamp(), _reqTimer(), _mapTimer()
{
  // Bind UDP socket to any address and some port.
  _socket.bind(QHostAddress::Any);

  // Check every 500ms
  _reqTimer.setInterval(500);
  _reqTimer.setSingleShot(false);

  // Check every 5min
  _mapTimer.setInterval(1000*60*5);
  _mapTimer.setSingleShot(false);

  // Get notified on new datagrams
  QObject::connect(&_socket, SIGNAL(readyRead()), this, SLOT(_onDatagramReceived()));
  QObject::connect(&_reqTimer, SIGNAL(timeout()), this, SLOT(_onReqTimer()));
  QObject::connect(&_mapTimer, SIGNAL(timeout()), this, SLOT(_onMapTimer()));
}

void
PMPClient::requestMap(uint16_t iport, const QHostAddress &addr, uint16_t port)
{
  // Send request
  if (! _sendMapRequest(iport, 0, addr, port)) {
    qDebug() << "Failed to send NAT-PMP Map request.";
    emit failed(iport); return;
  }

  // Register pending request
  _iport = iport; _addr  = addr; _port = port;
  _reqTimestamp = QDateTime::currentDateTime();

  // Start request timer
  if (!_reqTimer.isActive()) { _reqTimer.start(); }
}

void
PMPClient::_onDatagramReceived() {
  while (_socket.hasPendingDatagrams()) {
    PMPMapResponse response;

    // Read data
    QHostAddress addr; uint16_t port;
    size_t size = _socket.readDatagram((char *) &response, sizeof(PMPMapResponse), &addr, &port);
    qDebug() << "Received message from" << addr << ":" << port;

    // Check if we have a pending request:
    if (! _reqTimestamp.isValid()) {
      qDebug() << "Unexpected message from" << addr << ":" << port; continue;
    }

    // Check size
    if (size != sizeof(PMPMapResponse)) {
      qDebug() << "Invalid response received (size)."; continue;
    }

    // Check opcode & response code
    if (129 != response.opcode) {
      qDebug() << "Invalid response received (opcode:" << response.opcode << ")"; continue;
    }

    // Check iport
    if (_iport != ntohs(response.iport)) {
      qDebug() << "Unexpected internal port" << ntohs(response.iport); continue;
    }

    // Check result code
    if (response.result) {
      qDebug() << "NAT-PMP returned error:" << ntohs(response.result);
      _reqTimer.stop(); _reqTimestamp = QDateTime(); continue;
      // If response was not a refresh -> signal failiure
      if (! _mappings.contains(_iport)) { emit failed(_iport); }
    }

    // Stop timer & invalidate current request
    _reqTimer.stop();
    _reqTimestamp = QDateTime();

    // Gather result & create mapping
    _mappings[_iport] = MapItem(ntohs(response.iport), ntohs(response.eport),
                                ntohl(response.lifetime), addr, port);

    // Start mapTimer in not running
    if (! _mapTimer.isActive()) { _mapTimer.start(); }

    // If response was not a refresh -> signal success
    if (! _mappings.contains(_iport)) { emit success(_iport, ntohs(response.eport)); }
  }
}

void
PMPClient::_onReqTimer() {
  // If there is no pending request -> stop timer
  if (! _reqTimestamp.isValid()) { _reqTimer.stop(); return; }
  // Check for timeout
  if (_reqTimestamp.addMSecs(2000)<QDateTime::currentDateTime()) {
    // timeout
    _reqTimer.stop(); _reqTimestamp = QDateTime();
    // If request was not a refresh -> signal error
    if (! _mappings.contains(_iport)) { emit failed(_iport); }
  }
}

void
PMPClient::_onMapTimer() {
  // Check for required renewals of the mappings
  QHash<uint16_t, MapItem>::iterator item = _mappings.begin();
  while (item != _mappings.end()) {
    if (item->needsRefresh() & (! item->expired())) {
      // If item needs refresh but is not expired yet -> refresh
      _sendMapRequest(item->iport(), item->eport(), item->addr(), item->port());
      // Continue with next item
      item++;
    } else if (item->expired()) {
      // Remove expired mappings
      item = _mappings.erase(item);
    } else {
      // Continue with next item
      item++;
    }
  }
  // If mapping table is empty -> stop timer.
  if (0 == _mappings.size()) { _mapTimer.stop(); }
}

bool
PMPClient::_sendMapRequest(uint16_t iport, uint16_t eport, const QHostAddress &addr, uint16_t port) {
  PMPMapRequest request;
  // Assemble request
  request.version  = 0;
  request.opcode   = 1;
  request.iport    = htons(iport);
  request.eport    = htons(eport);
  request.lifetime = htonl(60*60);
  // send it
  // Send request
  return sizeof(PMPMapRequest) ==
      _socket.writeDatagram((char *)&request, sizeof(PMPMapRequest), addr, port);
}
