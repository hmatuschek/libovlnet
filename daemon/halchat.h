#ifndef HALCHAT_H
#define HALCHAT_H

#include "lib/dht.h"
#include "lib/crypto.h"
#include "qhal.h"

#include <QObject>


class HalChat : public QObject, public SecureSocket
{
  Q_OBJECT

public:
  explicit HalChat(DHT &dht, QHalModel &model,QObject *parent = 0);

protected:
  void handleDatagram(const uint8_t *data, size_t len);

protected:
  QHalModel &_model;
};

#endif // HALCHAT_H
