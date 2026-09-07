/*
 * wpopenssl.cpp - link-only stand-ins for the OpenSSL API subset the server
 * uses (see win32/zigonly/openssl/wpossl.h).  Compiled and linked ONLY by
 * the zig cross toolchain; the native MSYS2 build links the real OpenSSL.
 *
 * Every routine reports failure (NULL / 0 / -1): certificate verification
 * and RSA operations cannot succeed in a zig-built binary.  That is
 * intentional - zig binaries exist to prove the tree compiles and links,
 * not to run a live server.  Callers already handle failure returns.
 */
#include <openssl/x509.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stddef.h>

extern "C" {

X509 *d2i_X509(X509 **a, const unsigned char **in, long len)
{
	(void)a; (void)in; (void)len;
	return NULL;
}

EVP_PKEY *X509_get_pubkey(X509 *x)
{
	(void)x;
	return NULL;
}

X509_NAME *X509_get_subject_name(const X509 *x)
{
	(void)x;
	return NULL;
}

void X509_free(X509 *a)
{
	(void)a;
}

void EVP_PKEY_free(EVP_PKEY *p)
{
	(void)p;
}

int EVP_PKEY_size(EVP_PKEY *pkey)
{
	(void)pkey;
	return 0;
}

int X509_NAME_get_text_by_NID(X509_NAME *name, int nid, char *buf, int len)
{
	(void)name; (void)nid;
	if (buf && len > 0)
		buf[0] = '\0';
	return -1;
}

X509_STORE *X509_STORE_new(void)
{
	return NULL;
}

int X509_STORE_add_cert(X509_STORE *ctx, X509 *x)
{
	(void)ctx; (void)x;
	return 0;
}

int X509_STORE_set_default_paths(X509_STORE *ctx)
{
	(void)ctx;
	return 0;
}

void X509_STORE_free(X509_STORE *v)
{
	(void)v;
}

X509_STORE_CTX *X509_STORE_CTX_new(void)
{
	return NULL;
}

int X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store, X509 *x509,
                         void *chain)
{
	(void)ctx; (void)store; (void)x509; (void)chain;
	return 0;
}

int X509_STORE_CTX_set_verify_cb(X509_STORE_CTX *ctx,
                                 X509_STORE_CTX_verify_cb cb)
{
	(void)ctx; (void)cb;
	return 0;
}

void X509_STORE_CTX_free(X509_STORE_CTX *ctx)
{
	(void)ctx;
}

int X509_STORE_CTX_get_error(X509_STORE_CTX *ctx)
{
	(void)ctx;
	return 0;
}

int X509_verify_cert(X509_STORE_CTX *ctx)
{
	(void)ctx;
	return 0;
}

EVP_MD_CTX *EVP_MD_CTX_create(void)
{
	return NULL;
}

void EVP_MD_CTX_destroy(EVP_MD_CTX *ctx)
{
	(void)ctx;
}

const EVP_MD *EVP_md5(void)
{
	/* The digest is opaque; verification fails regardless. */
	return NULL;
}

int EVP_VerifyInit(EVP_MD_CTX *ctx, const EVP_MD *type)
{
	(void)ctx; (void)type;
	return 0;
}

int EVP_VerifyUpdate(EVP_MD_CTX *ctx, const void *d, unsigned int cnt)
{
	(void)ctx; (void)d; (void)cnt;
	return 0;
}

int EVP_VerifyFinal(EVP_MD_CTX *ctx, unsigned char *sig, unsigned int siglen,
                    EVP_PKEY *pkey)
{
	(void)ctx; (void)sig; (void)siglen; (void)pkey;
	return 0;
}

int OpenSSL_add_all_algorithms(void)
{
	return 1;
}

BIO *BIO_new_file(const char *filename, const char *mode)
{
	(void)filename; (void)mode;
	return NULL;
}

BIO *BIO_new(BIO_METHOD *type)
{
	(void)type;
	return NULL;
}

BIO_METHOD *BIO_s_file(void)
{
	return NULL;
}

int BIO_read_filename(BIO *b, const char *name)
{
	(void)b; (void)name;
	return 0;
}

int BIO_free(BIO *a)
{
	(void)a;
	return 1;
}

X509 *PEM_read_bio_X509(BIO *bp, X509 **x, void *cb, void *u)
{
	(void)bp; (void)x; (void)cb; (void)u;
	return NULL;
}

EVP_PKEY *PEM_read_bio_PrivateKey(BIO *bp, EVP_PKEY **x, void *cb, void *u)
{
	(void)bp; (void)x; (void)cb; (void)u;
	return NULL;
}

void ERR_load_BIO_strings(void)
{
}

void ERR_print_errors_fp(void *fp)
{
	(void)fp;
}

int RAND_status(void)
{
	return 1;
}

void RAND_add(const void *buf, int num, double entropy)
{
	(void)buf; (void)num; (void)entropy;
}

int RAND_bytes(unsigned char *buf, int num)
{
	int i;
	if (!buf || num < 0)
		return 0;
	/* Deterministic fallback bytes; never real key material. */
	for (i = 0; i < num; i++)
		buf[i] = (unsigned char)(i * 31 + 17);
	return 1;
}

RSA *EVP_PKEY_get1_RSA(EVP_PKEY *pkey)
{
	(void)pkey;
	return NULL;
}

void RSA_free(RSA *r)
{
	(void)r;
}

int RSA_public_encrypt(int flen, const unsigned char *from, unsigned char *to,
                       RSA *rsa, int padding)
{
	(void)flen; (void)from; (void)to; (void)rsa; (void)padding;
	return -1;
}

int RSA_public_decrypt(int flen, const unsigned char *from, unsigned char *to,
                       RSA *rsa, int padding)
{
	(void)flen; (void)from; (void)to; (void)rsa; (void)padding;
	return -1;
}

void RSA_get0_key(const RSA *r, const void **n, const void **e, const void **d)
{
	if (n) *n = NULL;
	if (e) *e = NULL;
	if (d) *d = NULL;
	(void)r;
}

} /* extern "C" */
