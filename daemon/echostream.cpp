#include "echostream.h"

EchoStream::EchoStream(bool incomming, Identity &id)
  : SecureStream(incomming, id)
{
  // pass...
}

void
EchoStream::handleDatagram(uint32_t seq, const uint8_t *data, size_t len) {
  sendDatagram(data, len);
}
