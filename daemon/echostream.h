#ifndef ECHOSTREAM_H
#define ECHOSTREAM_H

#include "lib/crypto.h"

class EchoStream : public SecureStream
{
public:
  EchoStream(Identity &id);

  void handleDatagram(uint32_t seq, const uint8_t *data, size_t len);
};

#endif // ECHOSTREAM_H
