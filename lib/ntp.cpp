#include "ntp.h"
#include "logger.h"
#include <QtEndian>
#include <QHostInfo>
#include <QEventLoop>



/* ********************************************************************************************* *
 * Non-public declaraions
 * ********************************************************************************************* */

/** NTP Timestamp format. */
struct __attribute__((packed)) NTPTimestamp {
  /** Number of seconds passed since Jan 1 1900, in big-endian format. */
  quint32 seconds;
  /** Fractional time part, in <tt>1/0xFFFFFFFF</tt>s of a second. */
  quint32 fraction;

  /** Construction from DateTime. */
  static NTPTimestamp fromDateTime(const QDateTime &dateTime);
  /** Conversion to DateTime. */
  QDateTime toDateTime() const;
};


struct __attribute__((packed)) NTPPacketFlags {
  unsigned char mode: 3;
  unsigned char versionNumber: 3;
  unsigned char leapIndicator: 2;
};


struct __attribute__((packed)) NTPPacket{
  /** Flags. */
  NTPPacketFlags flags;
  /** Stratum of the clock. */
  quint8 stratum;
  /** Maximum interval between successive messages, in log2 seconds. Note that the value is signed. */
  qint8 poll;
  /** Precision of the clock, in log2 seconds. Note that the value is signed. */
  qint8 precision;
  /** Round trip time to the primary reference source, in NTP short format. */
  qint32 rootDelay;
  /** Nominal error relative to the primary reference source. */
  qint32 rootDispersion;
  /** Reference identifier (either a 4 character string or an IP address). */
  qint8 referenceID[4];
  /** The time at which the clock was last set or corrected. */
  NTPTimestamp referenceTimestamp;
  /** The time at which the request departed the client for the server. */
  NTPTimestamp originateTimestamp;
  /** The time at which the request arrived at the server. */
  NTPTimestamp receiveTimestamp;
  /** The time at which the reply departed the server for client. */
  NTPTimestamp transmitTimestamp;

  /** Assembles a simple NTP request. */
  NTPPacket();

  void setTransmittTime();
};


/** Optional part of an NTP packet. */
struct __attribute__((packed)) NTPAuthenticationInfo {
  /** Key identifier. */
  quint32 keyId;
  /** Message Digest. */
  quint8 messageDigest[16];
};

/** Full NTP packet. */
struct __attribute__((packed)) NTPFullPacket {
  NTPPacket basic;
  NTPAuthenticationInfo auth;

  NTPFullPacket();
};


/* ********************************************************************************************* *
 * Implementation of NTPTimestamp
 * ********************************************************************************************* */
NTPTimestamp
NTPTimestamp::fromDateTime(const QDateTime &dateTime) {
  /* Convert given time to number of milliseconds passed since Jan 1 1900. */
  qint64 ntpMSecs = dateTime.toMSecsSinceEpoch() - -2208988800000ll;
  quint32 seconds = ntpMSecs / 1000;
  quint32 fraction = 0x100000000ll * (ntpMSecs % 1000) / 1000;

  /* Convert to big-endian. */
  NTPTimestamp result;
  result.seconds = qToBigEndian(seconds);
  result.fraction = qToBigEndian(fraction);
  return result;
}

QDateTime
NTPTimestamp::toDateTime() const {
  /* Convert to host byteorder. */
  quint32 seconds = qFromBigEndian(this->seconds);
  quint32 fraction = qFromBigEndian(this->fraction);
  /* Convert NTP timestamp to number of milliseconds passed since Jan 1 1900. */
  qint64 ntpMSecs = seconds * 1000ll + fraction * 1000ll / 0x100000000ll;
  /* Construct Qt date time. */
  return QDateTime::fromMSecsSinceEpoch(ntpMSecs - 2208988800000ll);
}


/* ********************************************************************************************* *
 * Implementation of NTPPacket
 * ********************************************************************************************* */
NTPPacket::NTPPacket()
{
  memset(this, 0, sizeof(NTPPacket));
  flags.mode = 3;
  flags.versionNumber = 4;
}

void
NTPPacket::setTransmittTime() {
  transmitTimestamp = NTPTimestamp::fromDateTime(QDateTime::currentDateTimeUtc());
}


/* ********************************************************************************************* *
 * Implementation of NTPFullPacket
 * ********************************************************************************************* */
NTPFullPacket::NTPFullPacket()
{
  memset(this, 0, sizeof(NTPFullPacket));
}


/* ********************************************************************************************* *
 * Implementation of NTPClient
 * ********************************************************************************************* */
NTPClient::NTPClient(QObject *parent)
  : QObject(parent), _socket(), _timer(), _offset(0)
{
  _socket.bind(QHostAddress::Any);
  _timer.setInterval(2000);
  _timer.setSingleShot(true);

  connect(&_socket, SIGNAL(readyRead()), this, SLOT(onDatagramReceived()));
  connect(&_timer, SIGNAL(timeout()), this, SIGNAL(timeout()));
}

qint64
NTPClient::offset() const {
  return _offset;
}

bool
NTPClient::request(const QString &name, uint16_t port) {
  QHostInfo info = QHostInfo::fromName(name);
  if (QHostInfo::NoError == info.error()) {
    return request(info.addresses().first(), port);
  }
  return false;
}

bool
NTPClient::request(const QHostAddress &addr, uint16_t port) {
  NTPPacket packet;
  packet.setTransmittTime();
  if (sizeof(NTPPacket) != _socket.writeDatagram((char *) &packet, sizeof(NTPPacket), addr, port)) {
    return false;
  }
  _timer.start();
  return true;
}

void
NTPClient::onDatagramReceived() {
  while (_socket.hasPendingDatagrams()) {
    NTPFullPacket packet;
    QHostAddress address;
    quint16 port;

    if(int(sizeof(NTPPacket)) > _socket.readDatagram(
         (char *)&packet, sizeof(NTPFullPacket), &address, &port)) {
      continue;
    }

    _timer.stop();

    QDateTime now = QDateTime::currentDateTime();
    _offset =
        (packet.basic.originateTimestamp.toDateTime().msecsTo(packet.basic.receiveTimestamp.toDateTime())
         + now.msecsTo(packet.basic.transmitTimestamp.toDateTime())) / 2;
    logDebug() << "Got NTP local offset:" << _offset << "ms.";
    emit received(_offset);
  }
}

qint64
NTPClient::getOffset(const QString &name, uint16_t port) {
  NTPClient client;

  QEventLoop loop;
  // Exit loop on response or timeout
  QObject::connect(&client, SIGNAL(received(int64_t)), &loop, SLOT(quit()));
  QObject::connect(&client, SIGNAL(timeout()), &loop, SLOT(quit()));
  // send request
  if (! client.request(name, port)) { return 0; }
  // Wait for response or timeout
  loop.exec();
  // done.
  return client.offset();
}
