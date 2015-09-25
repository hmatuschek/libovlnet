#include "identity.h"
#include <QDir>

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ec.h>
#include <openssl/pem.h>


Identity::Identity(EVP_PKEY *key, QObject *parent)
  : QObject(parent), _keyPair(key)
{
  EVP_MD_CTX mdctx;
  char *data = 0;
  unsigned int siglen = 20;
  unsigned char fingerprint[20];
  size_t len;

  // Compute RMD160 hash of public key in PEM format
  BIO *buffer = BIO_new(BIO_s_mem());
  if (! i2d_EC_PUBKEY_bio(buffer, EVP_PKEY_get1_EC_KEY(key) ))
    goto error;
  // Get buffer & size of binary enc. public key
  len = BIO_get_mem_data(buffer, &data);

  EVP_MD_CTX_init(&mdctx);
  if (!EVP_DigestInit(&mdctx, EVP_ripemd160()))
    goto error;

  if (!EVP_DigestUpdate(&mdctx, data, len))
    goto error;

  if (!EVP_DigestFinal(&mdctx, fingerprint, &siglen))
    goto error;

  EVP_MD_CTX_cleanup(&mdctx);
  BIO_free_all(buffer);

  _fingerprint = Identifier((char *)fingerprint);
  return;

error:
  ERR_load_crypto_strings();
  ERR_print_errors_fp(stderr);
  EVP_MD_CTX_cleanup(&mdctx);
  if (buffer) { BIO_free_all(buffer); }
}

Identity::~Identity() {
  EVP_PKEY_free(_keyPair);
  EVP_cleanup();
}

const Identifier &
Identity::id() const {
  return _fingerprint;
}

Identity *
Identity::newIdentity(const QFile &path)
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
  if (0 == (out = BIO_new_file(path.fileName().toLocal8Bit(), "w")))
    goto error;
  if (!PEM_write_bio_PUBKEY(out, pkey))
    goto error;
  if (!PEM_write_bio_PrivateKey(out, pkey, NULL, NULL, 0, 0, NULL))
    goto error;
  BIO_flush(out);
  BIO_free_all(out);

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
Identity::load(const QFile &path)
{
  BIO *bio = 0;
  EVP_PKEY *pkey = 0;
  // Read key from file
  if (0 == (bio = BIO_new_file(path.fileName().toLocal8Bit(), "r")))
    goto error;
  if (! (pkey = PEM_read_bio_PUBKEY(bio, 0, 0,0)))
    goto error;
  if (! (pkey = PEM_read_bio_PrivateKey(bio, &pkey, 0,0)) ) {
    // Ignore a missing private key^
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
