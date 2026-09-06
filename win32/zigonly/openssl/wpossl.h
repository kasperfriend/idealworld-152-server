/* Local (zig/clang) compile-only stand-in for the OpenSSL API subset the
 * server uses.  Never used by the native MSYS2/OpenSSL build, which compiles
 * against the real mingw-w64 OpenSSL headers. */
#ifndef _WP_OPENSSL_STUB_H
#define _WP_OPENSSL_STUB_H

#define OPENSSL_VERSION_NUMBER 0x10100000L

#ifdef __cplusplus
extern "C" {
#endif

typedef struct evp_pkey_st EVP_PKEY;
typedef struct evp_md_st EVP_MD;
typedef struct evp_md_ctx_st EVP_MD_CTX;
typedef struct x509_st X509;
typedef struct x509_store_st X509_STORE;
typedef struct x509_store_ctx_st X509_STORE_CTX;
typedef struct x509_name_st X509_NAME;
typedef struct rsa_st RSA;
typedef struct bio_st BIO;
typedef struct bio_method_st BIO_METHOD;
typedef struct engine_st ENGINE;
typedef struct x509_req_st X509_REQ;

typedef int (*X509_STORE_CTX_verify_cb)(int, X509_STORE_CTX *);

#define NID_commonName 13
#define NID_organizationalUnitName 65

#define X509_V_ERR_CERT_HAS_EXPIRED 10
#define RSA_PKCS1_PADDING 1

X509 *d2i_X509(X509 **a, const unsigned char **in, long len);
EVP_PKEY *X509_get_pubkey(X509 *x);
X509_NAME *X509_get_subject_name(const X509 *x);
void X509_free(X509 *a);
void EVP_PKEY_free(EVP_PKEY *p);
int X509_NAME_get_text_by_NID(X509_NAME *name, int nid, char *buf, int len);

X509_STORE *X509_STORE_new(void);
int X509_STORE_add_cert(X509_STORE *ctx, X509 *x);
int X509_STORE_set_default_paths(X509_STORE *ctx);
void X509_STORE_free(X509_STORE *v);
X509_STORE_CTX *X509_STORE_CTX_new(void);
int X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store, X509 *x509, void *chain);
int X509_STORE_CTX_set_verify_cb(X509_STORE_CTX *ctx, X509_STORE_CTX_verify_cb cb);
void X509_STORE_CTX_free(X509_STORE_CTX *ctx);
int X509_STORE_CTX_get_error(X509_STORE_CTX *ctx);
int X509_verify_cert(X509_STORE_CTX *ctx);

EVP_MD_CTX *EVP_MD_CTX_create(void);
void EVP_MD_CTX_destroy(EVP_MD_CTX *ctx);
const EVP_MD *EVP_md5(void);
int EVP_VerifyInit(EVP_MD_CTX *ctx, const EVP_MD *type);
int EVP_VerifyUpdate(EVP_MD_CTX *ctx, const void *d, unsigned int cnt);
int EVP_VerifyFinal(EVP_MD_CTX *ctx, unsigned char *sig, unsigned int siglen, EVP_PKEY *pkey);
int OpenSSL_add_all_algorithms(void);

BIO *BIO_new_file(const char *filename, const char *mode);
BIO *BIO_new(BIO_METHOD *type);
BIO_METHOD *BIO_s_file(void);
int BIO_read_filename(BIO *b, const char *name);
int BIO_free(BIO *a);
X509 *PEM_read_bio_X509(BIO *bp, X509 **x, void *cb, void *u);
EVP_PKEY *PEM_read_bio_PrivateKey(BIO *bp, EVP_PKEY **x, void *cb, void *u);
void ERR_load_BIO_strings(void);
void ERR_print_errors_fp(void *fp);

int RAND_status(void);
void RAND_add(const void *buf, int num, double entropy);
int RAND_bytes(unsigned char *buf, int num);

RSA *EVP_PKEY_get1_RSA(EVP_PKEY *pkey);
void RSA_free(RSA *r);
int RSA_public_encrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding);
int RSA_public_decrypt(int flen, const unsigned char *from, unsigned char *to, RSA *rsa, int padding);
void RSA_get0_key(const RSA *r, const void **n, const void **e, const void **d);

#ifdef __cplusplus
}
#endif
#endif
