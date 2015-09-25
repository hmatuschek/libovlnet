#ifndef NATPMPCLIENT_H
#define NATPMPCLIENT_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QDateTime>
#include <QTimer>
#include <QHash>


/** Implements a trivial NAT-PMP client to request a port mapping from a NAT. */
class PMPClient : public QObject
{
  Q_OBJECT

public:
  /** Represents an established mapping. */
  class MapItem
  {
  public:
    /** Empty constructor. */
    MapItem();
    /** Constructor. */
    MapItem(uint16_t iport, uint16_t eport, uint32_t lifetime,
            const QHostAddress &addr, uint16_t port);
    /** Copy constructor. */
    MapItem(const MapItem &other);
    /** Assignment operator. */
    MapItem &operator=(const MapItem &other);

    /** Retruns the internal port of the mapping. */
    inline uint16_t iport() const { return _iport; }
    /** Retruns the external port of the mapping. */
    inline uint16_t eport() const { return _eport; }
    /** Returns the address of the NAT-PMP device. */
    inline const QHostAddress &addr() const { return _addr; }
    /** Returns the port of the NAT-PMP service. */
    inline uint16_t port() const { return _port; }
    /** Returns @c true if the mapping should be refreshed. */
    inline bool needsRefresh() const {
      return _timestamp.addSecs((_lifetime*2)/3) < QDateTime::currentDateTime();
    }
    /** Returns true if the mapping is expired. */
    inline bool expired() const {
      return _timestamp.addSecs(_lifetime) < QDateTime::currentDateTime();
    }

  protected:
    /** The internal port of the mapping. */
    uint16_t     _iport;
    /** The external port of the mapping. */
    uint16_t     _eport;
    /** The lifetime of the mapping in seconds. */
    uint32_t     _lifetime;
    /** The address of the NAT-PMP device. */
    QHostAddress _addr;
    /** The port of the NAT-PMP service. */
    uint16_t     _port;
    /** The timestamp of the last refresh. */
    QDateTime    _timestamp;
  };

public:
  /** Constructor. */
  explicit PMPClient(QObject *parent = 0);

public slots:
  /** Requests a mapping for the specified port from the given gateway. */
  void requestMap(uint16_t iport, const QHostAddress &addr, uint16_t port=5351);

signals:
  /** Gets emitted on success. */
  void success(uint16_t iport, uint16_t eport);
  /** Gets emitted on failiure. */
  void failed(uint16_t iport);


protected:
  /** Internal used method to send a map request. */
  bool _sendMapRequest(uint16_t iport, uint16_t eport, const QHostAddress &addr, uint16_t port);

protected slots:
  /** Gets called on reception of a UDP datagram. */
  void _onDatagramReceived();
  /** Gets called by the request timer. */
  void _onReqTimer();
  /** Gets called by the mapping timer. */
  void _onMapTimer();

protected:
  /** The UDP socket. */
  QUdpSocket _socket;
  /** Timestamp of the pending request. */
  QDateTime  _reqTimestamp;
  /** Internal port of the pending request. */
  uint16_t   _iport;
  /** Address of the gateway of the pending request. */
  QHostAddress _addr;
  /** Port of the NAT-PMP service of the pending request. */
  uint16_t     _port;
  /** Established mappings. */
  QHash<uint16_t, MapItem> _mappings;
  /** The request timer. */
  QTimer _reqTimer;
  /** The mapping timer. */
  QTimer _mapTimer;
};

#endif // NATPMPCLIENT_H
