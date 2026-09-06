/*
 * wpmd5.cpp - OpenSSL-compatible one-shot MD5() built on the Windows
 * CNG/BCrypt API.  Only the subset used by the game server is provided
 * (<openssl/md5.h> -> void MD5(const unsigned char*, size_t,
 * unsigned char[16])).
 *
 * The native (MSYS2/OpenSSL) build uses the real OpenSSL MD5; this file is
 * harmless there and is only linked so the zig cross-toolchain can link gs.
 */
#include <windows.h>
#include <bcrypt.h>
#include <string.h>
#include <stddef.h>

extern "C" {

typedef struct
{
	unsigned char digest[16];
} wp_md5_ctx;

int wp_md5(const unsigned char *data, size_t len, unsigned char *out)
{
	BCRYPT_ALG_HANDLE alg = NULL;
	NTSTATUS st = BCryptOpenAlgorithmProvider(&alg, BCRYPT_MD5_ALGORITHM,
	                                          NULL, 0);
	if (st != 0)
		return 0;
	/* BCRYPT_HASH_REUSABLE_FLAG keeps a single handle valid for reuse */
	BCRYPT_HASH_HANDLE hash = NULL;
	st = BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0);
	if (st != 0)
	{
		BCryptCloseAlgorithmProvider(alg, 0);
		return 0;
	}
	if (len > 0 && data)
		BCryptHashData(hash, (PUCHAR)data, (ULONG)len, 0);
	st = BCryptFinishHash(hash, out, 16, 0);
	BCryptDestroyHash(hash);
	BCryptCloseAlgorithmProvider(alg, 0);
	return st == 0 ? 1 : 0;
}

/* OpenSSL compatible entry point */
void MD5(const unsigned char *d, size_t n, unsigned char *md)
{
	wp_md5(d, n, md);
}

} /* extern "C" */
