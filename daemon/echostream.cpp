#include "echostream.h"

EchoStream::EchoStream(Identity &id)
  : SecureSocket(id)
{
  // pass...
}

void
EchoStream::handleDatagram(const uint8_t *data, size_t len) {
  if ((0 == data) && (0 == len)) {
    sendNull();
  } else {
    sendDatagram(data, len);
  }
}
