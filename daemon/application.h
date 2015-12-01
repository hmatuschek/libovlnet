#ifndef APPLICATION_H
#define APPLICATION_H

#include <QCoreApplication>
#include "lib/crypto.h"
#include "lib/dht.h"
#include "qhal.h"
#include "sockswhitelist.h"


class Application : public QCoreApplication
{
  Q_OBJECT
public:
  explicit Application(int argc, char *argv[]);

  DHT &dht();

protected:
  class HalChatService: public AbstractService
  {
  public:
    HalChatService(Application &app);
    SecureSocket *newSocket();
    bool allowConnection(const NodeItem &peer);
    void connectionStarted(SecureSocket *stream);
    void connectionFailed(SecureSocket *stream);
  protected:
    Application &_application;
  };

  class SocksService: public AbstractService
  {
  public:
    SocksService(Application &app);
    SecureSocket *newSocket();
    bool allowConnection(const NodeItem &peer);
    void connectionStarted(SecureSocket *stream);
    void connectionFailed(SecureSocket *stream);
  protected:
    Application &_application;
  };

protected:
  Identity *_identity;
  DHT *_dht;
  QHalModel _model;
  SOCKSWhitelist _socksWhiteList;
};

#endif // APPLICATION_H
