#ifndef IDENTITY_H
#define IDENTITY_H

#include <QObject>
#include <QFile>

#include "dht.h"
#include "dht_config.h"

#include <openssl/evp.h>


class Identity
{
protected:
  explicit Identity(EVP_PKEY *key, QObject *parent = 0);

public:
  virtual ~Identity();

  const Identifier &id() const;
  bool hasPublicKey() const;
  bool hasPrivateKey() const;
  int publicKey(uint8_t *key, size_t len) const;

  /** Requires the private key. */
  int sign(const uint8_t *data, size_t datalen, uint8_t *sig, size_t siglen) const;
  /** Requires the public key. */
  bool verify(const uint8_t *data, size_t datalen, const uint8_t *sig, size_t siglen) const;

public:
  static Identity *newIdentity(const QString &path);
  static Identity *load(const QString &path);
  static Identity *fromPublicKey(const uint8_t *key, size_t len);

protected:
  EVP_PKEY *_keyPair;
  Identifier _fingerprint;
};


class SecureStream
{
public:
  SecureStream(bool incomming, Identity &id);
  virtual ~SecureStream();

  /** The stream ID. */
  const Identifier &id() const;
  /** Peer identifier derived from its pubkey. */
  const Identifier &peerId() const;
  /** Returns @c true if the stream was initiated by the peer. */
  bool isIncomming() const;

  /** Needs to be implemented by any specialization to handle received datagrams. */
  virtual void handleDatagram(uint32_t seq, const uint8_t *data, size_t len) = 0;
  /** Sends the given @c data as an encrypted datagram. */
  bool sendDatagram(const uint8_t *data, size_t len);


protected:
  /** Processes (decrypt) an incomming datagram. */
  void handleData(const uint8_t *data, size_t len);

  /** Creates a session key pair and an initalization message. The message contains the public
   * key of the node, a newly generated ECC public key to derive a session key from and the
   * signature of the session key made from the nodes public key. This allows to verify the
   * identity of the node and to establish an encrypted communication.
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

  /** Verifies the peer from its initalization message (see @c prepare).
   * @param msg A pointer to the initialization message.
   * @param len The length of the initialization message.
   * @returns @c true if the peek could be verified and @c false if not or if an error ocurred.
   */
  bool verify(const uint8_t *msg, size_t len);

  /** Derives the session secret from the session keys & initializes the symmetric
   * encryption/decryption. */
  bool start(const Identifier &streamId, const PeerItem &peer, QUdpSocket *socket);
  int encrypt(uint32_t seq, const uint8_t *in, size_t inlen, uint8_t *out);
  int decrypt(uint32_t seq, const uint8_t *in, size_t inlen, uint8_t *out);

protected:
  bool _incomming;
  Identity &_identity;
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
  uint8_t _inBuffer[DHT_MAX_MESSAGE_SIZE];
  Identifier _streamId;
  QUdpSocket *_socket;

  friend class DHT;
};


class StreamHandler
{
protected:
  StreamHandler();

public:
  virtual ~StreamHandler();

  virtual SecureStream *newStream(bool incomming, uint16_t service) = 0;
  virtual bool allowStream(uint16_t service, const NodeItem &peer) = 0;
  virtual void streamStarted(SecureStream *stream) = 0;
};


#endif // IDENTITY_H
