/*
	verbose.cpp - tiny runtime verbosity switch of the cmlib (abase) support
	library.

	gameserver never calls verbose()/verbosef() itself; it only tests the
	global `glb_verbose` through the __PRINT* macros in cgame/include, so the
	only hard requirement is that glb_verbose exists and that turning it on
	makes the library print somewhere sane.

	The level filter follows the enums in verbose.h: glb_verbose holds the
	current level, verbose()/verbosef() print when the requested level passes
	the comparison set by set_verbose_level(), and set_verbose_mode() selects
	stdout / a file / nothing at all.
*/

#include "verbose.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

int glb_verbose = 0;			/* 0 = quiet, that is the default */

static int glb_verbose_level = 0;
static int glb_verbose_level_mode = VERBOSE_LEVEL_ALL;
static int glb_verbose_mode = VERBOSE_NORMAL;
static FILE * glb_verbose_file = NULL;

static int verbose_accept(int lvl)
{
	switch (glb_verbose_level_mode)
	{
	case VERBOSE_LEVEL_HIGHER:	return lvl > glb_verbose_level;
	case VERBOSE_LEVEL_LOWER:	return lvl < glb_verbose_level;
	case VERBOSE_LEVEL_EQUAL:	return lvl == glb_verbose_level;
	case VERBOSE_LEVEL_ALL:
	default:			return lvl <= glb_verbose;
	}
}

static FILE * verbose_output()
{
	switch (glb_verbose_mode)
	{
	case VERBOSE_NULL:
		return NULL;
	case VERBOSE_FILE:
		return glb_verbose_file;
	case VERBOSE_NORMAL:
	default:
		return stdout;
	}
}

extern "C"
int verbose(int lvl, const char * data)
{
	if (!verbose_accept(lvl)) return 0;
	FILE * out = verbose_output();
	if (!out || !data) return 0;
	fputs(data, out);
	if (*data && data[strlen(data) - 1] != '\n') fputc('\n', out);
	fflush(out);
	return (int)strlen(data);
}

extern "C"
int verbosef(int lvl, const char * format, ...)
{
	if (!verbose_accept(lvl)) return 0;
	FILE * out = verbose_output();
	if (!out || !format) return 0;
	va_list ap;
	va_start(ap, format);
	int rst = vfprintf(out, format, ap);
	va_end(ap);
	fflush(out);
	return rst;
}

/*
	mode == VERBOSE_FILE: data is the file name to append to.
	mode == VERBOSE_NULL: silence.  VERBOSE_NORMAL: stdout.
*/
extern "C"
int set_verbose_mode(enum VERBOSE_MODE mode, char * data)
{
	if (glb_verbose_file && glb_verbose_file != stdout && glb_verbose_file != stderr)
	{
		fclose(glb_verbose_file);
		glb_verbose_file = NULL;
	}
	glb_verbose_mode = (int)mode;
	switch (mode)
	{
	case VERBOSE_FILE:
		if (!data || !*data) return -1;
		glb_verbose_file = fopen(data, "a");
		if (!glb_verbose_file) return -1;
		break;
	case VERBOSE_NULL:
	case VERBOSE_NORMAL:
	default:
		break;
	}
	return 0;
}

extern "C"
int set_verbose_level(enum VERBOSE_LEVEL_MODE mode, int level)
{
	glb_verbose_level_mode = (int)mode;
	glb_verbose_level = level;
	return 0;
}
