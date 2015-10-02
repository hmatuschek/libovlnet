#include "lib/crypto.h"

#include <QDebug>
#include <QDir>

class TempStream: public SecureStream {
public:
  TempStream(Identity &id) : SecureStream(id)
  {
    uint8_t data[1024]; int datalen=1024;
    datalen = prepare(data, datalen);
    qDebug() << QByteArray((char *)data, datalen).toHex();
  }

  void handleDatagram(const uint8_t *data, size_t len) {
    // pass...
  }
};

int main() {
  Identity *id = Identity::load(QDir::home().path()+"/.vlf/identity.pem");

  uint8_t pubKey[100]; int keyLen=100;
  keyLen = id->publicKey(pubKey, keyLen);
  qDebug() << "Got pubkey" << QByteArray((char *)pubKey, keyLen).toBase64() << ":" << keyLen;
  Identity *id2 = Identity::fromPublicKey(pubKey, keyLen);

  uint8_t sig[100]; int sigLen=100;
  sigLen = id->sign((uint8_t *)"abc", 3, sig, sigLen);
  qDebug() << "Success:" << id2->verify((uint8_t *)"abc", 3, sig, sigLen);

  TempStream str(*id);
  return 0;
}
