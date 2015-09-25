#ifndef IDENTITY_H
#define IDENTITY_H

#include <QObject>
#include <QFile>

#include "dht.h"
#include <openssl/evp.h>

class Identity : public QObject
{
  Q_OBJECT

protected:
  explicit Identity(EVP_PKEY *key, QObject *parent = 0);

public:
  virtual ~Identity();

  bool hasPrivateKey() const;
  const Identifier &id() const;

public:
  static Identity *newIdentity(const QFile &path);
  static Identity *load(const QFile &path);

protected:
  EVP_PKEY *_keyPair;
  Identifier _fingerprint;
};

#endif // IDENTITY_H
