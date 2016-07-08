/** @mainpage libovlnet An overlay network library based on Qt5
 *
 * Libovlnet at its core, implements a <a href="https://en.wikipedia.org/wiki/Kademlia">kademlia</a>
 * like <a href="https://en.wikipedia.org/wiki/Distributed_hash_table">distributed hash table</a>
 * (DHT class). The objective of such a network is to resolve identifiers -- addressing some data or a
 * node in the network -- efficiently. Hence every node participating in such a network is uniquely
 * identified by a fingerprint. For the OVL network, this fingerprint is the
 * <a href="https://en.wikipedia.org/wiki/RIPEMD">RIPEMD160</a> hash of a 256 bit
 * <a href="https://en.wikipedia.org/wiki/Elliptic_Curve_Digital_Signature_Algorithm">ECDSA</a>
 * public key (using the ANSI X9.62 Prime 256v1 curve). This
 * <a href="https://en.wikipedia.org/wiki/Public-key_cryptography">key pair</a> acts as the
 * Identity of the node, irrespective of the current IP address assigned to the node. Resolving an
 * Identifier to a IP addresse and port number is the main objective of the DHT used in the OVL
 * network.
 *
 * Beside resolving identifiers (the DHT), a OVL network node provides a
 * <a href="https://en.wikipedia.org/wiki/Hole_punching_%28networking%29">rendezvous service</a>
 * to assist establishing connections to hosts behind
 * <a href="https://en.wikipedia.org/wiki/Network_address_translation">NATs</a>. A OVL network node
 * may also provide services. Libovlnet implements basic means to establish an encrypted and
 * athenticated peer-to-peer connection, either
 * <a href="https://en.wikipedia.org/wiki/Datagram">datagram</a> based (like UDP) using the
 * SecureSocket class or stream based (TCP like) using the SecureStream class. Additionally some
 * typical services are already implemented like SecureChat, SecureCall, FileUpload, FileDownload,
 * and SocksOutStream. In contrast to the DHT routing, these services are usually not accessible by
 * every node participating in the network, but only to some specific nodes (contacts or "friends").
 * Hence, concening the services OVL is a
 * <a href="https://en.wikipedia.org/wiki/Friend-to-friend">friend-to-friend</a> network while the
 * DHT routing forms a public <a href="https://en.wikipedia.org/wiki/Peer-to-peer">peer-to-peer</a>
 * network.
 *
 * \section routing Routing
 * Resolving an Identifier to an IP addresse and port is an essential part of the OVL network
 * capability. This is achieved by two remote procdure calls (RPC) implemented by all nodes
 * participaing in the network and a routing table (called buckets) maintained by all nodes.
 * The routing table is just a table mapping an Identifier to an address/port tuple. To keep
 * routing tables compact, every node will maintain detailed knowledge about its neighbourhood
 * (w.r.t. a distance metric on the Identifier, here XOR metric). Meaning the closer another node
 * is, the more likely it will be kept in the routing table.
 *
 * The first RPC is called PING and the second FIND_NODE. The PING RPC is used to keep the routing
 * table up-to-date and to detect when nodes have left the network. The FIND_NODE RPC is used to
 * actually resolve an Identifier and to discover new nodes in the network.
 *
 * To resolve an Identifier ID, the node first collects the nodes from its own routing table that
 * are closest to ID. Then, it will send a FIND_NODE request to these nodes. As a response, these
 * nodes will send those entries (triples identifier, address and port) from their routing tables
 * which are closest to the requested identifier ID. The returned triples are used to populate the
 * nodes own routing table. If the requested identifier was returned by one of the nodes, the
 * search is complete. If not, the search will be continued with the nodes closest to the
 * requested ID. If no progress can be made, i.e. no new node was discovered that is closer to the
 * requested ID, the search failed.
 *
 * The routing table not only maps identifier to addresses and port but also keeps a timestamp when
 * of the last time, the node was reachable. If this timestamp gets older than a certain age
 * (about 20 min), the entry will be removed from the routing table. To maintain the routing table,
 * the node will send a PING request to the nodes in the table regularily (about every 15 minutes).
 * If the pinged node anwers, its timestamp in the routing table gets updated. If not, the node
 * will timeout later and will be removed from the routing table.
 *
 * Nodes dicovered by FIND_NODE requests are added to the routing table as "gossip" or "hear-say"
 * with an invalid timestamp. Hence they will be pinged and removed from the routing table on the
 * next refresh of the table. If they reply to the PING request, they are added to the routing
 * table again with a proper timestamp. If not, e.g. they are not directly reachable or left the
 * network, they are not longer kept in the routing table. Note: OVL network nodes do not spread
 * gossip. Outdated or "gossip" entries in the routing table are not included into FIND_NODE
 * responses.
 *
 * \section nat NAT hole-punching
 * Usually, you are not connected directly to the internet but are rather a member of a local
 * private network and access the internet through a router that implements network address
 * translation (NAT). Consequently, a OVL network node behind a NAT will not be reachable by other
 * nodes in the network. This has two consequences. First, a node that attempts to access a service
 * at the NATed node will not be able to connect and the NATed node will not appear in the routing
 * tables of the network and a node attempting to connect to a NATed node will not be able to
 * resolve its identifier. To solve the second issue, the NATed node needs to ping its K (usually
 * K=8) neighbours at a rather heigh frequency (usually 5 seconds). This basically performs a
 * so-called <a href="https://en.wikipedia.org/wiki/UDP_hole_punching">UDP NAT hole-punching</a>.
 * These "holes", however, only point towards the K neighbours of the NATed node. This still
 * prevents an arbitrary node to access the NATed one directly. To enable arbitrary nodes to
 * connect directly, every OVL node provides a rendezvous service.
 *
 * Assume two nodes A & B are behind NATs and a public reachable node C is in the neighbourhood of
 * A. Now B wants to connect to A but it is not reachable diectly. As C is in the neighbourhood of
 * A, C is likely able (through UDP hole-punching by A towards C) to access A. In a first step, B
 * searches for the neighbourhood of A using the FIND_NODE requests described above. With high
 * probability, B will find C and learn about the public address and port of A. B will then send a
 * PING to A. This ping will fail if A is behind a NAT w/o static address translation. This ping,
 * however, punched a hole in the NAT of B pointing towards A. Then it will send a RENDEZVOUS
 * request to C and C will forward the request to A including the public address and port of B. A
 * will then send a PING to B which succeeds if neither A's nor B's NAT is symmetric. Now, also A
 * has "punched" a hole in its NAT pointing towards B and both parties can communicate directly.
 * This scheme works for full-cone, address-restricted-cone and (partially) port-restricted-cone
 * NATs and thus for the majority of cases. For symmetric NATs -- the most nasty variant of a NAT
 * -- this scheme does not work. In these cases, you may need to reconfigure your router if
 * possible or use one of the provided NAT traversal techniques (NAT-PMP or PCP) to request a
 * temporary static port mapping from your router (not all routers support these features).
 *
 * \section services Services
 * Libovlnet provides means to establish encrypted and authenticated connections between nodes.
 * These connections can then be used to provide different services to other nodes. The following
 * services are provided by Libovlnet:
 *
 *   - A simple chat service (@ref chat).
 *   - A simple VoIP/voice call service (@ref voip).
 *   - File transfer (@ref filetransfer).
 *   - SOCKS proxy service (@ref socks).
 *   - A secure shell service (@ref rshell).
 *   - A HTTP server (@ref http).
 */

/** @defgroup services Services */

/** @defgroup internal Internal used classes. */

#ifndef __OVLNET_H__
#define __OVLNET_H__

#include "dht_config.hh"
#include "node.hh"
#include "subnetwork.hh"
#include "crypto.hh"
#include "stream.hh"

#include "securechat.hh"
#include "securecall.hh"
#include "filetransfer.hh"
#include "socks.hh"
#include "httpservice.hh"

#include "logger.hh"
#include "ntp.hh"
#include "pcp.hh"
#include "natpmp.hh"


#endif // __OVLNET_H__

