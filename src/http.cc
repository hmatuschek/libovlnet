#include "http.hh"
#include <QUrl>


/* ********************************************************************************************* *
 * Implementation of HostName
 * ********************************************************************************************* */
HostName::HostName(const QString &name, uint16_t defaultPort)
  : _name(name), _port(defaultPort)
{
  // Split at ':'
  if (_name.contains(':')) {
    int idx = _name.indexOf(':');
    _port = _name.mid(idx+1).toUInt();
    _name = _name.left(idx);
  }
}

HostName::HostName(const HostName &other)
  : _name(other._name), _port(other._port)
{
  // pass...
}

HostName &
HostName::operator =(const HostName &other) {
  _name = other._name;
  _port = other._port;
  return *this;
}

const QString &
HostName::name() const {
  return _name;
}

uint16_t
HostName::port() const {
  return _port;
}

bool
HostName::isOvlNode() const {
  return _name.endsWith(".ovl");
}

Identifier
HostName::ovlId() const {
  return Identifier::fromBase32(_name.left(_name.size()-4));
}


/* ********************************************************************************************* *
 * Implementation of URI
 * ********************************************************************************************* */
URI::URI()
  : _proto(), _host(""), _path(), _query()
{

}

URI::URI(const QString &uri)
  : _proto(), _host(""), _path(), _query()
{
  QUrl url(uri);
  _proto = url.scheme();
  _host  = HostName(url.host(), url.port());
  _path  = url.path();
  _query = url.query();
}

URI::URI(const URI &other)
  : _proto(other._proto), _host(other._host), _path(other._path), _query(other._query)
{
  // pass...
}

URI &
URI::operator =(const URI &other) {
  _proto = other._proto;
  _host = other._host;
  _path = other._path;
  _query = other._query;
  return *this;
}
