#ifndef UTILS_H
#define UTILS_H

#include "logger.h"

#include <inttypes.h>


/** Returns a random byte.
 * @ingroup utils */
inline uint8_t dht_rand8() {
  return (qrand() & 0xff);
}

/** Returns a 16bit random value.
 * @ingroup utils */
inline uint16_t dht_rand16() {
#if (RAND_MAX>=0xffff)
  return (qrand() & 0xffff);
#else
  return (uint16_t(dht_rand8())<<8)+dht_rand8()
#endif
}

/** Returns a 32bit random value.
 * @ingroup utils */
inline uint32_t dht_rand32() {
  return (uint32_t(dht_rand16())<<16) + dht_rand16();
}

/** Returns a 64bit random value.
 * @ingroup utils */
inline uint64_t dht_rand64() {
  return (uint64_t(dht_rand32())<<32) + dht_rand32();
}

#endif // UTILS_H
