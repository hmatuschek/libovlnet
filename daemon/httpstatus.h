#ifndef __OVLNET_DAEMON_HTTP_STATUS_H__
#define __OVLNET_DAEMON_HTTP_STATUS_H__
#include "lib/httpservice.h"

class HttpStatus: public HttpRequestHandler
{
  Q_OBJECT

public:
  HttpStatus(DHT &dht, QObject *parent=0);

  bool acceptReqest(HttpRequest *request);
  HttpResponse *processRequest(HttpRequest *request);

protected:
  DHT &_dht;
};

#endif // __OVLNET_DAEMON_HTTP_STATUS_H__
