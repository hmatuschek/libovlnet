#include "upnp.hh"
#include "logger.hh"
#include <QUrl>
#include <QNetworkReply>
#include <QBuffer>
#include <QDomDocument>
#include <QDomElement>


/* ******************************************************************************************** *
 * Implementation of UPNP
 * ******************************************************************************************** */
UPNP::UPNP(uint16_t iport, uint16_t eport, QObject *parent)
  : QObject(parent), _state(IDLE), _timeout(), _socket(), _network(), _iport(iport), _eport(eport)
{
  _localAddress = getLocalAddress();
  _timeout.setInterval(1000);
  _timeout.setSingleShot(true);
  connect(&_timeout, SIGNAL(timeout()), this, SLOT(onTimeout()));
}


bool
UPNP::discover() {
  if (IDLE != _state) { return false; }
  _state = DISCOVERY;

  if(! _socket.bind(QHostAddress::Any, 1900)) {
    logDebug() << "Cannot bind to UDP socket.";
    _state = IDLE; return false;
  }

  // connect signals
  connect(&_socket, SIGNAL(readyRead()), this, SLOT(processDiscover()));

  // Assenble request
  QByteArray datagram("M-SEARCH * HTTP/1.1\r\n"
                      "Host:239.255.255.250:1900\r\n"
                      "ST:urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
                      "Man:\"ssdp:discover\"\r\n"
                      "MX:3\r\n\r\n");
  if (datagram.size() != _socket.writeDatagram(datagram, QHostAddress("239.255.255.250"), 1900)) {
    logError() << "Cannot send UPnP request.";
    _state = IDLE;
    return false;
  }

  // Start timeout
  _timeout.start();

  return true;
}

bool
UPNP::getDescription(const QUrl &url) {
  if (IDLE != _state) { return false; }
  logDebug() << "Request device description from: " << url.toString();

  _state = GET_DESCRIPION;
  connect(&_network, SIGNAL(finished(QNetworkReply*)), this, SLOT(processDescription(QNetworkReply*)));
  _network.get(QNetworkRequest(url));

  return true;
}

bool
UPNP::addPortMapping(UPNPDeviceDescription *device, uint16_t iport, uint16_t eport) {
  if (IDLE != _state) { return false; }
  logDebug() << "Request port mapping for internal port " << iport;

  UPNPServiceDescription *service = device->findServiceByType(
        "urn:schemas-upnp-org:service:WANIPConnection:1");
  if (! service) {
    logError() << "No service with type 'urn:schemas-upnp-org:service:WANIPConnection:1' found.";
    return false;
  }

  QList< QPair<QString, QString> >args;
  args.push_back(QPair<QString,QString>("NewRemoteHost", ""));
  args.push_back(QPair<QString,QString>("NewExternalPort", QString::number(eport)));
  args.push_back(QPair<QString,QString>("NewProtocol", "UDP"));
  args.push_back(QPair<QString,QString>("NewInternalPort", QString::number(iport)));
  args.push_back(QPair<QString,QString>("NewInternalClient", _localAddress.toString()));
  args.push_back(QPair<QString,QString>("NewEnabled", "1"));
  args.push_back(QPair<QString,QString>("NewPortMappingDescription", "OVLNet"));
  args.push_back(QPair<QString,QString>("NewLeaseDuration", "0"));

  _state = ADD_PORTMAPPING;
  connect(&_network, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(processAddPortMapping(QNetworkReply*)));

  return sendCommand(service->controlURL(), "urn:schemas-upnp-org:service:WANIPConnection:1",
                     "AddPortMapping", args);
}

bool
UPNP::getPortMapping(UPNPDeviceDescription *device, uint16_t eport) {
  if (IDLE != _state) { return false; }
  logDebug() << "Request port mapping entry for port " << eport;

  UPNPServiceDescription *service = device->findServiceByType(
        "urn:schemas-upnp-org:service:WANIPConnection:1");
  if (! service) {
    logError() << "No service with type 'urn:schemas-upnp-org:service:WANIPConnection:1' found.";
    return false;
  }

  QList< QPair<QString, QString> >args;
  args.push_back(QPair<QString,QString>("NewRemoteHost", ""));
  args.push_back(QPair<QString,QString>("NewExternalPort", QString::number(eport)));
  args.push_back(QPair<QString,QString>("NewProtocol", "UDP"));

  _state = GET_PORTMAPPING;
  connect(&_network, SIGNAL(finished(QNetworkReply*)),
          this, SLOT(processGetPortMapping(QNetworkReply*)));

  return sendCommand(service->controlURL(), "urn:schemas-upnp-org:service:WANIPConnection:1",
                     "GetSpecificPortMappingEntry", args);
}

QHostAddress
UPNP::getLocalAddress() {
  _socket.connectToHost(QHostAddress("239.255.255.250"), 1900);
  if (! _socket.waitForConnected(500)) {
    return QHostAddress();
  }
  QHostAddress addr = _socket.localAddress();
  _socket.close();
  return addr;
}

bool
UPNP::sendCommand(const QUrl &url, const QString &service, const QString &action, const QList<QPair<QString, QString> > &args)
{
  QByteArray command;
  // Assemble SOAP command
  command.append(QString("<?xml version=\"1.0\"?>\r\n"
                         "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                         " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                         "<s:Body>"
                         "<u:%1 xmlns:u=\"%2\">").arg(action, service));
  QPair<QString, QString> arg;
  foreach (arg, args) {
    command.append(QString("<%1>%2</%1>").arg(arg.first, arg.second));
  }
  command.append(QString("</u:%3></s:Body></s:Envelope>").arg(action));

  logDebug() << "Send UPnP command to " << url.toString() << ".";
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");
  _network.post(request, command);

  _timeout.start();

  return true;
}

void
UPNP::processDiscover() {
  _timeout.stop(); _state = IDLE;
  while (_socket.hasPendingDatagrams()) {
    QByteArray buffer;
    buffer.resize(_socket.pendingDatagramSize());
    QHostAddress sender; uint16_t port;
    _socket.readDatagram(buffer.data(), buffer.size(), &sender, &port);
    // Parse response
    QBuffer b(&buffer); b.open(QIODevice::ReadOnly);
    while (b.canReadLine()) {
      QByteArray line = b.readLine();
      if (line.startsWith("LOCATION: ")) {
        line = line.mid(10); line.chop(2);
        logDebug() << "Found UPNP device at " << line << " @ " << _socket.localAddress();
        emit foundUPnPDevice(QUrl(line));
        // continue with requesting the device description
        getDescription(QUrl(line));
      }
    }
  }
}

void
UPNP::processDescription(QNetworkReply *reply) {
  _timeout.stop(); _state = IDLE;
  disconnect(&_network, SIGNAL(finished(QNetworkReply*)),
             this, SLOT(processDescription(QNetworkReply*)));

  QDomDocument doc("");
  QString errorMsg;
  if (! doc.setContent(reply, &errorMsg)) {
    logError() << "Cannot parse device description: " << errorMsg;
    reply->close();
    emit error();
    reply->deleteLater();
    return;
  }
  reply->close();
  reply->deleteLater();

  QString baseURL;
  QDomElement root = doc.documentElement();

  QDomElement baseURLNode = root.firstChildElement("URLBase");
  if (! baseURLNode.isNull()) {
    baseURL = baseURLNode.text();
  }

  QDomElement device = root.firstChildElement("device");
  if (device.isNull()) {
    logError() << "Description does not contain a device element.";
    emit error(); return;
  }

  UPNPDeviceDescription *desc = new UPNPDeviceDescription(baseURL, device);
  emit gotDescription(desc);

  // Try to establish port mapping
  addPortMapping(desc, _iport, _eport);
}

void
UPNP::processAddPortMapping(QNetworkReply *reply) {
  _timeout.stop(); _state = IDLE;

  QDomDocument doc;
  if (! doc.setContent(reply, true)) {
    logError() << "Cannot parse AddPortMapping response.";
    reply->close(); reply->deleteLater();
    emit error(); return;
  }
  reply->close(); reply->deleteLater();

  QDomElement root = doc.documentElement();
  QDomElement body = root.firstChildElement("Body");
  if (! body.firstChildElement("AddPortMappingResponse").isNull()) {
    // success
    logDebug() << "Established port mapping.";
    emit establishedPortMapping();
  } else {
    // fail
    logError() << "Failed to establish port mapping.";
    emit error();
  }
}

void
UPNP::processGetPortMapping(QNetworkReply *reply) {
  _timeout.stop(); _state = IDLE;

  QDomDocument doc;
  if (! doc.setContent(reply, true)) {
    logError() << "Cannot parse GetSpecificPortMapping response.";
    reply->close(); reply->deleteLater();
    emit error(); return;
  }
  reply->close(); reply->deleteLater();

  QDomElement root = doc.documentElement();
  QDomElement body = root.firstChildElement("Body");
  QDomElement resp = body.firstChildElement("GetSpecificPortMappingEntryResponse");
  if (! resp.isNull()) {
    // success
    uint16_t iport = resp.firstChildElement("NewInternalPort").text().toUInt();
    QHostAddress ihost = QHostAddress(resp.firstChildElement("NewInternalClient").text());
    QString descr = resp.firstChildElement("NewPortMappingDescription").text();
    emit gotPortMapping(ihost, iport, descr);
  } else {
    // fail
    logError() << "Failed to request port mapping.";
    emit error();
  }
}

void
UPNP::onTimeout() {
  _state = IDLE;
  logError() << "Timeout.";
  emit error();
}


/* ******************************************************************************************** *
 * Implementation of UPNPDeviceDescription
 * ******************************************************************************************** */
UPNPDeviceDescription::UPNPDeviceDescription(const QString urlBase, const QDomElement &device)
{
  // parse device description...
  QDomElement deviceType = device.firstChildElement("deviceType");
  if (! deviceType.isNull()) {
    _type = deviceType.text();
  }
  QDomElement UDN = device.firstChildElement("UDN");
  if (! UDN.isNull()) {
    _UDN = UDN.text();
  }
  logDebug() << "Found device " << _type << " '" << _UDN <<"'.";

  // parse services
  QDomElement serviceList = device.firstChildElement("serviceList");
  QDomElement service = serviceList.firstChildElement("service");
  for (; ! service.isNull(); service = service.nextSiblingElement("service")) {
    _services.push_back(new UPNPServiceDescription(urlBase, service));
  }
  // parse devices
  QDomElement deviceList = device.firstChildElement("deviceList");
  QDomElement subdevice = deviceList.firstChildElement("device");
  for (; ! subdevice.isNull(); subdevice = subdevice.nextSiblingElement("device")) {
    _devices.push_back(new UPNPDeviceDescription(urlBase, subdevice));
  }
}

UPNPDeviceDescription::~UPNPDeviceDescription() {
  QList<UPNPServiceDescription *>::iterator service = _services.begin();
  for (; service != _services.end(); service++) {
    delete *service;
  }

  QList<UPNPDeviceDescription *>::iterator device = _devices.begin();
  for (; device != _devices.end(); device++) {
    delete *device;
  }
}

UPNPServiceDescription *
UPNPDeviceDescription::findServiceByType(const QString &serviceType) {
  QList<UPNPServiceDescription *>::iterator service = _services.begin();
  for (; service != _services.end(); service++) {
    if (serviceType == (*service)->type()) {
      return *service;
    }
  }

  QList<UPNPDeviceDescription *>::iterator device = _devices.begin();
  for (; device != _devices.end(); device++) {
    UPNPServiceDescription *service = (*device)->findServiceByType(serviceType);
    if (service) { return service; }
  }

  return 0;
}


/* ******************************************************************************************** *
 * Implementation of UPNPServiceDescription
 * ******************************************************************************************** */
UPNPServiceDescription::UPNPServiceDescription(const QString urlBase, const QDomElement &service)
{
  // parse service description
  QDomElement serviceType = service.firstChildElement("serviceType");
  if (! serviceType.isNull()) {
    _type = serviceType.text();
  }
  QDomElement serviceId = service.firstChildElement("serviceId");
  if (! serviceId.isNull()) {
    _id = serviceId.text();
  }
  QDomElement SCPDURL = service.firstChildElement("SCPDURL");
  if (! SCPDURL.isNull()) {
    _SCPDURL = QUrl(urlBase+SCPDURL.text());
  }
  QDomElement controlURL = service.firstChildElement("controlURL");
  if (! controlURL.isNull()) {
    _controlUrl = QUrl(urlBase+controlURL.text());
  }
  QDomElement eventSubURL = service.firstChildElement("eventSubURL");
  if (! eventSubURL.isNull()) {
    _eventSubURL = QUrl(urlBase+eventSubURL.text());
  }

  logDebug() << "Found service " << _id << " @ " << _controlUrl.toString();
}

UPNPServiceDescription::~UPNPServiceDescription() {
  // pass...
}
