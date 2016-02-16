#include "httpstatus.hh"
#include "lib/dht.h"

HttpStatus::HttpStatus(DHT &dht, QObject *parent)
  : HttpRequestHandler(parent), _dht(dht)
{
  // pass...
}

bool
HttpStatus::acceptReqest(HttpRequest *request) {
  if ("/status" == request->path()) {
    return true;
  }
  return false;
}

HttpResponse *
HttpStatus::processRequest(HttpRequest *request) {
  QString resp =
      "<html> <body> <h1> Status of Node </h1>"
      "<h3> Id: %1</h3>"
      "<table>"
      " <tr><td>Active streams</td> <td>%2</td></tr>"
      " <tr><td>Bytes received</td> <td>%3</td></tr>"
      " <tr><td>Bytes send</td> <td>%4</td></tr>"
      "</table>"
      "</body></html>";
  return new HttpStringResponse(
        HTTP_OK, resp.arg(_dht.id().toBase32()).arg(_dht.numSockets()).arg(_dht.bytesReceived()).arg(_dht.bytesSend()),
        request->connection());
}


