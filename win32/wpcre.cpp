/*
 * wpcre.cpp - local-only stand-in implementation for the small PCRE surface
 * the server uses.  Compiled (and linked) only by the zig cross toolchain;
 * the native MSYS2 build links the real PCRE library instead.
 */
#include <pcre.h>
#include <regex>
#include <string>
#include <new>
#include <stdlib.h>

struct real_pcre
{
	std::regex re;
	bool ok;
	real_pcre(const std::string &pat, bool icase)
		: re(pat, std::regex::ECMAScript |
		          (icase ? std::regex::icase : std::regex_constants::ECMAScript)),
		  ok(true)
	{
	}
};

void *(*pcre_malloc)(size_t) = NULL;
void (*pcre_free)(void *) = NULL;

extern "C" {

pcre *pcre_compile(const char *pattern, int options, const char **errptr,
                   int *erroffset, const unsigned char *tableptr)
{
	(void)tableptr;
	if (!pattern)
	{
		if (errptr) *errptr = "NULL pattern";
		return NULL;
	}
	bool icase = (options & PCRE_CASELESS) != 0;
	try
	{
		void *mem = malloc(sizeof(real_pcre));
		if (!mem)
			return NULL;
		return new (mem) real_pcre(std::string(pattern), icase);
	}
	catch (const std::regex_error &)
	{
		if (errptr) *errptr = "regular expression error";
		if (erroffset) *erroffset = 0;
		return NULL;
	}
	catch (...)
	{
		if (errptr) *errptr = "out of memory";
		return NULL;
	}
}

int pcre_exec(const pcre *code, const pcre_extra *extra, const char *subject,
              int length, int startoffset, int options, int *ovector,
              int ovecsize)
{
	(void)extra;
	(void)options;
	(void)ovector;
	(void)ovecsize;
	if (!code || !subject || length < 0)
		return PCRE_ERROR_NOMATCH;
	if (startoffset < 0)
		startoffset = 0;
	if (startoffset > length)
		startoffset = length;
	try
	{
		std::string s(subject + startoffset, (size_t)(length - startoffset));
		if (std::regex_search(s, code->re))
			return 0;
	}
	catch (...)
	{
	}
	return PCRE_ERROR_NOMATCH;
}

} /* extern "C" */

/* pcre 8.x header declares pcre_free as a global function pointer defaulting
 * to free(); mirror that (matcher calls pcre_free(regexp)). */
namespace
{
	struct PcreHookInit
	{
		PcreHookInit()
		{
			if (!pcre_free)
				pcre_free = &free;
			if (!pcre_malloc)
				pcre_malloc = &malloc;
		}
	};
}

extern "C" void wp_pcre_hook_init();

void wp_pcre_hook_init()
{
	static PcreHookInit init;
	(void)init;
}
