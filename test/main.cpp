#include "lib/crypto.h"

#include <QDebug>
#include <QDir>

int main() {
  Identity *id = Identity::load(QDir::home().path()+"/.vlf/identity.pem");
  qDebug() << "Public key size:" << id->publicKey(0, 0);
  const char *txt = "abc";
  uint8_t    sig[256];
  int        siglen=256;
  if(0 > (siglen = id->sign((const uint8_t *)txt, 3, sig, siglen))) {
    qDebug() << "Failed to sign data";
    return -1;
  }
  qDebug() << "Signature length: " << siglen;
  if(! id->verify((const uint8_t *)txt, 3, sig, siglen)) {
    qDebug() << "Failed to verify data";
  }

  return 0;
}
