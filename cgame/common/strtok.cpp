/*
	strtok.cpp - abase::strtok, the string split helper of the cmlib
	(support) library.

	Used all over the gameserver to walk configuration values such as
	"instance_servers", world limits, accepted zones and the GM command
	line, e.g.

		abase::strtok tok(servers.c_str(), ";,\\r\\n");
		const char * token;
		while ((token = tok.token()))
		{
			if (!*token) continue;
			...
		}

	so token() has to hand out one field per delimiter separated slot -
	including empty ones (callers skip them themselves) - and NULL once the
	source string is exhausted.  token() copies into the caller supplied
	buffer and never runs past its end; org_token() reuses the source string
	by writing a 0 over the delimiter, exactly as the header warns.
*/

#include "strtok.h"
#include "ASSERT.h"

#include <string.h>

namespace abase
{

strtok::strtok(const char * src, const char * delim)
{
	reset(src, delim);
}

int strtok::reset(const char * src, const char * delim)
{
	if (!src)
	{
		_srcstr = NULL;
		_last = NULL;
		_delim = delim ? delim : "";
		return -1;
	}
	_srcstr = src;
	_delim = delim ? delim : "";
	_last = src;
	return 0;
}

const char * strtok::token(char * output, int len)
{
	if (!output || len <= 0) return NULL;
	*output = 0;
	if (!_last || !_srcstr) return NULL;

	const char * begin = _last;
	if (!*begin)
	{
		_last = NULL;			/* consumed - offset() reports -1 */
		return NULL;
	}

	const size_t dlen = strlen(_delim);
	const char * p = begin;
	if (dlen)
	{
		for (;;)
		{
			if (*p == 0) break;
			if (dlen == 1)
			{
				if (*p == _delim[0]) break;
			}
			else if (memchr(_delim, *p, dlen))
			{
				break;
			}
			++p;
		}
	}

	size_t n = (size_t)(p - begin);
	if (n > (size_t)(len - 1)) n = (size_t)(len - 1);
	memcpy(output, begin, n);
	output[n] = 0;

	_last = (*p) ? p + 1 : NULL;
	return output;
}

char * strtok::org_token()
{
	if (!_last || !_srcstr) return NULL;

	char * begin = const_cast<char *>(_last);
	if (!*begin)
	{
		_last = NULL;
		return NULL;
	}

	const size_t dlen = strlen(_delim);
	char * p = begin;
	if (dlen)
	{
		for (;;)
		{
			if (*p == 0) break;
			if (dlen == 1)
			{
				if (*p == _delim[0]) break;
			}
			else if (memchr(_delim, *p, dlen))
			{
				break;
			}
			++p;
		}
	}

	if (*p)
	{
		*p = 0;
		_last = p + 1;
	}
	else
	{
		_last = NULL;
	}
	return begin;
}

}
