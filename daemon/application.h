#ifndef APPLICATION_H
#define APPLICATION_H

#include <QCoreApplication>
#include "lib/crypto.h"
#include "lib/dht.h"


class Application : public QCoreApplication, public SocketHandler
{
  Q_OBJECT

public:
  explicit Application(int argc, char *argv[]);

  SecureSocket *newStream(uint16_t service);
  bool allowStream(uint16_t service, const NodeItem &peer);
  void streamStarted(SecureSocket *stream);
  void streamFailed(SecureSocket *stream);

protected:
  Identity *_identity;
  DHT *_dht;
};

#endif // APPLICATION_H
