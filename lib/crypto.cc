#include "crypto.h"

#include "dht.h"
#include "dht_config.h"
#include "utils.h"

#include <QFile>
#include <QByteArray>
#include <QtEndian>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <openssl/ripemd.h>



/* ******************************************************************************************** *
 * Implementation of Identity
 * ******************************************************************************************** */
Identity::Identity(EVP_PKEY *key, QObject *parent)
  : _keyPair(key)
{
  EVP_MD_CTX mdctx;
  unsigned char *keydata = 0; int keylen = 0;
  unsigned char fingerprint[20];

  // Get get public key in DER format (binary)
  if ( 0 >= (keylen = i2d_PUBKEY(_keyPair, &keydata)) )
    goto error;

  // Compute RIPEMD160
  RIPEMD160(keydata, keylen, fingerprint);

  // Done
  _fingerprint = Identifier((char *)fingerprint);
  return;

error:
  ERR_load_crypto_strings();
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
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
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
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
  ERR_load_crypto_strings();
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
  EVP_MD_CTX_cleanup(&mdctx);
  return false;
}

Identity *
Identity::newIdentity()
{
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

  // Create identity instance with key pair
  return new Identity(pkey);

error:
  ERR_load_crypto_strings();
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
  if (key) { EC_KEY_free(key); }
  if (pkey) { EVP_PKEY_free(pkey); }
  return 0;
}


bool
Identity::save(const QString &path) const {
  BIO *out = 0;

  // Store keys in file
  if (0 == (out = BIO_new_file(path.toLocal8Bit(), "w"))) {
    logError() << "Identity: Cannot open file " << path;
    goto error;
  }

  if (hasPublicKey()) {
    if (! PEM_write_bio_PUBKEY(out, _keyPair)) {
      logError() << "Identity: Cannot write public key to file " << path;
      goto error;
    }
  }

  if (hasPrivateKey()) {
    if (!PEM_write_bio_PrivateKey(out, _keyPair, NULL, NULL, 0, 0, NULL)) {
      logError() << "Identity: Cannot write private key to file " << path;
      goto error;
    }
  }

  (void) BIO_flush(out);
  BIO_free_all(out);

  // Senure proper file permissions:
  if (! QFile::setPermissions(path, QFileDevice::ReadOwner|QFileDevice::WriteOwner)) {
    logWarning() << "Identity: Can not set permissions of file " << path << ".";
  }
  return true;

error:
  if (out) { BIO_free_all(out); }
  return false;
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
    logDebug() << "Read private key from" << path;
  }
  BIO_free_all(bio);

  // Crate identity instance with key pair.
  return new Identity(pkey);

error:
  ERR_load_crypto_strings();
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
  if (pkey) { EVP_PKEY_free(pkey); }
  if (bio) { BIO_free_all(bio); }
  return 0;
}

Identity *
Identity::fromPublicKey(const uint8_t *key, size_t len) {
  EVP_PKEY *pkey = EVP_PKEY_new();
  if (0 == d2i_PUBKEY(&pkey, &key, len)) {
    ERR_load_crypto_strings();
    unsigned long e = 0;
    while ( 0 != (e = ERR_get_error()) ) {
      logError() << "OpenSSL: " << ERR_error_string(e, 0);
    }
    return 0;
  }
  return new Identity(pkey);
}



/* ******************************************************************************************** *
 * Implementation of SecureSocket
 * ******************************************************************************************** */
SecureSocket::SecureSocket(DHT &dht)
  : _dht(dht), _sessionKeyPair(0), _peerPubKey(0), _streamId(Identifier::create())
{
  // pass...
}

SecureSocket::~SecureSocket() {
  if (_sessionKeyPair) { EVP_PKEY_free(_sessionKeyPair); }
  if (_peerPubKey) { EVP_PKEY_free(_peerPubKey); }
  _dht.socketClosed(_streamId);
}

int
SecureSocket::prepare(uint8_t *msg, size_t len) {
  memset(msg, 0, len);
  uint8_t *keyPtr = 0; int keyLen =0;
  EC_KEY *key = 0;
  size_t stored=0;

  // Store public key and its length into output buffer
  if (0 > (keyLen = _dht.identity().publicKey(msg+2, len-2)) )
    goto error;
  *((uint16_t *)msg) = qToBigEndian(qint16(keyLen));
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
  *((uint16_t *)msg) = qToBigEndian(qint16(keyLen));
  keyPtr = msg+2;
  if (0 > i2d_PUBKEY(_sessionKeyPair, &keyPtr) )
    goto error;
  // Save a pointer to the session public key
  keyPtr = msg+2;
  // update pointer
  stored += keyLen+2; msg += keyLen+2; len -= keyLen+2;

  // Sign session key
  if (0 > (keyLen = _dht.identity().sign(keyPtr, keyLen, msg+2, len-2)))
    goto error;
  *((uint16_t *)msg) = qToBigEndian(qint16(keyLen));
  stored += keyLen+2; msg += keyLen+2; len -= keyLen+2;
  return stored;

error:
  ERR_load_crypto_strings();
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
  if (key && !_sessionKeyPair) { EC_KEY_free(key); }
  if (_sessionKeyPair) {
    EVP_PKEY_free(_sessionKeyPair);
    _sessionKeyPair = 0;
  }
  return -1;
}

const Identifier &
SecureSocket::id() const {
  return _streamId;
}

const Identifier &
SecureSocket::peerId() const {
  return _peerId;
}

const PeerItem &
SecureSocket::peer() const {
  return _peer;
}

bool
SecureSocket::verify(const uint8_t *msg, size_t len)
{
  Identity *peer = 0;
  int keyLen=0, sigLen=0;
  const uint8_t *keyPtr=0;

  // Load peer public key
  // get length of key
  keyLen = qFromBigEndian(*(qint16 *)msg);
  // check length
  if (keyLen>(int(len)-2)) { goto error; }
  // read peer public key
  if (0 == (peer = Identity::fromPublicKey(msg+2, keyLen))) {
    goto error;
  }
  // get peer ID as fingerprint of its pubkey
  _peerId = peer->id();
  // update pointer & length
  msg += keyLen+2; len -= keyLen+2;

  // read session public key
  keyLen = qFromBigEndian(*(qint16 *)msg);
  if (keyLen>(int(len)-2)) { goto error; }
  keyPtr = msg+2;
  if (0 == (_peerPubKey = d2i_PUBKEY(&_peerPubKey, &keyPtr, len-2)))
    goto error;
  // Restore ptr to public session key
  keyPtr = msg+2;
  msg += keyLen+2; len -= keyLen+2;

  // verify session key
  sigLen = qFromBigEndian(*(qint16 *)msg);
  if (sigLen>(int(len)-2)) { goto error; }
  if (! peer->verify(keyPtr, keyLen, msg+2, sigLen))
    goto error;

  delete peer;
  return true;

error:
  ERR_load_crypto_strings();
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
  if (peer) { delete peer; }
  if (_peerPubKey) { EVP_PKEY_free(_peerPubKey); }
  return false;
}

bool
SecureSocket::start(const Identifier &streamId, const PeerItem &peer) {
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

  // Set seq to random value
  _outSeq = dht_rand64();
  // Store peer
  _peer = peer;
  // Store stream id and socket
  _streamId = streamId;
  return true;

error:
  ERR_load_crypto_strings();
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
  if (ctx) { EVP_PKEY_CTX_free(ctx); }
  if (skey) { OPENSSL_free(skey); }
  return false;
}

void
SecureSocket::failed() {
  // pass...
}

int
SecureSocket::encrypt(uint64_t seq, const uint8_t *in, size_t inlen, uint8_t *out, uint8_t *tag)
{
  // Check arguments
  if ((!in) || (!out)) { return -1; }

  int len1=0, len2=0;
  // "derive IV"
  uint8_t iv[16]; memcpy(iv, _sharedIV, 8);
  // Append seq number (in big endian) to shared IV (first 8bytes)
  *((uint64_t *)(iv+8)) = qToBigEndian(qint64(seq));
  // Init encryption
  EVP_CIPHER_CTX ctx;
  EVP_CIPHER_CTX_init(&ctx);
  // Set mode: AES 128bit GCM
  if (1 != EVP_EncryptInit_ex(&ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
    goto error;
  // set IV length to 16 (8byte IV derived from DH + 8byte counter)
  if (1 != EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL))
    goto error;
  // set key & IV
  if (1 != EVP_EncryptInit_ex(&ctx, NULL, NULL, _sharedKey, iv))
    goto error;
  // go
  if (1 != EVP_EncryptUpdate(&ctx, out, &len1, in, inlen))
    goto error;
  // finalize
  if (1 != EVP_EncryptFinal(&ctx, out+len1, &len2))
    goto error;
  // get MAC tag
  if(1 != EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_GET_TAG, 16, (void *)tag))
    goto error;
  // done
  EVP_CIPHER_CTX_cleanup(&ctx);
  return len1+len2;

error:
  if (tag) {
    logDebug() << "SecureSocket::encrypt(): TAG="
               << QByteArray((const char *)tag, 16).toHex();
  }
  ERR_load_crypto_strings();
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
  EVP_CIPHER_CTX_cleanup(&ctx);
  return -1;
}

int
SecureSocket::decrypt(uint64_t seq, const uint8_t *in, size_t inlen, uint8_t *out, const uint8_t *tag)
{
  // Check arguments
  if ((!in) || (!out)) { return -1; }
  int len1=DHT_MAX_DATA_SIZE, len2=0;
  // "derive IV"
  uint8_t iv[16]; memcpy(iv, _sharedIV, 8);
  // Append seq to shared IV (first 8bytes)
  *((uint64_t *)(iv+8)) = qToBigEndian(qint64(seq));
  // Init encryption
  EVP_CIPHER_CTX ctx;
  EVP_CIPHER_CTX_init(&ctx);
  // Init mode: 128bit AES in GCM
  if (1 != EVP_DecryptInit_ex(&ctx, EVP_aes_128_gcm(), NULL, NULL, NULL))
    goto error;
  // set IV len (16 = 8 shared IV + 8 counter)
  if (1 != EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL))
    goto error;
  // set key & IV
  if (1 != EVP_DecryptInit_ex(&ctx, NULL, NULL, _sharedKey, iv))
    goto error;
  // go
  if (1 != EVP_DecryptUpdate(&ctx, out, &len1, in, inlen))
    goto error;
  // Set MAC tag
  if (1 != EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_TAG, 16, (void *)tag))
    goto error;
  // Finalize decryption and verify tag
  if (0 >= EVP_DecryptFinal(&ctx, out+len1, &len2))
    goto error;
  // done
  EVP_CIPHER_CTX_cleanup(&ctx);
  return len1+len2;

error:
  if (tag) {
    logDebug() << "SecureSocket::decrypt(): TAG="
               << QByteArray((const char *)tag, 16).toHex();
  }
  ERR_load_crypto_strings();
  unsigned long e = 0;
  while ( 0 != (e = ERR_get_error()) ) {
    logError() << "OpenSSL: " << ERR_error_string(e, 0);
  }
  EVP_CIPHER_CTX_cleanup(&ctx);
  return -1;
}

void
SecureSocket::handleData(const uint8_t *data, size_t len) {
  if (0 == len) {
    // process null datagram
    this->handleDatagram(0, 0); return;
  } else if (len<24) {
    // A valid encrypted message needs at least 24 bytes (64bit seq + 128bit tag).
    return;
  }
  uint8_t inBuffer[DHT_MAX_DATA_SIZE];
  // Get sequence number
  qint64 seq = qFromBigEndian(*((quint64 *)data)); data +=8;
  // Get MAC tag
  const uint8_t *tag = data; data += 16;
  // Decrypt message, store result in _inBuffer
  int rxlen = 0;
  if (0 > (rxlen = decrypt(seq, data, len-24, inBuffer, tag))) {
    logDebug() << "Failed to decrypt message " << seq;
    return;
  }
  /// @bug That is somewhat late! However, sizeof(_inBuffer)==DHT_MAX_MESSAGE_SIZE
  ///      and the decrypted message cannot be larger than that. Hence it is
  ///      ensured that there is no buffer overrun.
  if (rxlen > DHT_SEC_MAX_DATA_SIZE) {
    logError() << "Fatal: Decrypted data larger than MAX_SEC_DATA_SIZE!"
               << " LEN=" << rxlen << ">" << DHT_SEC_MAX_DATA_SIZE;
  }
  // Forward decrypted data
  this->handleDatagram(inBuffer, rxlen);
}

bool
SecureSocket::sendDatagram(const uint8_t *data, size_t len) {
  uint8_t msg[DHT_MAX_MESSAGE_SIZE];
  uint8_t *ptr = msg;
  int txlen = 0, enclen=0;

  // store sequence number
  *((uint64_t *)ptr) = qToBigEndian(qint64(_outSeq)); txlen += 8; ptr += 8;

  // Get ptr to auth tag
  uint8_t *tag = ptr; txlen += 16; ptr += 16;

  // store encrypted data if there is any
  if ( (len <= 0) || (0 > (enclen = encrypt(_outSeq, data, len, ptr, tag))) )
    return false;
  txlen += enclen;

  // Send datagram
  if (! _dht.sendData(_streamId, msg, txlen, _peer)) {
    return false;
  }

  // Update seq, done
  _outSeq += txlen;
  return true;
}

bool
SecureSocket::sendNull() {
  // send only stream id
  return _dht.sendData(_streamId, 0, 0, _peer);
}


/* ******************************************************************************************** *
 * Implementation of SocketHandler
 * ******************************************************************************************** */
ServiceHandler::ServiceHandler()
{
  // pass...
}

ServiceHandler::~ServiceHandler() {
  // pass...
}


/* ******************************************************************************************** *
 * Implementation of AbstractService
 * ******************************************************************************************** */
AbstractService::AbstractService()
{
  // pass...
}

AbstractService::~AbstractService() {
  // pass...
}

