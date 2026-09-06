/*
 * winiconv.cpp - glibc-compatible subset of <iconv.h> implemented over the
 * Windows code page API.
 *
 * Supported charsets: UTF-8, UCS-2/UCS2 (big-endian UTF-16, as in glibc),
 * UCS-2LE/UTF-16LE, GBK/CP936/GB2312, BIG5/CP950, ASCII, GB18030 and other
 * Windows code pages referenced by number.
 */
#include <windows.h>
#include <wchar.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef EILSEQ
#define EILSEQ 84
#endif
#ifndef E2BIG
#define E2BIG 7
#endif
#ifndef EINVAL
#define EINVAL 22
#endif

extern "C" {

typedef void *iconv_t;

struct wp_iconv
{
	int incp;   /* source Windows code page (CP_UTF8 / 1200 / ... ) */
	int outcp;
	int in16;   /* source is UTF-16 with byte order */
	int inbe;
	int out16;
	int outbe;
};

static const char *wp_skip_suffixes(const char *s)
{
	/* strip //TRANSLIT, //IGNORE, // */
	static const char *const suffixes[] = { "//TRANSLIT", "//IGNORE", "//", NULL };
	for (int i = 0; suffixes[i]; i++)
	{
		size_t n = strlen(suffixes[i]);
		size_t m = strlen(s);
		if (m >= n && _stricmp(s + m - n, suffixes[i]) == 0)
		{
			char *copy = (char *)malloc(m - n + 1);
			memcpy(copy, s, m - n);
			copy[m - n] = 0;
			return copy;   /* caller frees */
		}
	}
	return NULL;
}

static int wp_charset_cp(const char *name, int *is16, int *be)
{
	*is16 = 0;
	*be = 0;
	char buf[64];
	size_t n = strlen(name);
	if (n >= sizeof(buf))
		return -1;
	for (size_t i = 0; i <= n; i++)
		buf[i] = (char)tolower((unsigned char)name[i]);

	if (!strcmp(buf, "utf-8") || !strcmp(buf, "utf8"))
		return CP_UTF8;
	if (!strcmp(buf, "ucs-2") || !strcmp(buf, "ucs2") ||
	    !strcmp(buf, "ucs-2be") || !strcmp(buf, "ucs2be"))
	{
		*is16 = 1; *be = 1;
		return 1200;
	}
	if (!strcmp(buf, "ucs-2le") || !strcmp(buf, "ucs2le"))
	{
		*is16 = 1; *be = 0;
		return 1200;
	}
	if (!strcmp(buf, "utf-16") || !strcmp(buf, "utf16") ||
	    !strcmp(buf, "utf-16be") || !strcmp(buf, "utf16be"))
	{
		*is16 = 1; *be = 1;
		return 1200;
	}
	if (!strcmp(buf, "utf-16le") || !strcmp(buf, "utf16le"))
	{
		*is16 = 1; *be = 0;
		return 1200;
	}
	if (!strcmp(buf, "gbk") || !strcmp(buf, "cp936") || !strcmp(buf, "ms936"))
		return 936;
	if (!strcmp(buf, "gb2312") || !strcmp(buf, "euc-cn") || !strcmp(buf, "euccn"))
		return 936;
	if (!strcmp(buf, "big5") || !strcmp(buf, "cp950") ||
	    !strcmp(buf, "big-5") || !strcmp(buf, "big5-hkscs"))
		return 950;
	if (!strcmp(buf, "gb18030"))
		return 54936;
	if (!strcmp(buf, "ascii") || !strcmp(buf, "us-ascii") ||
	    !strcmp(buf, "646") || !strcmp(buf, "ansi_x3.4-1968"))
		return 20127;
	if (!strcmp(buf, "latin1") || !strcmp(buf, "iso-8859-1") ||
	    !strcmp(buf, "iso8859-1"))
		return 28591;
	if (!strcmp(buf, "cp1252") || !strcmp(buf, "windows-1252"))
		return 1252;
	/* numeric windows code page? */
	char *end = NULL;
	long cp = strtol(buf, &end, 10);
	if (end && *end == 0 && cp > 0 && cp < 100000)
		return (int)cp;
	return -1;
}

iconv_t iconv_open(const char *tocode, const char *fromcode)
{
	if (!tocode || !fromcode)
		return (iconv_t)-1;
	/* strip a single //TRANSLIT or //IGNORE suffix */
	const char *f2 = wp_skip_suffixes(fromcode);
	const char *t2 = wp_skip_suffixes(tocode);
	const char *fa = f2 ? f2 : fromcode;
	const char *ta = t2 ? t2 : tocode;

	int f16, fbe, t16, tbe;
	int fcp = wp_charset_cp(fa, &f16, &fbe);
	int tcp = wp_charset_cp(ta, &t16, &tbe);
	free((void *)f2);
	free((void *)t2);
	if (fcp < 0 || tcp < 0)
		return (iconv_t)-1;

	wp_iconv *cd = new wp_iconv;
	cd->incp = fcp;
	cd->outcp = tcp;
	cd->in16 = f16;
	cd->inbe = fbe;
	cd->out16 = t16;
	cd->outbe = tbe;
	return (iconv_t)cd;
}

int iconv_close(iconv_t cd)
{
	if (cd == (iconv_t)-1 || cd == 0)
		return 0;
	delete (wp_iconv *)cd;
	return 0;
}

/* decode one source character; returns its byte length or 0 on invalid. */
static int wp_decode_one(wp_iconv *cd, const char *in, size_t left,
                         unsigned int *out_cp)
{
	if (left == 0)
		return 0;
	if (cd->in16)
	{
		if (left < 2)
			return -1;
		unsigned int u = ((unsigned char)in[0] << 8) | (unsigned char)in[1];
		if (!cd->inbe)
			u = (u >> 8) | ((u & 0xff) << 8);
		if (u >= 0xD800 && u <= 0xDBFF)
		{
			if (left < 4)
				return -1;   /* EINVAL: truncated */
			unsigned int lo = ((unsigned char)in[2] << 8) | (unsigned char)in[3];
			if (!cd->inbe)
				lo = (lo >> 8) | ((lo & 0xff) << 8);
			if (lo < 0xDC00 || lo > 0xDFFF)
				return 0;
			*out_cp = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
			return 4;
		}
		if (u >= 0xDC00 && u <= 0xDFFF)
			return 0;  /* stray low surrogate */
		*out_cp = u;
		return 2;
	}

	/* multi-byte or single-byte source: decode one character */
	int len = 1;
	if (cd->incp == CP_UTF8)
	{
		unsigned char b = (unsigned char)in[0];
		if (b < 0x80)
			len = 1;
		else if ((b & 0xE0) == 0xC0) len = 2;
		else if ((b & 0xF0) == 0xE0) len = 3;
		else if ((b & 0xF8) == 0xF0) len = 4;
		else
			return 0;
	}
	else if (cd->incp == 936 || cd->incp == 950 || cd->incp == 54936)
	{
		unsigned char b = (unsigned char)in[0];
		if (b < 0x80)
			len = 1;
		else
			len = 2;
	}
	else
	{
		len = 1;
	}
	if ((size_t)len > left)
		return -1;   /* EINVAL: need more input */

	unsigned int flags = MB_ERR_INVALID_CHARS;
	if (cd->incp == 20127 || cd->incp == 28591 || cd->incp == 1252)
	{
		*out_cp = (unsigned char)in[0];
		return len;
	}
	wchar_t wc = 0;
	int r = MultiByteToWideChar(cd->incp, flags, in, len, &wc, 1);
	if (r <= 0)
	{
		/* some code pages reject MB_ERR_INVALID_CHARS; retry without */
		r = MultiByteToWideChar(cd->incp, 0, in, len, &wc, 1);
		if (r <= 0)
			return 0;
	}
	*out_cp = wc;
	return len;
}

/* encode one code point; returns bytes written or -1 on E2BIG-like failure */
static int wp_encode_one(wp_iconv *cd, unsigned int cp, char *out, size_t cap)
{
	if (cd->out16)
	{
		unsigned int units[2];
		int nunits = 1;
		if (cp > 0xFFFF)
		{
			cp -= 0x10000;
			units[0] = 0xD800 + (cp >> 10);
			units[1] = 0xDC00 + (cp & 0x3FF);
			nunits = 2;
		}
		else
			units[0] = cp;
		if ((size_t)nunits * 2 > cap)
			return -2;
		for (int i = 0; i < nunits; i++)
		{
			unsigned int u = units[i];
			if (!cd->outbe)
				u = (u >> 8) | ((u & 0xff) << 8);
			out[i * 2] = (char)(u >> 8);
			out[i * 2 + 1] = (char)(u & 0xff);
		}
		return nunits * 2;
	}

	wchar_t wc = (wchar_t)(cp > 0xFFFF ? 0xFFFD : cp);
	if (cp > 0xFFFF)
	{
		/* no encoding for supplementary chars in these code pages */
		wc = L'?';
	}
	int flags = 0;
	if (cd->outcp == CP_UTF8)
		flags = WC_ERR_INVALID_CHARS;
	int r = WideCharToMultiByte(cd->outcp, flags, &wc, 1, out, (int)cap,
	                            NULL, NULL);
	if (r <= 0)
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			return -2;
		return 0;
	}
	return r;
}

size_t iconv(iconv_t cd_raw, char **inbuf, size_t *inbytesleft,
             char **outbuf, size_t *outbytesleft)
{
	wp_iconv *cd = (wp_iconv *)cd_raw;
	if (cd_raw == (iconv_t)-1 || cd == 0 || !outbuf || !outbytesleft)
	{
		errno = EINVAL;
		return (size_t)-1;
	}

	size_t total = 0;
	while (inbuf && *inbuf && inbytesleft && *inbytesleft > 0)
	{
		if (!outbuf || !*outbuf || *outbytesleft == 0)
		{
			errno = E2BIG;
			return (size_t)-1;
		}
		char *in = *inbuf;
		unsigned int cp = 0;
		int used = wp_decode_one(cd, in, *inbytesleft, &cp);
		if (used < 0)
		{
			errno = EINVAL;   /* truncated multibyte sequence */
			return (size_t)-1;
		}
		if (used == 0)
		{
			errno = EILSEQ;
			return (size_t)-1;
		}
		char tmp[8];
		int n = wp_encode_one(cd, cp, tmp, sizeof(tmp));
		if (n == -2 || (size_t)n > *outbytesleft)
		{
			/* output full: consume nothing, report E2BIG */
			errno = E2BIG;
			return (size_t)-1;
		}
		if (n <= 0)
		{
			errno = EILSEQ;
			return (size_t)-1;
		}
		memcpy(*outbuf, tmp, n);
		*inbuf += used;
		*inbytesleft -= used;
		*outbuf += n;
		*outbytesleft -= n;
		total += n;
	}
	return total;   /* number of non-reversible conversions: 0 */
}

} /* extern "C" */
