/*
 * wsyslog.cpp - minimal <syslog.h> implementation for Windows.
 *
 * The original code delivers log records to /dev/log (a Unix datagram
 * socket).  That socket does not exist on Windows, so this implementation
 * routes openlog/syslog output to stderr (and optionally to a file given in
 * the WP_SYSLOG_FILE environment variable).
 */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

extern "C" {

static const char *g_ident = "server";
static int g_option = 0;
static int g_facility = (1 << 3);   /* LOG_USER */
static FILE *g_file = NULL;
static bool g_file_failed = false;

void openlog(const char *ident, int option, int facility)
{
	if (ident)
		g_ident = ident;
	g_option = option;
	g_facility = facility & ~7;

	const char *path = getenv("WP_SYSLOG_FILE");
	if (path && *path)
	{
		if (g_file)
			fclose(g_file);
		g_file = fopen(path, "a");
		g_file_failed = (g_file == NULL);
	}
	else if (g_file)
	{
		fclose(g_file);
		g_file = NULL;
	}
}

void closelog(void)
{
	if (g_file)
	{
		fclose(g_file);
		g_file = NULL;
	}
}

void vsyslog(int priority, const char *format, va_list ap)
{
	char msg[2048];
	vsnprintf(msg, sizeof(msg), format, ap);

	/* compute syslog-style prefix */
	char line[2304];
	int pri = priority & 7;
	int fac = (priority & ~7) ? (priority & ~7) : g_facility;
	SYSTEMTIME st;
	GetLocalTime(&st);

	int n = snprintf(line, sizeof(line),
	                 "<%d>%s %02u %02u:%02u:%02u %s[%lu]: %s\n",
	                 fac | pri, g_ident,
	                 st.wMonth, st.wHour, st.wMinute, st.wSecond,
	                 g_ident, GetCurrentProcessId(), msg);
	if (n < 0)
		return;
	if (n >= (int)sizeof(line))
		n = (int)sizeof(line) - 1;

	FILE *out = g_file ? g_file : stderr;
	if (out)
	{
		fwrite(line, 1, (size_t)n, out);
		fflush(out);
	}
}

void syslog(int priority, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	vsyslog(priority, format, ap);
	va_end(ap);
}

} /* extern "C" */
