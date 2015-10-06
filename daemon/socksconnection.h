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

#endif // SOCKSCONNECTION_H
