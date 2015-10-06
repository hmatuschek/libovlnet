#include "socksconnection.h"
#include "application.h"


SOCKSConnection::SOCKSConnection(Application &app, QObject *parent)
  : SOCKSOutStream(app.dht(), parent), _application(app)
{
  // pass...
}


