#include "socksconnection.h"
#include "application.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>


SOCKSConnection::SOCKSConnection(Application &app, QObject *parent)
  : SOCKSOutStream(app.dht(), parent), _application(app)
{
  // pass...
}


SOCKSWhitelist::SOCKSWhitelist(const QString &filename)
  : _whitelist()
{
  QFile file(filename);
  if (! file.open(QIODevice::ReadOnly)) {
    logWarning() << "Can not read SOCKS whitelist from" << filename;
    return;
  }

  QJsonDocument doc = QJsonDocument::fromJson(file.readAll()); file.close();
  if (! doc.isArray()) { return; }
  QJsonArray list = doc.array();
  foreach(QJsonValue node, list) {
    if (node.isString()) {
      _whitelist.insert(QByteArray::fromHex(node.toString().toLocal8Bit()));
    }
  }
}

bool
SOCKSWhitelist::empty() const {
  return _whitelist.isEmpty();
}

bool
SOCKSWhitelist::allowed(const Identifier &id) const {
  return _whitelist.contains(id);
}
