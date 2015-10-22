#include "settings.h"
#include "lib/logger.h"


/* ********************************************************************************************* *
 * Implementation of Settings
 * ********************************************************************************************* */
Settings::Settings(const QString &filename, QObject *parent)
  : QObject(parent), _file(filename), _socksServiceSettings()
{
  if (!_file.open(QIODevice::ReadOnly)) { return; }
  logDebug() << "Settings: Load client settings from " << filename;
  QJsonDocument doc = QJsonDocument::fromJson(_file.readAll());
  _file.close();
  if (! doc.isObject()) { return; }
  // Check for socks service settings
  if (doc.object().contains("socks_service") && doc.object().value("socks_service").isObject()) {
    _socksServiceSettings = SocksServiceSettings(doc.object().value("socks_service").toObject());
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
  obj.insert("socks_service", _socksServiceSettings.toJson());
  QJsonDocument doc(obj);
  _file.write(doc.toJson());
  _file.close();
}

SocksServiceSettings &
Settings::socksServiceSettings() {
  return _socksServiceSettings;
}


/* ********************************************************************************************* *
 * Implementation of SocksServiceSettings
 * ********************************************************************************************* */
SocksServiceSettings::SocksServiceSettings(const QJsonObject &object)
  : _enabled(object.value("enabled").toBool(false)),
    _allowBuddies(object.value("allow_buddies").toBool(false)),
    _allowWhitelist(object.value("allow_whitelist").toBool(false)),
    _whitelist(object.value("whitelist").toArray())
{
  // pass...
}

SocksServiceSettings &
SocksServiceSettings::operator=(const SocksServiceSettings &other) {
  _enabled = other._enabled;
  _allowBuddies = other._allowBuddies;
  _allowWhitelist = other._allowWhitelist;
  _whitelist = other._whitelist;
  return *this;
}

bool
SocksServiceSettings::enabled() const {
  return _enabled;
}

void
SocksServiceSettings::enable(bool enabled) {
  _enabled = enabled;
}

bool
SocksServiceSettings::allowBuddies() const {
  return _allowBuddies;
}

void
SocksServiceSettings::setAllowBuddies(bool allow) {
  _allowBuddies = allow;
}

bool
SocksServiceSettings::allowWhiteListed() const {
  return _allowWhitelist;
}

void
SocksServiceSettings::setAllowWhiteListed(bool allow) {
  _allowWhitelist = allow;
}

const SocksServiceWhiteList &
SocksServiceSettings::whitelist() const {
  return _whitelist;
}

SocksServiceWhiteList &
SocksServiceSettings::whitelist() {
  return _whitelist;
}

QJsonObject
SocksServiceSettings::toJson() const {
  QJsonObject obj;
  obj.insert("enabled", _enabled);
  obj.insert("allow_buddies", _allowBuddies);
  obj.insert("allow_whitelist", _allowWhitelist);
  obj.insert("whitelist", _whitelist.toJson());
  return obj;
}


/* ********************************************************************************************* *
 * Implementation of SocksServiceWhiteList
 * ********************************************************************************************* */
SocksServiceWhiteList::SocksServiceWhiteList(const QJsonArray &lst)
  : QSet<Identifier>()
{
  for (QJsonArray::const_iterator item = lst.begin(); item != lst.end(); item++) {
    if (item->isString()) {
      this->insert(Identifier::fromBase32(item->toString()));
    }
  }
}

QJsonArray
SocksServiceWhiteList::toJson() const {
  QJsonArray lst;
  QSet<Identifier>::const_iterator item = this->begin();
  for (; item != this->end(); item++) {
    lst.append(item->toBase32());
  }
  return lst;
}
