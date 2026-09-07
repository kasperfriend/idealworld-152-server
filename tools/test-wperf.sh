#!/bin/sh
# test-wperf.sh - differential test: win32/wperf.c must behave bit-identically
# to the cnet/perf/x86_64 assembly originals (base64.s, crc32.s) on every
# input, including invalid base64 bytes (garbage-in, same-garbage-out).
#
# Linux/x86_64-only dev tool (needs the System V asm as reference).  Runs in
# a few seconds:  ./tools/test-wperf.sh
set -e
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT INT TERM

# Reference objects: rename asm symbols so both implementations link together.
as --64 "$ROOT/cnet/perf/x86_64/base64.s" -o "$TMP/ref_b64.o"
as --64 "$ROOT/cnet/perf/x86_64/crc32.s" -o "$TMP/ref_crc.o"
objcopy --redefine-sym base64_encode=ref_base64_encode \
	--redefine-sym base64_decode=ref_base64_decode "$TMP/ref_b64.o"
objcopy --redefine-sym crc32=ref_crc32 "$TMP/ref_crc.o"

cat > "$TMP/t.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void base64_encode(void *o, const void *i, unsigned long n);
unsigned long base64_decode(void *o, const void *i, unsigned long n);
unsigned long crc32(const unsigned char *s, unsigned long n);
void ref_base64_encode(void *o, const void *i, unsigned long n);
unsigned long ref_base64_decode(void *o, const void *i, unsigned long n);
unsigned long ref_crc32(const unsigned char *s, unsigned long n);

static unsigned long long rng = 0x123456789ABCDEFULL;
static unsigned rnd(unsigned m) {
	rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
	return (unsigned)((rng >> 33) % m);
}

static int fails = 0;
#define CHECK(cond, ...) do { \
	if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); fails++; } \
} while (0)

int main(void) {
	unsigned char in[300], o1[512], o2[512];
	int iter, i;
	/* fixed vectors incl. warmer regression probes */
	static const char *vecs[] = {
		"", "f", "fo", "foo", "foob", "fooba", "foobar",
		"Man", "Ma", "M", "any carnal pleasure.",
		"AAA!", "AAA!AAAA", "====", "TWFu", "TWE=", "TQ==", "TWF",
		"!@#$%^&*()_+-=[]{}|;:',.<>/? \t\n", "abcd\0efgh"
	};
	for (i = 0; i < 40; i++) {  /* exhaustive small encode lengths */
		int len = i;
		for (iter = 0; iter < 50; iter++) {
			int k;
			for (k = 0; k < len; k++) in[k] = rnd(256);
			memset(o1, 0xA5, sizeof o1); memset(o2, 0x5A, sizeof o2);
			base64_encode(o1, in, len);
			ref_base64_encode(o2, in, len);
			CHECK(memcmp(o1, o2, (len + 2) / 3 * 4) == 0,
				"encode len=%d iter=%d\n", len, iter);
		}
	}
	for (i = 0; i < (int)(sizeof vecs / sizeof vecs[0]); i++) {
		size_t len = strlen(vecs[i]) + (i == 19 ? 5 : 0); /* keep \0 case */
		unsigned long n1, n2;
		memset(o1, 0xA5, sizeof o1); memset(o2, 0x5A, sizeof o2);
		n1 = base64_decode(o1, vecs[i], len);
		n2 = ref_base64_decode(o2, vecs[i], len);
		CHECK(n1 == n2 && memcmp(o1, o2, n1) == 0,
			"decode vec %d n=%lu ref=%lu\n", i, n1, n2);
	}
	for (iter = 0; iter < 20000; iter++) {  /* fuzz decode incl. '=' */
		static const char alpha[] =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
		int len = rnd(65), k;
		unsigned long n1, n2;
		for (k = 0; k < len; k++) {
			unsigned r = rnd(100);
			in[k] = r < 80 ? alpha[rnd(65)] : rnd(256);
		}
		memset(o1, 0xA5, sizeof o1); memset(o2, 0x5A, sizeof o2);
		n1 = base64_decode(o1, in, len);
		n2 = ref_base64_decode(o2, in, len);
		CHECK(n1 == n2 && memcmp(o1, o2, n1) == 0,
			"decode fuzz iter=%d len=%d n=%lu ref=%lu\n",
			iter, len, n1, n2);
		if (fails > 5) return 1;
	}
	for (i = 1; i < 260; i++) {  /* crc32 all small lengths (asm: len>=1) */
		for (iter = 0; iter < 20; iter++) {
			int k;
			for (k = 0; k < i; k++) in[k] = rnd(256);
			CHECK(crc32(in, i) == ref_crc32(in, i),
				"crc32 len=%d\n", i);
		}
	}
	/* crc32 known-answer (init 0, no final xor - NOT zlib's 0xCBF43926) */
	printf("crc32('123456789') = %08lx / ref %08lx\n",
		crc32((unsigned char *)"123456789", 9),
		ref_crc32((unsigned char *)"123456789", 9));
	CHECK(crc32((unsigned char *)"123456789", 9) ==
		ref_crc32((unsigned char *)"123456789", 9),
		"crc32 known-answer mismatch\n");
	CHECK(crc32(0, 9) == 0, "crc32 NULL != 0\n");
	CHECK(crc32(in, 0) == 0, "crc32 len0 != 0\n");
	if (!fails) printf("wperf: all differential tests passed\n");
	return fails != 0;
}
EOF
cc -O2 -Wall -o "$TMP/t" "$TMP/t.c" "$ROOT/win32/wperf.c" \
	"$TMP/ref_b64.o" "$TMP/ref_crc.o"
"$TMP/t"
