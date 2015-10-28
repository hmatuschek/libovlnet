#ifndef APPLICATION_H
#define APPLICATION_H

#include <QCoreApplication>
#include "lib/crypto.h"
#include "lib/dht.h"
#include "qhal.h"
#include "settings.h"


class Application : public QCoreApplication, public ServiceHandler
{
  Q_OBJECT

public:
  explicit Application(int argc, char *argv[]);

  DHT &dht();

protected:
  SecureSocket *newSocket(uint16_t service);
  bool allowConnection(uint16_t service, const NodeItem &peer);
  void connectionStarted(SecureSocket *stream);
  void connectionFailed(SecureSocket *stream);

protected:
  Identity *_identity;
  DHT *_dht;
  QHalModel _model;
  Settings _settings;
};

#endif // APPLICATION_H
