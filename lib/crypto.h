#ifndef IDENTITY_H
#define IDENTITY_H

#include <QObject>
#include <QIODevice>
#include <QTcpSocket>
#include <QAbstractSocket>
#include <QFile>

#include "dht.h"
#include "dht_config.h"

#include <openssl/evp.h>

/** Maximum unencrypted payload per message. */
#define DHT_SEC_MAX_DATA_SIZE (DHT_MAX_DATA_SIZE-20)


/** Represents the identity of a node. A node is unquely identified by its keypair. The private key
 * of that key is only known to the node. Its public key is used to verify its identity. */
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

  /** Returns the identifier of the identity (RMD160 of the public key). */
  const Identifier &id() const;
  /** Retruns @c true if the public key of the identity is present. */
  bool hasPublicKey() const;
  /** Retruns @c true if the private key of the identity is present. */
  bool hasPrivateKey() const;
  /** Copyies the public key in a binary format into the given buffer. */
  int publicKey(uint8_t *key, size_t len) const;

  /** Requires the private key. */
  int sign(const uint8_t *data, size_t datalen, uint8_t *sig, size_t siglen) const;
  /** Requires the public key. */
  bool verify(const uint8_t *data, size_t datalen, const uint8_t *sig, size_t siglen) const;

public:
  /** Creates a new identity and stores the key pair in the specified file. */
  static Identity *newIdentity(const QString &path);
  /** Loads the identity (key pair or public key) from the specified file. */
  static Identity *load(const QString &path);
  /** Constructs an Identity from the given public key (e.g. received via network). */
  static Identity *fromPublicKey(const uint8_t *key, size_t len);

protected:
  /** Key pair or public key. */
  EVP_PKEY *_keyPair;
  /** The fingerprint of the public key, the identifier of the node. */
  Identifier _fingerprint;
};


/** Represents a simple encrypted datagram socket between two nodes. */
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

protected:
  /** Needs to be implemented by any specialization to handle received datagrams. */
  virtual void handleDatagram(const uint8_t *data, size_t len) = 0;

  /** Sends the given @c data as an encrypted datagram. */
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
  bool start(const Identifier &streamId, const PeerItem &peer, QUdpSocket *socket);
  /** Encrypts the given data @c in using the sequential number @c seq and stores the
   * result in the ouput buffer @c out. */
  int encrypt(uint32_t seq, const uint8_t *in, size_t inlen, uint8_t *out);
  /** Decrypts the given data @c in using the sequential number @c seq and stores the
   * result in the output buffer @c out. */
  int decrypt(uint32_t seq, const uint8_t *in, size_t inlen, uint8_t *out);

protected:
  /** A weak reference to the DHT instance. */
  DHT &_dht;
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
  uint8_t _sharedIV[20];
  /** The current sequence number (bytes send). */
  uint32_t _outSeq;
  /** Buffer holding the decrypted message. */
  uint8_t _inBuffer[DHT_MAX_MESSAGE_SIZE];
  /** Identifier of the stream. */
  Identifier _streamId;
  /** The UDP socket. */
  QUdpSocket *_socket;
  // DHT may access some of the protected methods
  friend class DHT;
};


class SocketHandler
{
protected:
  SocketHandler();

public:
  virtual ~SocketHandler();

  virtual SecureSocket *newStream(uint16_t service) = 0;
  virtual bool allowStream(uint16_t service, const NodeItem &peer) = 0;
  virtual void streamStarted(SecureSocket *stream) = 0;
  virtual void streamFailed(SecureSocket *stream) = 0;
};



#endif // IDENTITY_H
