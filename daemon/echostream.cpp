#include "echostream.h"

EchoStream::EchoStream(Identity &id)
  : SecureStream(id)
{
  // pass...
}

void
EchoStream::handleDatagram(uint32_t seq, const uint8_t *data, size_t len) {
  sendDatagram(data, len);
}
