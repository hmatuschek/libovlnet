#ifndef ECHOSTREAM_H
#define ECHOSTREAM_H

#include "lib/crypto.hh"

class EchoStream : public SecureSocket
{
public:
  EchoStream(Node &dht);

  void handleDatagram(const uint8_t *data, size_t len);
};

#endif // ECHOSTREAM_H
