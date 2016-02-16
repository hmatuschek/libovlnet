#ifndef IDENTITY_H
#define IDENTITY_H

#include <QObject>
#include <QIODevice>
#include <QUdpSocket>
#include <QAbstractSocket>
#include <QFile>

#include "buckets.hh"
#include "dht_config.hh"

#include <openssl/evp.h>

// Forward decl.
class DHT;


/** Maximum unencrypted payload per message
 * (DHT_MAX_DATA_SIZE - 8 (sequence) - 16 (GCM-MAC) - 16 (AES 128 BLOCK MARGIN)). */
#define DHT_SEC_MAX_DATA_SIZE (DHT_MAX_DATA_SIZE-40)
/** The max. public key size for a START_STREAM message. */
#define DHT_MAX_PUBKEY_SIZE (DHT_MAX_MESSAGE_SIZE-DHT_HASH_SIZE-3)


/** Represents the identity of a node. A node is unquely identified by its keypair. The private key
 * of that key is only known to the node. Its public key is used to verify its identity. The hash
 * of the public key is the identifier of the node.
 * @ingroup core */
class Identity
{
protected:
  /** "Hidden" constructor.
   * @param key Key pair or only the public key of a node.
   * @param parent The optional parent of the QObject. */
  explicit Identity(EVP_PKEY *key, QObject *parent = 0);

public:
  /** Destructor. */
  virtual ~Identity();

  /** Returns the identifier of the identity (RMD160 hash of the public key). */
  const Identifier &id() const;
  /** Returns @c true if the public key of the identity is present. */
  bool hasPublicKey() const;
  /** Returns @c true if the private key of the identity is present. */
  bool hasPrivateKey() const;
  /** Copyies the public key in a binary format into the given buffer. */
  int publicKey(uint8_t *key, size_t len) const;

  /** Signs the given data and stores the signature in @c sig. Requires the private key. */
  int sign(const uint8_t *data, size_t datalen, uint8_t *sig, size_t siglen) const;
  /** Verifies the signature @c sig of the given @c data. Requires the public key. */
  bool verify(const uint8_t *data, size_t datalen, const uint8_t *sig, size_t siglen) const;

public:
  /** Creates a new identity. */
  static Identity *newIdentity();
  /** Loads the identity (key pair or public key) from the specified file. */
  static Identity *load(const QString &path);
  /** Constructs an Identity from the given public key (e.g. received via network). */
  static Identity *fromPublicKey(const uint8_t *key, size_t len);
  /** Saves the identity in the given file. */
  bool save(const QString &path) const;

protected:
  /** Key pair or public key. */
  EVP_PKEY *_keyPair;
  /** The fingerprint of the public key, the identifier of the node. */
  Identifier _fingerprint;
};



/** Represents a simple encrypted datagram socket between two nodes.
 * Although being secure by means of encryption and authentication, it is not reliable. A message
 * or datagram may get lost during the transmission without notice. For a reliable transport,
 * consider @c SecureStream.
 *
 * The shared secret for the connection will be derived from a ECDH handshake, where a fresh ECDH
 * keypair is used for every connection. The SHA-2-256 hash is used as the key-derivative function
 * to obtain the 128bit shared symmetric key and the first 64bit of the IV. The second half of the
 * IV (64bit) is a sequence nuber send along with the encrypted datagrams. The symmetric cipher is
 * AES-128 in GCM mode.
 * @ingroup core */
class SecureSocket
{
public:
  /** Constructor. */
  SecureSocket(DHT &dht);
  /** Destructor. */
  virtual ~SecureSocket();

  /** The stream ID. */
  const Identifier &id() const;
  /** Peer identifier derived from its pubkey. */
  const Identifier &peerId() const;
  /** Returns remote peer. */
  const PeerItem &peer() const;

protected:
  /** Needs to be implemented by any specialization to handle received datagrams. */
  virtual void handleDatagram(const uint8_t *data, size_t len) = 0;

  /** Sends the given @c data as an encrypted datagram.
   * Datagram format:
   * \code
   * struct {
   *   // 64bit big-endian sequence number, forms the second half of the IV used to encrypt the
   *   // datagram. The initial tag is chosed randomly and will be incremented by the size of the
   *   // encrypted payload.
   *   uint64_t sequence;
   *   // 128bit Message authentication tag
   *   uint8_t  mac_tag[16];
   *   // Encrypted message
   *   uint8_t  data[*];
   * };
   **/
  bool sendDatagram(const uint8_t *data, size_t len);

  /** Sends a null datagram. */
  bool sendNull();

  /** Processes (decrypt) an incomming datagram. */
  void handleData(const uint8_t *data, size_t len);

  /** Creates a session key pair and an initalization message. The message contains the public
   * key of the node, a newly generated ECC public key to derive a session key and the
   * signature of the session key made with the public key of the node. This allows to prevent
   * man-in-the-middle attacks on the secure communication. This does not ensure that the sender
   * of a valid handshake package is actually the verified node.
   *
   * \code
   * struct {
   *   uint16_t pubkeyLen;         // length of identity pubkey, network order
   *   char     pubkey[pubkeyLen]; // the identity pubkey
   *   uint16_t sesKeyLen;         // length of session pubkey, network order
   *   char     sesKey[sesKeyLen]; // the session pubkey
   *   uint16_t sigLen;            // length of the signature
   *   char     sig[sigLen];       // signature of the session key
   * };
   * \endcode
   *
   * @param msg A pointer to a buffer, the initialization message will be written to.
   * @param maxlen Specifies the size of the buffer.
   * @returns The length of the initialization message or -1 on error.
   */
  int prepare(uint8_t *msg, size_t maxlen);

  /** Verifies the initialization message (see @c prepare).
   * @param msg A pointer to the initialization message.
   * @param len The length of the initialization message.
   * @returns @c true if the peek could be verified and @c false if not or if an error ocurred.
   */
  bool verify(const uint8_t *msg, size_t len);

  /** Derives the session secret from the session keys & initializes the symmetric
   * encryption/decryption. */
  virtual bool start(const Identifier &streamId, const PeerItem &peer);
  /** Signals that the connection failed. */
  virtual void failed();
  /** Encrypts the given data @c in using the sequential number @c seq and stores the
   * result in the ouput buffer @c out. */
  int encrypt(uint64_t seq, const uint8_t *in, size_t inlen, uint8_t *out, uint8_t *tag);
  /** Decrypts the given data @c in using the sequential number @c seq and stores the
   * result in the output buffer @c out. */
  int decrypt(uint64_t seq, const uint8_t *in, size_t inlen, uint8_t *out, const uint8_t *tag);

protected:
  /** A weak reference to the DHT instance. */
  DHT &_dht;

private:
  /** The ECDH key pair of this node for the session. */
  EVP_PKEY *_sessionKeyPair;
  /** Public session key provided by the peer. */
  EVP_PKEY *_peerPubKey;
  /** Identifier of the peer key. */
  Identifier _peerId;
  /** Peer address and port. */
  PeerItem _peer;
  /** The shared key. */
  uint8_t _sharedKey[16];
  /** The shared IV. */
  uint8_t _sharedIV[16];
  /** The current sequence number (bytes send). */
  uint64_t _outSeq;
  /** Buffer holding the decrypted message. */
  uint8_t _inBuffer[DHT_MAX_DATA_SIZE];
  /** Identifier of the stream. */
  Identifier _streamId;

  // DHT may access some of the protected methods
  friend class DHT;
};


/** Interface of a service handler.
 * Such a handler acts as a gate keeper and dispatcher for incomming and established
 * secure connections (@c SecureSocket).
 *
 * On an incomming connection, first @c newSocket gets called. This method should create the
 * matching @c SecureSocket instance for the given service. Then the @c DHT instance will initiate
 * a secure connection. In that step, the identity of the peer will be verified. Once the connection
 * is initiated, @c allowConnection will be called. If it returns @c true, the connection is
 * considered as established and @c connectionStarted gets called passing the ownership of the
 * socket. If @c allowConnection fails (returns @c false) or the identity of the node can not be
 * verified, @c connectionFailed gets called also passing the ownership of the socket.
 *
 * On an outgoing connection, e.g. by calling @c DHT::startStream, the DHT will try to establish
 * a secure connection. On success, @c connectionStarted will be called and @c connectionFailed
 * on error. Again, both methods transfer the ownership of the socket.
 * @ingroup core */
class ServiceHandler
{
protected:
  /** Hidden constructor. */
  ServiceHandler();

public:
  /** Destructor. */
  virtual ~ServiceHandler();

  /** Needs to be implemented to construct a socket for the incomming connection to the given
   * service. */
  virtual SecureSocket *newSocket(uint16_t service) = 0;
  /** Needs to be implemented to allow or deny connections from the given peer to the given
   * service. */
  virtual bool allowConnection(uint16_t service, const NodeItem &peer) = 0;
  /** Gets called if a connection is established. */
  virtual void connectionStarted(SecureSocket *stream) = 0;
  /** Gets called if a connection failed. */
  virtual void connectionFailed(SecureSocket *stream) = 0;
};


/** Interface of a service provided via the OVL network.
 * Such a handler acts as a gate keeper and dispatcher for incomming and established
 * secure connections (@c SecureSocket).
 *
 * On an incomming connection, first @c newSocket gets called. This method should create the
 * matching @c SecureSocket instance for the service. Then the @c DHT instance will initiate
 * a secure connection. In that step, the identity of the peer will be verified. Once the connection
 * is initiated, @c allowConnection will be called. If it returns @c true, the connection is
 * considered as established and @c connectionStarted gets called passing the ownership of the
 * socket. If @c allowConnection fails (returns @c false) or the identity of the node can not be
 * verified, @c connectionFailed gets called also passing the ownership of the socket.
 *
 * On an outgoing connection, e.g. by calling @c DHT::startConnection, the DHT will try to establish
 * a secure connection. On success, @c connectionStarted will be called and @c connectionFailed
 * on error. Again, both methods transfer the ownership of the socket.
 * @ingroup core */
class AbstractService
{
protected:
  /** Hidden constructor. */
  AbstractService();

public:
  /** Destructor. */
  virtual ~AbstractService();

  /** Needs to be implemented to construct a socket for the incomming connection. */
  virtual SecureSocket *newSocket() = 0;
  /** Needs to be implemented to allow or deny connections from the given peer. */
  virtual bool allowConnection(const NodeItem &peer) = 0;
  /** Gets called if a connection is established. */
  virtual void connectionStarted(SecureSocket *stream) = 0;
  /** Gets called if a connection failed. */
  virtual void connectionFailed(SecureSocket *stream) = 0;
};


#endif // IDENTITY_H
