#include "sockswhitelist.hh"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QFile>


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
