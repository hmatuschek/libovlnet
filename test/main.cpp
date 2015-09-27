#include "lib/crypto.h"

#include <QDebug>
#include <QDir>

class TempStream: public SecureStream {
public:
  TempStream(Identity &id) : SecureStream(id)
  {
    uint8_t data[1024]; int datalen=1024;
    datalen = prepare(data, datalen);
    qDebug() << "Prepared init message with" << datalen << "bytes.";
    qDebug() << QByteArray((char *)data, datalen).toHex();
    qDebug() << "Session verified:" << verify(data, datalen);
  }

  void handleDatagram(uint32_t seq, const uint8_t *data, size_t len) {
    // pass...
  }
};

int main() {
  Identity *id = Identity::load(QDir::home().path()+"/.vlf/identity.pem");
  uint8_t pubKey[100]; int keylen=100;
  keylen = id->publicKey(pubKey, keylen);
  Identity *id2 = Identity::fromPublicKey(pubKey, keylen);
  qDebug() << "Success:" << id2->hasPublicKey();
  return 0;
}
