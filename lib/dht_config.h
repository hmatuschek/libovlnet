#ifndef DHT_CONFIG_H
#define DHT_CONFIG_H

/** Size of of the hash to use, e.g. RMD160 -> 20bytes. */
#define DHT_HASH_SIZE        20
/** Maximum message size per UDP packet. */
#define DHT_MAX_MESSAGE_SIZE 1024
/** Minimum message size per UDP packet. */
#define DHT_MIN_MESSAGE_SIZE DHT_HASH_SIZE

/** The size of the triple (hash, IPv4, port). */
#define DHT_TRIPLE_SIZE (DHT_HASH_SIZE + 4 + 2)
/** The max. number of triples in a response. */
#define DHT_MAX_TRIPLES int((DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-1)/DHT_TRIPLE_SIZE)
/** The max. data response. */
#define DHT_MAX_DATA_SIZE (DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE)
/** The max. public key size for a START_STREAM message. */
#define DHT_MAX_PUBKEY_SIZE (DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-3)

/** The bucket size.
 * It is ensured that a complete bucket can be transferred with one UDP message. */
#define DHT_K std::min(8, DHT_MAX_TRIPLES)

#endif // DHT_CONFIG_H
