#include "settings.hh"
#include "lib/logger.h"
#include <QJsonArray>


/* ********************************************************************************************* *
 * Implementation of Settings
 * ********************************************************************************************* */
Settings::Settings(const QString &filename, QObject *parent)
  : QObject(parent), _file(filename), _socksServiceWhitelist()
{
  if (!_file.open(QIODevice::ReadOnly)) { return; }
  logDebug() << "Settings: Load settings from " << filename;
  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(_file.readAll(), &err);
  _file.close();

  if (QJsonParseError::NoError != err.error) {
    logError() << "Settings: Parser error: " << err.errorString() << ".";
    return;
  }

  if (! doc.isObject()) {
    logError() << "Settings: Malformed settings file.";
    return;
  }

  // Check for socks service whitelist
  if (doc.object().contains("socks_whitelist") && doc.object().value("socks_whitelist").isArray()) {
    logDebug() << "Settings: Read SOCKS white list.";
    _socksServiceWhitelist = ServiceWhiteList(doc.object().value("socks_whitelist").toArray());
  } else {
    logDebug() << "Settings: No SOCKS white list specified.";
  }
  // Check for shell service whitelist
  if (doc.object().contains("shell_whitelist") && doc.object().value("shell_whitelist").isArray()) {
    logDebug() << "Settings: Read shell white list.";
    _shellServiceWhitelist = ServiceWhiteList(doc.object().value("shell_whitelist").toArray());
  } else {
    logDebug() << "Settings: No shell white list specified.";
  }
}

void
Settings::save() {
  if (! _file.open(QIODevice::WriteOnly)) {
    logWarning() << "Settings: Cannot save settings to " << _file.fileName()
                 << ": " << _file.errorString();
    return;
  }

  QJsonObject obj;
  obj.insert("socks_whitelist", _socksServiceWhitelist.toJson());
  obj.insert("shell_whitelist", _shellServiceWhitelist.toJson());
  QJsonDocument doc(obj);
  _file.write(doc.toJson());
  _file.close();
}

ServiceWhiteList &
Settings::socksServiceWhiteList() {
  return _socksServiceWhitelist;
}

ServiceWhiteList &
Settings::shellServiceWhiteList() {
  return _shellServiceWhitelist;
}


/* ********************************************************************************************* *
 * Implementation of SocksServiceWhiteList
 * ********************************************************************************************* */
ServiceWhiteList::ServiceWhiteList(const QJsonArray &lst)
  : QSet<Identifier>()
{
  for (QJsonArray::const_iterator item = lst.begin(); item != lst.end(); item++) {
    if ((*item).isString()) {
      Identifier id = Identifier::fromBase32((*item).toString());
      logDebug() << "Add node " << id << " to SOCKS white list.";
      this->insert(id);
    }
  }
}

QJsonArray
ServiceWhiteList::toJson() const {
  QJsonArray lst;
  QSet<Identifier>::const_iterator item = this->begin();
  for (; item != this->end(); item++) {
    lst.append(item->toBase32());
  }
  return lst;
}
