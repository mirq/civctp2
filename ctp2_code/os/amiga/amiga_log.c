/*
 * The log (amiga_platform.h): PROGDIR:ctp2.log, collected in memory and
 * written every 2 seconds and at exit. A line per write straight to disk cost
 * tens of milliseconds a frame on these machines (OpenSWOS docs/WARPOS.md).
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "amiga_os.h"
#include "amiga_platform.h"

#define LOG_SIZE     32768
#define LOG_PERIOD   2000   /* ms */

static char     s_buf[LOG_SIZE];
static size_t   s_len;
static uint32_t s_lastFlush;
static int      s_started;
static int      s_sync = -1;   /* ENV:CTP2_LOGSYNC, read at the first line */

void amiga_log_flush(void)
{
	BPTR fh;

	if (s_len == 0) return;

	/* The first write of a run replaces the old log. */
	fh = Open((CONST_STRPTR)"PROGDIR:ctp2.log", s_started ? MODE_OLDFILE : MODE_NEWFILE);
	if (!fh && s_started) fh = Open((CONST_STRPTR)"PROGDIR:ctp2.log", MODE_NEWFILE);
	if (fh)
	{
		Seek(fh, 0, OFFSET_END);
		Write(fh, s_buf, (LONG)s_len);
		Close(fh);
		s_started = 1;
	}

	s_len       = 0;
	s_lastFlush = amiga_ticks_ms();
}

void amiga_log(const char *fmt, ...)
{
	char    line[512];
	va_list args;
	int     n;
	uint32_t now = amiga_ticks_ms();

	n = snprintf(line, sizeof(line), "[%6u.%03u] ", (unsigned)(now / 1000), (unsigned)(now % 1000));
	va_start(args, fmt);
	vsnprintf(line + n, sizeof(line) - n, fmt, args);
	va_end(args);

	n = (int)strlen(line);
	if (s_len + n > LOG_SIZE) amiga_log_flush();
	if ((size_t)n > LOG_SIZE) n = LOG_SIZE;
	memcpy(s_buf + s_len, line, n);
	s_len += n;

	/* ENV:CTP2_LOGSYNC: every line to disk at once, so a hang loses none. */
	if (s_sync < 0)
	{
		char env[8];
		s_sync = (GetVar((CONST_STRPTR)"CTP2_LOGSYNC", (STRPTR)env, sizeof(env), GVF_GLOBAL_ONLY) > 0) ? 1 : 0;
	}

	if (s_sync || now - s_lastFlush >= LOG_PERIOD) amiga_log_flush();
}

void amiga_log_exit(void)
{
	amiga_log("Exit\n");
	amiga_log_flush();
}
