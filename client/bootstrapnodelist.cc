#include "bootstrapnodelist.hh"
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>


BootstrapNodeList::BootstrapNodeList()
  : QList< QPair<QString, uint16_t> >(), _file()
{
  // pass...
}

BootstrapNodeList::BootstrapNodeList(const QString &path)
 : QList< QPair<QString, uint16_t> >(), _file(path)
{
  if (!_file.open(QIODevice::ReadOnly)) { return; }
  // Read json
  QJsonDocument doc = QJsonDocument::fromJson(_file.readAll()); _file.close();
  // Check type
  if (! doc.isArray()) { return; }
  QJsonValue val;
  foreach (val, doc.array()) {
    if (! val.isString()) { continue; }
    QString host = val.toString();
    uint16_t port = 7741;
    // Check if port is specified
    if (host.contains(':')) {
      QStringList parts = host.split(':');
      if (2 != parts.size()) { continue; }
      host = parts.front();
      port = parts.back().toUInt();
    }
    this->append(QPair<QString, uint16_t>(host, port));
  }
}

BootstrapNodeList::BootstrapNodeList(const BootstrapNodeList &other)
  : QList< QPair<QString, uint16_t> >(other), _file(other._file.fileName())
{
  // pass
}

BootstrapNodeList &
BootstrapNodeList::operator =(const BootstrapNodeList &other) {
  QList< QPair<QString, uint16_t> >::operator =(other);
  _file.setFileName(other._file.fileName());
  return *this;
}

void
BootstrapNodeList::insert(const QString &host, uint16_t port) {
  QPair<QString, uint16_t> pair(host, port);
  if (this->contains(pair)) { return; }
  this->append(pair);
  // Serialize as JSON
  QJsonArray peers;
  QPair<QString, uint16_t> peer;
  foreach (peer, *this) {
    peers.append(QString("%1:%2").arg(peer.first).arg(peer.second));
  }
  QJsonDocument doc;
  doc.setArray(peers);
  // Save in file
  if (!_file.open(QIODevice::WriteOnly)) { return; }
  _file.write(doc.toJson());
  _file.close();
}
