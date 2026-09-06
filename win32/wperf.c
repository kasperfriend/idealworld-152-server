/*
 * wperf.c - portable C replacements for the cnet/perf assembly helpers used
 * on x86_64, for the Windows server build.
 *
 * The cnet/perf/x86_64 asm files are written for the System V ABI (arguments
 * in %rdi/%rsi/%rdx): they assemble to COFF objects just fine, but the code
 * they contain reads the wrong registers under the Windows x64 calling
 * convention, so linking them would produce silently corrupt results.  The
 * only three entry points referenced anywhere on x86_64 are reimplemented
 * here in plain C with bit-identical behaviour (verified against the
 * original objects, see tools/test-wperf.sh):
 *
 *   base64_encode / base64_decode   (cnet/io/base64.h)
 *   crc32                           (cnet/gamed/libcommon.h)
 *
 * Everything else in cnet/perf (rc4/md5/sha1/aes/bf/mppc256) is only
 * referenced from __i386__-only code paths (cnet/io/security.h) or not at
 * all, and is deliberately absent here so any unexpected reference fails
 * loudly at link time instead of binding to a stub.
 *
 * This file is compiled for both Windows toolchains (zig cross and native
 * MSYS2); the Linux build keeps using the assembly originals.
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ crc32 */

static const unsigned int wp_crc32_table[256] = {
	0x00000000U, 0x77073096U, 0xEE0E612CU, 0x990951BAU, 0x076DC419U,
	0x706AF48FU, 0xE963A535U, 0x9E6495A3U, 0x0EDB8832U, 0x79DCB8A4U,
	0xE0D5E91EU, 0x97D2D988U, 0x09B64C2BU, 0x7EB17CBDU, 0xE7B82D07U,
	0x90BF1D91U, 0x1DB71064U, 0x6AB020F2U, 0xF3B97148U, 0x84BE41DEU,
	0x1ADAD47DU, 0x6DDDE4EBU, 0xF4D4B551U, 0x83D385C7U, 0x136C9856U,
	0x646BA8C0U, 0xFD62F97AU, 0x8A65C9ECU, 0x14015C4FU, 0x63066CD9U,
	0xFA0F3D63U, 0x8D080DF5U, 0x3B6E20C8U, 0x4C69105EU, 0xD56041E4U,
	0xA2677172U, 0x3C03E4D1U, 0x4B04D447U, 0xD20D85FDU, 0xA50AB56BU,
	0x35B5A8FAU, 0x42B2986CU, 0xDBBBC9D6U, 0xACBCF940U, 0x32D86CE3U,
	0x45DF5C75U, 0xDCD60DCFU, 0xABD13D59U, 0x26D930ACU, 0x51DE003AU,
	0xC8D75180U, 0xBFD06116U, 0x21B4F4B5U, 0x56B3C423U, 0xCFBA9599U,
	0xB8BDA50FU, 0x2802B89EU, 0x5F058808U, 0xC60CD9B2U, 0xB10BE924U,
	0x2F6F7C87U, 0x58684C11U, 0xC1611DABU, 0xB6662D3DU, 0x76DC4190U,
	0x01DB7106U, 0x98D220BCU, 0xEFD5102AU, 0x71B18589U, 0x06B6B51FU,
	0x9FBFE4A5U, 0xE8B8D433U, 0x7807C9A2U, 0x0F00F934U, 0x9609A88EU,
	0xE10E9818U, 0x7F6A0DBBU, 0x086D3D2DU, 0x91646C97U, 0xE6635C01U,
	0x6B6B51F4U, 0x1C6C6162U, 0x856530D8U, 0xF262004EU, 0x6C0695EDU,
	0x1B01A57BU, 0x8208F4C1U, 0xF50FC457U, 0x65B0D9C6U, 0x12B7E950U,
	0x8BBEB8EAU, 0xFCB9887CU, 0x62DD1DDFU, 0x15DA2D49U, 0x8CD37CF3U,
	0xFBD44C65U, 0x4DB26158U, 0x3AB551CEU, 0xA3BC0074U, 0xD4BB30E2U,
	0x4ADFA541U, 0x3DD895D7U, 0xA4D1C46DU, 0xD3D6F4FBU, 0x4369E96AU,
	0x346ED9FCU, 0xAD678846U, 0xDA60B8D0U, 0x44042D73U, 0x33031DE5U,
	0xAA0A4C5FU, 0xDD0D7CC9U, 0x5005713CU, 0x270241AAU, 0xBE0B1010U,
	0xC90C2086U, 0x5768B525U, 0x206F85B3U, 0xB966D409U, 0xCE61E49FU,
	0x5EDEF90EU, 0x29D9C998U, 0xB0D09822U, 0xC7D7A8B4U, 0x59B33D17U,
	0x2EB40D81U, 0xB7BD5C3BU, 0xC0BA6CADU, 0xEDB88320U, 0x9ABFB3B6U,
	0x03B6E20CU, 0x74B1D29AU, 0xEAD54739U, 0x9DD277AFU, 0x04DB2615U,
	0x73DC1683U, 0xE3630B12U, 0x94643B84U, 0x0D6D6A3EU, 0x7A6A5AA8U,
	0xE40ECF0BU, 0x9309FF9DU, 0x0A00AE27U, 0x7D079EB1U, 0xF00F9344U,
	0x8708A3D2U, 0x1E01F268U, 0x6906C2FEU, 0xF762575DU, 0x806567CBU,
	0x196C3671U, 0x6E6B06E7U, 0xFED41B76U, 0x89D32BE0U, 0x10DA7A5AU,
	0x67DD4ACCU, 0xF9B9DF6FU, 0x8EBEEFF9U, 0x17B7BE43U, 0x60B08ED5U,
	0xD6D6A3E8U, 0xA1D1937EU, 0x38D8C2C4U, 0x4FDFF252U, 0xD1BB67F1U,
	0xA6BC5767U, 0x3FB506DDU, 0x48B2364BU, 0xD80D2BDAU, 0xAF0A1B4CU,
	0x36034AF6U, 0x41047A60U, 0xDF60EFC3U, 0xA867DF55U, 0x316E8EEFU,
	0x4669BE79U, 0xCB61B38CU, 0xBC66831AU, 0x256FD2A0U, 0x5268E236U,
	0xCC0C7795U, 0xBB0B4703U, 0x220216B9U, 0x5505262FU, 0xC5BA3BBEU,
	0xB2BD0B28U, 0x2BB45A92U, 0x5CB36A04U, 0xC2D7FFA7U, 0xB5D0CF31U,
	0x2CD99E8BU, 0x5BDEAE1DU, 0x9B64C2B0U, 0xEC63F226U, 0x756AA39CU,
	0x026D930AU, 0x9C0906A9U, 0xEB0E363FU, 0x72076785U, 0x05005713U,
	0x95BF4A82U, 0xE2B87A14U, 0x7BB12BAEU, 0x0CB61B38U, 0x92D28E9BU,
	0xE5D5BE0DU, 0x7CDCEFB7U, 0x0BDBDF21U, 0x86D3D2D4U, 0xF1D4E242U,
	0x68DDB3F8U, 0x1FDA836EU, 0x81BE16CDU, 0xF6B9265BU, 0x6FB077E1U,
	0x18B74777U, 0x88085AE6U, 0xFF0F6A70U, 0x66063BCAU, 0x11010B5CU,
	0x8F659EFFU, 0xF862AE69U, 0x616BFFD3U, 0x166CCF45U, 0xA00AE278U,
	0xD70DD2EEU, 0x4E048354U, 0x3903B3C2U, 0xA7672661U, 0xD06016F7U,
	0x4969474DU, 0x3E6E77DBU, 0xAED16A4AU, 0xD9D65ADCU, 0x40DF0B66U,
	0x37D83BF0U, 0xA9BCAE53U, 0xDEBB9EC5U, 0x47B2CF7FU, 0x30B5FFE9U,
	0xBDBDF21CU, 0xCABAC28AU, 0x53B39330U, 0x24B4A3A6U, 0xBAD03605U,
	0xCDD70693U, 0x54DE5729U, 0x23D967BFU, 0xB3667A2EU, 0xC4614AB8U,
	0x5D681B02U, 0x2A6F2B94U, 0xB40BBE37U, 0xC30C8EA1U, 0x5A05DF1BU,
	0x2D02EF8DU
};

/* Reflected CRC-32 (poly 0xEDB88320), init 0, no final complement - exactly
 * the loop in cnet/perf/x86_64/crc32.s.  The asm reads one byte too many for
 * len == 0; this version returns 0 instead of running off the end. */
unsigned long wp_crc32(const unsigned char *s, unsigned long len);

unsigned long crc32(const unsigned char *s, unsigned long len)
{
	unsigned long i;
	unsigned int crc = 0;
	if (s == 0)
		return 0;
	for (i = 0; i < len; i++)
		crc = wp_crc32_table[(crc ^ s[i]) & 0xFF] ^ (crc >> 8);
	return (unsigned long)crc;
}

/* ----------------------------------------------------------------- base64 */

static const char wp_b64_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void base64_encode(void *output, const void *input, unsigned long len)
{
	const unsigned char *in = (const unsigned char *)input;
	unsigned char *out = (unsigned char *)output;
	unsigned long i = 0;

	/* Full 6-byte blocks first, like the asm fast path. */
	while (len - i >= 6)
	{
		unsigned long long v =
			((unsigned long long)in[i] << 40) |
			((unsigned long long)in[i + 1] << 32) |
			((unsigned long long)in[i + 2] << 24) |
			((unsigned long long)in[i + 3] << 16) |
			((unsigned long long)in[i + 4] << 8) |
			(unsigned long long)in[i + 5];
		out[0] = wp_b64_alphabet[(v >> 42) & 63];
		out[1] = wp_b64_alphabet[(v >> 36) & 63];
		out[2] = wp_b64_alphabet[(v >> 30) & 63];
		out[3] = wp_b64_alphabet[(v >> 24) & 63];
		out[4] = wp_b64_alphabet[(v >> 18) & 63];
		out[5] = wp_b64_alphabet[(v >> 12) & 63];
		out[6] = wp_b64_alphabet[(v >> 6) & 63];
		out[7] = wp_b64_alphabet[v & 63];
		out += 8;
		i += 6;
	}
	if (len - i >= 3)
	{
		unsigned int v =
			((unsigned int)in[i] << 16) |
			((unsigned int)in[i + 1] << 8) |
			(unsigned int)in[i + 2];
		out[0] = wp_b64_alphabet[(v >> 18) & 63];
		out[1] = wp_b64_alphabet[(v >> 12) & 63];
		out[2] = wp_b64_alphabet[(v >> 6) & 63];
		out[3] = wp_b64_alphabet[v & 63];
		out += 4;
		i += 3;
	}
	if (len - i == 2)
	{
		unsigned int v =
			((unsigned int)in[i] << 16) |
			((unsigned int)in[i + 1] << 8);
		out[0] = wp_b64_alphabet[(v >> 18) & 63];
		out[1] = wp_b64_alphabet[(v >> 12) & 63];
		out[2] = wp_b64_alphabet[(v >> 6) & 63];
		out[3] = '=';
	}
	else if (len - i == 1)
	{
		unsigned int v = (unsigned int)in[i] << 16;
		out[0] = wp_b64_alphabet[(v >> 18) & 63];
		out[1] = wp_b64_alphabet[(v >> 12) & 63];
		out[2] = '=';
		out[3] = '=';
	}
}

/* Inverse alphabet.  Entries 0x00-0x7F match the 128-byte table embedded in
 * base64.s (0xFF = "no such character"); invalid bytes are NOT rejected, they
 * decode through the raw table value just as the asm version does.
 *
 * Entries 0x80-0xFF replicate a quirk of the asm: its table is only 128 bytes
 * long, so input bytes >= 0x80 read past the table into the machine code that
 * follows it (pop %r8, the decode loop, ...).  The bytes below are exactly
 * those following code bytes as emitted by GNU as for the current base64.s -
 * tools/test-wperf.sh re-assembles the reference and fails loudly if the two
 * ever drift apart, so this stays bit-identical on every possible input. */
static const unsigned char wp_b64_inverse[256] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, 0xFF, 0x3F,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
	0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
	0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
	0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	/* 0x80-0xFF: over-read into the code following the asm table. */
	0x41, 0x58, 0x49, 0x83, 0xFB, 0x04, 0x76, 0x52,
	0x0F, 0xB6, 0x06, 0x0F, 0xB6, 0x5E, 0x01, 0x0F,
	0xB6, 0x4E, 0x02, 0x0F, 0xB6, 0x56, 0x03, 0x41,
	0x0F, 0xB6, 0x04, 0x00, 0x41, 0x0F, 0xB6, 0x1C,
	0x18, 0x41, 0x0F, 0xB6, 0x0C, 0x08, 0x41, 0x0F,
	0xB6, 0x14, 0x10, 0xC1, 0xE0, 0x1A, 0xC1, 0xE3,
	0x14, 0xC1, 0xE1, 0x0E, 0xC1, 0xE2, 0x08, 0x09,
	0xD8, 0x09, 0xC8, 0x09, 0xD0, 0x0F, 0xC8, 0x66,
	0x89, 0x07, 0xC1, 0xE8, 0x10, 0x88, 0x47, 0x02,
	0x48, 0x83, 0xC6, 0x04, 0x48, 0x83, 0xC7, 0x03,
	0x49, 0x83, 0xC2, 0x03, 0x49, 0x83, 0xEB, 0x04,
	0xEB, 0xA8, 0x75, 0x5B, 0x0F, 0xB6, 0x1E, 0x41,
	0x0F, 0xB6, 0x04, 0x18, 0x0F, 0xB6, 0x5E, 0x01,
	0xC1, 0xE0, 0x02, 0x41, 0x0F, 0xB6, 0x1C, 0x18,
	0x89, 0xDA, 0xC1, 0xEB, 0x04, 0xC1, 0xE2, 0x04,
	0x09, 0xD8, 0x88, 0x07, 0x0F, 0xB6, 0x5E, 0x02,
};

unsigned long base64_decode(void *output, const void *input, unsigned long len)
{
	const unsigned char *in = (const unsigned char *)input;
	unsigned char *out = (unsigned char *)output;
	unsigned long remaining = len;
	unsigned long count = 0;
	unsigned int a, b, c, d;

	/* Full groups: no '=' handling, raw table values (asm .Lloop_decode). */
	while (remaining > 4)
	{
		a = wp_b64_inverse[in[0]];
		b = wp_b64_inverse[in[1]];
		c = wp_b64_inverse[in[2]];
		d = wp_b64_inverse[in[3]];
		out[0] = (unsigned char)((a << 2) | (b >> 4));
		out[1] = (unsigned char)(((b & 15) << 4) | (c >> 2));
		out[2] = (unsigned char)(((c & 3) << 6) | d);
		in += 4;
		out += 3;
		count += 3;
		remaining -= 4;
	}
	/* A trailing 1-3 bytes are ignored entirely (asm .Llast_bytes_decode). */
	if (remaining != 4)
		return count;

	a = wp_b64_inverse[in[0]];
	b = wp_b64_inverse[in[1]];
	out[0] = (unsigned char)((a << 2) | (b >> 4));
	count += 1;
	if (in[2] == '=')
		return count;
	c = wp_b64_inverse[in[2]];
	out[1] = (unsigned char)(((b & 15) << 4) | (c >> 2));
	count += 1;
	if (in[3] == '=')
		return count;
	d = wp_b64_inverse[in[3]];
	out[2] = (unsigned char)(((c & 3) << 6) | d);
	count += 1;
	return count;
}

#ifdef __cplusplus
}
#endif
