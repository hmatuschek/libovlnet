#ifndef DHT_CONFIG_H
#define DHT_CONFIG_H

/** Size of the magic cookie. */
#define DHT_COOKIE_SIZE      20
/** Size of of the hash to use, e.g. RMD160 -> 20bytes. */
#define DHT_HASH_SIZE        20
/** Maximum message size per UDP packet. */
#define DHT_MAX_MESSAGE_SIZE 8192
/** Minimum message size per UDP packet. */
#define DHT_MIN_MESSAGE_SIZE DHT_HASH_SIZE

/** The size of the triple (ID, IPv6, port). */
#define DHT_TRIPLE_SIZE (DHT_HASH_SIZE + 16 + 2)
/** The max. number of triples in a response. */
#define DHT_MAX_TRIPLES ((DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-1)/DHT_TRIPLE_SIZE)
/** The max. payload size in a DATA message. */
#define DHT_MAX_DATA_SIZE (DHT_MAX_MESSAGE_SIZE-DHT_COOKIE_SIZE)

/** The bucket size.
 * It is ensured that a complete bucket can be transferred within one UDP message. */
#define DHT_K std::min(8, DHT_MAX_TRIPLES)

#endif // DHT_CONFIG_H
