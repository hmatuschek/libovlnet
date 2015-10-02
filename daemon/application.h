#ifndef APPLICATION_H
#define APPLICATION_H

#include <QCoreApplication>
#include "lib/crypto.h"
#include "lib/dht.h"


class Application : public QCoreApplication, public StreamHandler
{
  Q_OBJECT

public:
  explicit Application(int argc, char *argv[]);

  SecureStream *newStream(uint16_t service);
  bool allowStream(uint16_t service, const NodeItem &peer);
  void streamStarted(SecureStream *stream);
  void streamFailed(SecureStream *stream);

protected:
  Identity *_identity;
  DHT *_dht;
};

#endif // APPLICATION_H
