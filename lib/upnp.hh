#ifndef __OVL_UPNP_HH__
#define __OVL_UPNP_HH__

#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>
#include <QNetworkAccessManager>


class UPNPDeviceDescription;
class UPNPServiceDescription;
class QDomElement;


class UPNP : public QObject
{
  Q_OBJECT

public:
  explicit UPNP(uint16_t iport, uint16_t eport, QObject *parent = 0);

public slots:
  bool discover();
  bool getDescription(const QUrl &url);
  bool getPortMapping(UPNPDeviceDescription *device, uint16_t eport);
  bool addPortMapping(UPNPDeviceDescription *device, uint16_t iport, uint16_t eport=0);
  QHostAddress getLocalAddress();

signals:
  void foundUPnPDevice(const QUrl &descrURL);
  void gotDescription(UPNPDeviceDescription *desc);
  void establishedPortMapping();
  void gotPortMapping(const QHostAddress &addr, uint16_t port, const QString &description);
  void error();

protected:
  typedef enum {
    IDLE, DISCOVERY, GET_DESCRIPION, ADD_PORTMAPPING, GET_PORTMAPPING, GET_NUMPORTMAPPING
  } State;

protected slots:
  bool sendCommand(const QUrl &url, const QString &service, const QString &action,
                   const QList< QPair<QString, QString> > &args);
  void onTimeout();
  void processDiscover();
  void processDescription(QNetworkReply *reply);
  void processAddPortMapping(QNetworkReply *reply);
  void processGetPortMapping(QNetworkReply *reply);

protected:
  State _state;
  QTimer _timeout;
  QUdpSocket _socket;
  QNetworkAccessManager _network;

  QHostAddress _localAddress;
  uint16_t _iport;
  uint16_t _eport;
};


class UPNPDeviceDescription
{
public:
  UPNPDeviceDescription(const QString urlBase, const QDomElement &device);
  ~UPNPDeviceDescription();

  UPNPServiceDescription *findServiceByType(const QString &serviceType);

protected:
  QString _type;
  QString _UDN;
  QList<UPNPServiceDescription *> _services;
  QList<UPNPDeviceDescription *> _devices;
};


class UPNPServiceDescription
{
public:
  UPNPServiceDescription(const QString urlBase, const QDomElement &service);
  ~UPNPServiceDescription();

  inline const QString &id() const { return _id; }
  inline const QString &type() const { return _type; }
  inline const QUrl &controlURL() const { return _controlUrl; }

protected:
  QString _id;
  QString _type;
  QUrl _SCPDURL;
  QUrl _controlUrl;
  QUrl _eventSubURL;
};

#endif // __OVL_UPNP_HH__
