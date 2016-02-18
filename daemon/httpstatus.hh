#ifndef __OVLNET_DAEMON_HTTP_STATUS_H__
#define __OVLNET_DAEMON_HTTP_STATUS_H__
#include "lib/httpservice.hh"

class HttpStatus: public HttpRequestHandler
{
  Q_OBJECT

public:
  HttpStatus(Node &dht, QObject *parent=0);

  bool acceptReqest(HttpRequest *request);
  HttpResponse *processRequest(HttpRequest *request);

protected:
  Node &_dht;
};

#endif // __OVLNET_DAEMON_HTTP_STATUS_H__
