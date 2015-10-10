#ifndef SOCKSCONNECTION_H
#define SOCKSCONNECTION_H

#include <QObject>
#include "lib/socks.h"

class Application;

class SOCKSConnection : public SOCKSOutStream
{
  Q_OBJECT

public:
  explicit SOCKSConnection(Application &app, QObject *parent = 0);

protected:
  Application &_application;
};

class SOCKSWhitelist
{
public:
  SOCKSWhitelist(const QString &filename);

  bool empty() const;
  bool allowed(const Identifier &id) const;

protected:
  QSet<Identifier> _whitelist;
};

#endif // SOCKSCONNECTION_H
