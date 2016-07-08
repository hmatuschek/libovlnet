#ifndef __OVL_DHT_CONFIG_HH__
#define __OVL_DHT_CONFIG_HH__

/** Size of the magic cookie. */
#define OVL_COOKIE_SIZE      20
/** Size of of the hash to use, e.g. RMD160 -> 20bytes. */
#define OVL_HASH_SIZE        20
/** Maximum message size per UDP packet. */
#define OVL_MAX_MESSAGE_SIZE 8192
/** Minimum message size per UDP packet. */
#define OVL_MIN_MESSAGE_SIZE OVL_HASH_SIZE

/** The size of the triple (ID, IPv6, port). */
#define OVL_TRIPLE_SIZE (OVL_HASH_SIZE + 16 + 2)
/** The max. number of triples in a response. */
#define OVL_MAX_TRIPLES ((OVL_MAX_MESSAGE_SIZE-OVL_COOKIE_SIZE-1)/OVL_TRIPLE_SIZE)
/** The max. payload size in a DATA message. */
#define OVL_MAX_DATA_SIZE (OVL_MAX_MESSAGE_SIZE-OVL_COOKIE_SIZE)

/** The bucket size.
 * It is ensured that a complete bucket can be transferred within one UDP message. */
#define OVL_K std::min(8, OVL_MAX_TRIPLES)

#define OVL_PING_REQU_SIZE            (OVL_COOKIE_SIZE+2*OVL_HASH_SIZE+1)
#define OVL_PING_RESP_SIZE            OVL_PING_REQU_SIZE
#define OVL_SEARCH_MIN_REQU_SIZE      (OVL_COOKIE_SIZE+OVL_HASH_SIZE+1)
#define OVL_SEARCH_MIN_RESP_SIZE      (OVL_COOKIE_SIZE+1)
#define OVL_CONNECT_MIN_REQU_SIZE     (OVL_COOKIE_SIZE+OVL_HASH_SIZE+1)
#define OVL_CONNECT_MIN_RESP_SIZE     OVL_CONNECT_MIN_REQU_SIZE
#define OVL_RENDEZVOUS_REQU_SIZE      (OVL_COOKIE_SIZE+OVL_HASH_SIZE+19)

/** Maximum unencrypted payload per message
 * (OVL_MAX_DATA_SIZE - 8 (sequence) - 16 (GCM-MAC) - 16 (AES 128 BLOCK MARGIN)). */
#define OVL_SEC_MAX_DATA_SIZE (OVL_MAX_DATA_SIZE-40)
/** The max. public key size for a START_STREAM message. */
#define OVL_MAX_PUBKEY_SIZE (OVL_MAX_MESSAGE_SIZE-OVL_COOKIE_SIZE-OVL_HASH_SIZE-1)

#endif // __OVL_DHT_CONFIG_HH__
