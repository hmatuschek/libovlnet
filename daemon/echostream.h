#ifndef ECHOSTREAM_H
#define ECHOSTREAM_H

#include "lib/crypto.h"

class EchoStream : public SecureSocket
{
public:
  EchoStream(Identity &id);

  void handleDatagram(const uint8_t *data, size_t len);
};

#endif // ECHOSTREAM_H
