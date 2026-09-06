/*
	base64.cpp - base64 codec of the cmlib (abase) support library.

	The alphabet and the reverse table are the ones found in the original
	cmlib object ("A..Za..0..9+/" with '=' as the pad character, every other
	byte -1 in the decode table), so strings produced by the old server keep
	decoding - this is what the login / matrix-passwd glue hands to the
	account server.

	Contract from cgame/include/base64.h:

		base64_encode(in, inlen, out)
			writes ceil(inlen/3)*4 characters, always 0 terminated, and
			returns the number of characters written (excluding the 0).
			`out` therefore needs (inlen+2)/3*4 + 1 bytes.

		base64_decode(in, inlen, out)
			returns the number of bytes written, or -1 when the input is
			not a multiple of 4 or holds a character outside the alphabet.
			'=' padding is accepted and shortens the output as usual.
*/

#include "base64.h"

static const char b64_alphabet[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* -1 marks "not part of the alphabet", '=' is filtered out before lookup */
static const signed char b64_reverse[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,	/* '+' '/' */
	52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,	/* '0'-'9' '=' */
	-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,	/* 'A'-'N' */
	15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,	/* 'O'-'Z' */
	-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,	/* 'a'-'n' */
	41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,	/* 'o'-'z' */
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

int base64_encode(unsigned char * __in, int __inlen, char * __out)
{
	if (!__out) return -1;
	if (!__in || __inlen <= 0)
	{
		*__out = 0;
		return 0;
	}

	char * out = __out;
	int i = 0;
	while (i + 3 <= __inlen)
	{
		unsigned v = ((unsigned)__in[i] << 16) | ((unsigned)__in[i + 1] << 8) | __in[i + 2];
		*out++ = b64_alphabet[(v >> 18) & 0x3f];
		*out++ = b64_alphabet[(v >> 12) & 0x3f];
		*out++ = b64_alphabet[(v >> 6) & 0x3f];
		*out++ = b64_alphabet[v & 0x3f];
		i += 3;
	}

	int rest = __inlen - i;
	if (rest == 1)
	{
		unsigned v = (unsigned)__in[i] << 16;
		*out++ = b64_alphabet[(v >> 18) & 0x3f];
		*out++ = b64_alphabet[(v >> 12) & 0x3f];
		*out++ = '=';
		*out++ = '=';
	}
	else if (rest == 2)
	{
		unsigned v = ((unsigned)__in[i] << 16) | ((unsigned)__in[i + 1] << 8);
		*out++ = b64_alphabet[(v >> 18) & 0x3f];
		*out++ = b64_alphabet[(v >> 12) & 0x3f];
		*out++ = b64_alphabet[(v >> 6) & 0x3f];
		*out++ = '=';
	}

	*out = 0;
	return (int)(out - __out);
}

int base64_decode(char * __in, int __inlen, unsigned char * __out)
{
	if (!__in || __inlen <= 0) return 0;
	if (!__out) return -1;
	if ((__inlen & 3) != 0) return -1;	/* must be a whole number of groups */

	unsigned char * out = __out;
	for (int i = 0; i < __inlen; i += 4)
	{
		int v[4];
		int pad = 0;
		for (int k = 0; k < 4; ++k)
		{
			char c = __in[i + k];
			if (c == '=')
			{
				/* only legal in the last two slots of the last group */
				if (k < 2) return -1;
				v[k] = 0;
				++pad;
				continue;
			}
			signed char r = b64_reverse[(unsigned char)c];
			if (r < 0) return -1;
			if (pad) return -1;		/* data after padding */
			v[k] = r;
		}

		unsigned b = ((unsigned)v[0] << 18) | ((unsigned)v[1] << 12) |
			     ((unsigned)v[2] << 6) | (unsigned)v[3];
		*out++ = (unsigned char)(b >> 16);
		if (pad < 2) *out++ = (unsigned char)(b >> 8);
		if (pad < 1) *out++ = (unsigned char)(b);
	}

	return (int)(out - __out);
}
