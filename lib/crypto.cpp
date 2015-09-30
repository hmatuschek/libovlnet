#include "crypto.h"
#include "dht_config.h"
#include <QFile>
#include <QByteArray>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/ripemd.h>

#include "netinet/in.h"


Identity::Identity(EVP_PKEY *key, QObject *parent)
  : _keyPair(key)
{
  EVP_MD_CTX mdctx;
  unsigned char *keydata = 0; int keylen = 0;
  unsigned char fingerprint[20];

  // Get get public key in DER format (binary)
  if ( 0 >= (keylen = i2d_PublicKey(_keyPair, &keydata)) )
    goto error;

  // Compute RIPEMD160
  RIPEMD160(keydata, keylen, fingerprint);

  // Done
  _fingerprint = Identifier((char *)fingerprint);
  return;

error:
  ERR_load_crypto_strings();
  ERR_print_errors_fp(stderr);
  EVP_MD_CTX_cleanup(&mdctx);
}

Identity::~Identity() {
  EVP_PKEY_free(_keyPair);
  EVP_cleanup();
}

bool
Identity::hasPublicKey() const {
  return 0<i2d_PUBKEY(_keyPair,0);
}

int Identity::publicKey(uint8_t *key, size_t len) const {
  // Get pubkey
  int keylen = 0;
  if (0 > (keylen = i2d_PUBKEY(_keyPair, 0)) )
    return -1;
  // If just length is requested -> done
  if (0 == key) { return keylen; }
  // If not enough space is available -> error
  if (keylen > int(len)) { return -1; }
  // Store key
  if (0 > i2d_PUBKEY(_keyPair, &key))
    return -1;
  return keylen;
}

bool
Identity::hasPrivateKey() const {
  return 0<i2d_PrivateKey(_keyPair,0);
}

const Identifier &
Identity::id() const {
  return _fingerprint;
}

int
Identity::sign(const uint8_t *data, size_t datalen, uint8_t *sig, size_t siglen) const {
  if (! hasPrivateKey()) { return false; }

  EVP_MD_CTX mdctx;
  EVP_MD_CTX_init(&mdctx);
  size_t slen = 0;

  if (1 != EVP_DigestSignInit(&mdctx, 0, EVP_sha256(), 0, _keyPair))
    goto error;
  if (1 != EVP_DigestSignUpdate(&mdctx, data, datalen))
    goto error;
  if(1 != EVP_DigestSignFinal(&mdctx, 0, &slen))
    goto error;
  if (siglen < slen)
    goto error;
  if(1 != EVP_DigestSignFinal(&mdctx, sig, &slen))
    goto error;
  EVP_MD_CTX_cleanup(&mdctx);
  return slen;

error:
  ERR_load_crypto_strings();
  ERR_print_errors_fp(stderr);
  EVP_MD_CTX_cleanup(&mdctx);
  return -1;
}

bool
Identity::verify(const uint8_t *data, size_t datalen, const uint8_t *sig, size_t siglen) const {
  if (! hasPublicKey()) { return false; }

  EVP_MD_CTX mdctx;
  EVP_MD_CTX_init(&mdctx);

  if (1 != EVP_DigestVerifyInit(&mdctx, 0, EVP_sha256(), 0, _keyPair))
    goto error;
  if (1 != EVP_DigestVerifyUpdate(&mdctx, data, datalen))
    goto error;
  if (1 != EVP_DigestVerifyFinal(&mdctx, (uint8_t *)sig, siglen))
    goto error;

  EVP_MD_CTX_cleanup(&mdctx);
  return true;

error:
  EVP_MD_CTX_cleanup(&mdctx);
  return false;
}

Identity *
Identity::newIdentity(const QString &path)
{
  BIO *out = 0;
  EC_KEY *key = 0;
  EVP_PKEY *pkey = 0;

  // Allocage and generate key
  if (0 == (key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)))
    goto error;
  EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
  if (1 != EC_KEY_generate_key(key))
    goto error;

  // Store in EVP
  if (0 == (pkey = EVP_PKEY_new()))
    goto error;
  if (! EVP_PKEY_assign_EC_KEY(pkey, key))
    goto error;
  // Ownership of key has been taken by pkey
  key = 0;

  // Store keys in file
  if (0 == (out = BIO_new_file(path.toLocal8Bit(), "w")))
    goto error;
  if (!PEM_write_bio_PUBKEY(out, pkey))
    goto error;
  if (!PEM_write_bio_PrivateKey(out, pkey, NULL, NULL, 0, 0, NULL))
    goto error;
  (void) BIO_flush(out);
  BIO_free_all(out);

  // Senure proper file permissions:
  QFile::setPermissions(path, QFileDevice::ReadOwner|QFileDevice::WriteOwner);

  // Create identity instance with key pair
  return new Identity(pkey);

error:
  ERR_load_crypto_strings();
  ERR_print_errors_fp(stderr);
  if (key) { EC_KEY_free(key); }
  if (pkey) { EVP_PKEY_free(pkey); }
  if (out) { BIO_free_all(out); }
  return 0;
}

Identity *
Identity::load(const QString &path)
{
  BIO *bio = 0;
  EVP_PKEY *pkey = 0;
  // Read key from file
  if (0 == (bio = BIO_new_file(path.toLocal8Bit(), "r")))
    goto error;
  if (! (pkey = PEM_read_bio_PUBKEY(bio, 0, 0,0)))
    goto error;
  if ( (pkey = PEM_read_bio_PrivateKey(bio, &pkey, 0,0)) ) {
    qDebug() << "Read private key from" << path;
  }
  BIO_free_all(bio);

  // Crate identity instance with key pair.
  return new Identity(pkey);

error:
  ERR_load_crypto_strings();
  ERR_print_errors_fp(stderr);
  if (pkey) { EVP_PKEY_free(pkey); }
  if (bio) { BIO_free_all(bio); }
  return 0;
}

Identity *
Identity::fromPublicKey(const uint8_t *key, size_t len) {
  EVP_PKEY *pkey = EVP_PKEY_new();
  if (0 == d2i_PUBKEY(&pkey, &key, len)) {
    ERR_load_crypto_strings();
    ERR_print_errors_fp(stderr);
    return 0;
  }
  return new Identity(pkey);
}


/* ******************************************************************************************** *
 * Implementation of SecureSession
 * ******************************************************************************************** */
SecureStream::SecureStream(bool incomming, Identity &id)
  : _incomming(incomming), _identity(id), _sessionKeyPair(0), _peerPubKey(0),
    _streamId()
{
  // pass...
}

SecureStream::~SecureStream() {
  if (_sessionKeyPair) { EVP_PKEY_free(_sessionKeyPair); }
  if (_peerPubKey) { EVP_PKEY_free(_peerPubKey); }
}

int
SecureStream::prepare(uint8_t *msg, size_t len) {
  memset(msg, 0, len);
  uint8_t *keyPtr = 0; int keyLen =0;
  EC_KEY *key = 0;
  size_t stored=0;

  // Store public key and its length into output buffer
  if (0 > (keyLen = _identity.publicKey(msg+2, len-2)) )
    goto error;
  *((uint16_t *)msg) = htons(uint16_t(keyLen));
  stored += keyLen+2; msg += keyLen+2; len -= keyLen+2;

  // Generate session keys
  // Allocage and generate EC key
  if (0 == (key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1)))
    goto error;
  EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
  if (1 != EC_KEY_generate_key(key))
    goto error;
  // Store in EVP
  if (0 == (_sessionKeyPair = EVP_PKEY_new()))
    goto error;
  if (! EVP_PKEY_assign_EC_KEY(_sessionKeyPair, key))
    goto error;
  // store public key in output buffer
  if (0 > (keyLen = i2d_PUBKEY(_sessionKeyPair, 0)) )
    goto error;
  if (keyLen>(int(len)-2))
    goto error;
  *((uint16_t *)msg) = htons(uint16_t(keyLen));
  keyPtr = msg+2;
  if (0 > i2d_PUBKEY(_sessionKeyPair, &keyPtr) )
    goto error;
  // Save a pointer to the session public key
  keyPtr = msg+2;
  // update pointer
  stored += keyLen+2; msg += keyLen+2; len -= keyLen+2;

  // Sign session key
  if (0 > (keyLen = _identity.sign(keyPtr, keyLen, msg+2, len-2)))
    goto error;
  *((uint16_t *)msg) = htons(uint16_t(keyLen));
  stored += keyLen+2; msg += keyLen+2; len -= keyLen+2;
  return stored;

error:
  if (key && !_sessionKeyPair) { EC_KEY_free(key); }
  if (_sessionKeyPair) {
    EVP_PKEY_free(_sessionKeyPair);
    _sessionKeyPair = 0;
  }
  return -1;
}

const Identifier &
SecureStream::id() const {
  return _streamId;
}

const Identifier &
SecureStream::peerId() const {
  return _peerId;
}

bool
SecureStream::isIncomming() const {
  return _incomming;
}


bool
SecureStream::verify(const uint8_t *msg, size_t len)
{
  Identity *peer = 0;
  int keyLen=0, sigLen=0;
  const uint8_t *keyPtr=0;

  // Load peer public key
  // get length of key
  keyLen = ntohs(*(uint16_t *)msg);
  // read peer public key
  if (keyLen>(int(len)-2)) {
    goto error;
  }
  if (0 == (peer = Identity::fromPublicKey(msg+2, keyLen))) {
    goto error;
  }
  _peerId = peer->id();
  msg += keyLen+2; len -= keyLen+2;

  // read session public key
  keyLen = ntohs(*(uint16_t *)msg);
  if (keyLen>(int(len)-2))
    goto error;
  keyPtr = msg+2;
  if (0 == (_peerPubKey = d2i_PUBKEY(&_peerPubKey, &keyPtr, len-2)))
    goto error;
  // Restore ptr to public session key
  keyPtr = msg+2;
  msg += keyLen+2; len -= keyLen+2;

  // verify session key
  sigLen = ntohs(*(uint16_t *)msg);
  if (! peer->verify(keyPtr, keyLen, msg+2, sigLen))
    goto error;

  delete peer;
  return true;

error:
  if (peer) { delete peer; }
  if (_peerPubKey) { EVP_PKEY_free(_peerPubKey); }
  return false;
}

bool
SecureStream::start(const Identifier &streamId, const PeerItem &peer, QUdpSocket *socket) {
  // Check if everything is present
  if (! _peerPubKey) { return false; }
  if (! _sessionKeyPair) { return false; }

  EVP_PKEY_CTX *ctx = 0;
  size_t skeyLen = 0;
  uint8_t *skey = 0;
  uint8_t tmp[32];

  // Derive shared secret
  if (! (ctx = EVP_PKEY_CTX_new(_sessionKeyPair, 0)))
    goto error;
  if (0 >= EVP_PKEY_derive_init(ctx))
    goto error;
  if (0 >= EVP_PKEY_derive_set_peer(ctx, _peerPubKey))
    goto error;
  // Get length of shared secret
  if(0 >= EVP_PKEY_derive(ctx, 0, &skeyLen))
    goto error;
  if(! (skey = (uint8_t *) OPENSSL_malloc(skeyLen)) )
    goto error;
  // get it
  if(0 >= EVP_PKEY_derive(ctx, skey, &skeyLen))
    goto error;

  // Derive shared key and iv
  SHA256(skey, skeyLen, tmp);
  memcpy(_sharedKey, tmp, 16);
  memcpy(_sharedIV, tmp+16, 16);
  OPENSSL_cleanse(tmp, 32);

  EVP_PKEY_CTX_free(ctx);
  OPENSSL_free(skey);

  // Set seq to 0
  _outSeq = 0;
  // Store peer
  _peer = peer;
  // Store stream id and socket
  _streamId = streamId; _socket = socket;
  return true;

error:
  if (ctx) { EVP_PKEY_CTX_free(ctx); }
  if (skey) { OPENSSL_free(skey); }
  return false;
}

int
SecureStream::encrypt(uint32_t seq, const uint8_t *in, size_t inlen, uint8_t *out)
{
  // Check arguments
  if ((!in) || (!out)) { return -1; }

  // Derive IV from shared IV and sq
  uint8_t iv[32];
  int len1=0, len2=0;
  // Append seq to shared IV
  *((uint32_t *)(_sharedIV+16)) = seq;
  // Compute IV from shared IV + 4bytes seq number
  SHA256(_sharedIV, 20, iv);
  // Init encryption
  EVP_CIPHER_CTX ctx;
  EVP_CIPHER_CTX_init(&ctx);
  if (! EVP_EncryptInit(&ctx, EVP_aes_128_cbc(), _sharedKey, iv))
    goto error;
  // go
  if (! EVP_EncryptUpdate(&ctx, out, &len1, in, inlen))
    goto error;
  if (! EVP_EncryptFinal(&ctx, out+len1, &len2))
    goto error;
  // done
  EVP_CIPHER_CTX_cleanup(&ctx);
  return len1+len2;

error:
  EVP_CIPHER_CTX_cleanup(&ctx);
  return -1;
}

int
SecureStream::decrypt(uint32_t seq, const uint8_t *in, size_t inlen, uint8_t *out)
{
  // Check arguments
  if ((!in) || (!out)) { return -1; }

  // Derive IV from shared IV and sq
  uint8_t iv[32];
  int len1=0, len2=0;
  // Append seq to shared IV
  *((uint32_t *)(_sharedIV+16)) = seq;
  SHA256(_sharedIV, 20, iv);
  // Init encryption
  EVP_CIPHER_CTX ctx;
  EVP_CIPHER_CTX_init(&ctx);
  if (! EVP_DecryptInit(&ctx, EVP_aes_128_cbc(), _sharedKey, iv))
    goto error;
  // go
  if (! EVP_DecryptUpdate(&ctx, out, &len1, in, inlen))
    goto error;
  if (! EVP_DecryptFinal(&ctx, out+len1, &len2))
    goto error;
  // done
  EVP_CIPHER_CTX_cleanup(&ctx);
  return len1+len2;

error:
  EVP_CIPHER_CTX_cleanup(&ctx);
  return -1;
}

void
SecureStream::handleData(const uint8_t *data, size_t len) {
  if (0 == len) {
    this->handleDatagram(0, 0, 0); return;
  } else if (len<4) {
    // A valid encrypted message needs at least 4 bytes (the sequence int32_t).
    return;
  }
  // Get sequence number
  uint32_t seq = ntohl(*((uint32_t *)data)); data +=4;
  int rxlen = 0;
  // Decrypt message
  if (0 > (rxlen = decrypt(seq, data, len-4, _inBuffer))) {
    qDebug() << "Failed to decrypt message" << seq;
    return;
  }
  this->handleDatagram(seq, _inBuffer, rxlen);
}

bool
SecureStream::sendDatagram(const uint8_t *data, size_t len) {
  uint8_t msg[DHT_MAX_MESSAGE_SIZE];
  uint8_t *ptr = msg;
  int txlen = 0, enclen=0;

  // Store stream cookie
  memcpy(ptr, _streamId.data(), DHT_HASH_SIZE);
  ptr += DHT_HASH_SIZE; txlen += DHT_HASH_SIZE;
  // store sequence number
  *((uint32_t *)ptr) = htonl(_outSeq); txlen += 4; ptr += 4;
  // store encrypted data if there is any
  if ( (len > 0) && (0 > (enclen = encrypt(_outSeq, data, len, ptr))) )
    return false;
  txlen += enclen;
  // Send datagram
  if (txlen != _socket->writeDatagram((char *)msg, txlen, _peer.addr(), _peer.port()))
    return false;
  // Update seq
  _outSeq += len;
  return true;
}

bool
SecureStream::sendNull() {
  // send only stream id
  return DHT_HASH_SIZE == _socket->writeDatagram((const char *)_streamId.data(), DHT_HASH_SIZE,
                                                 _peer.addr(), _peer.port());
}




/* ******************************************************************************************** *
 * Implementation of StreamHandler
 * ******************************************************************************************** */
StreamHandler::StreamHandler()
{
  // pass...
}

StreamHandler::~StreamHandler() {
  // pass...
}


