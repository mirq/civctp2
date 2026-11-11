/*
 * File names for the Amiga builds (amiga_platform.h, ctp/cifm.h).
 *
 * The game builds POSIX-style relative paths ("ctp2_data/default/gamedata",
 * "./save/", "../Scenarios"). AmigaDOS has no "." (it is a file name), reads
 * "//" as the parent of the parent and writes the parent as an empty part
 * ("/x"). Every name the game opens goes through CI_FixName here (c3.h sends
 * fopen through ci_fopen), as the Linux build sends it through its case-
 * insensitive search: "." and empty parts are dropped, "a/.." resolved, a
 * leading ".." becomes "/", and a volume ("Work:") stays. Case needs nothing:
 * AmigaDOS file systems ignore it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include "amiga_os.h"
#include "amiga_platform.h"
#include "cifm.h"
#include <dirent.h>     /* os/amiga/include: ctp2_opendir & co. */

#define FIX_BUFFERS  8
#define FIX_SIZE     1024
#define MAX_PARTS    64

const char *amiga_fix_path(const char *path)
{
	static char buffers[FIX_BUFFERS][FIX_SIZE];
	static int  next;

	char *       out;
	char         work[FIX_SIZE];
	const char * parts[MAX_PARTS];
	int          count = 0;
	size_t       o = 0;
	char *       p;
	char *       rest;
	int          i;

	if (!path) return NULL;

	out  = buffers[next];
	next = (next + 1) % FIX_BUFFERS;

	strncpy(work, path, FIX_SIZE - 1);
	work[FIX_SIZE - 1] = 0;

	/* Windows separators in the game's data ("dll\map\geometric.dll"). */
	for (p = work; *p; ++p)
	{
		if (*p == '\\') *p = '/';
	}

	/* A volume or assign keeps its place: "PROGDIR:", "Work:ctp2". One after
	 * a separator is a full name joined onto a directory by the game
	 * ("dir/Work:x"): said in the log, it would name a volume that is not
	 * there. */
	rest = strchr(work, ':');
	if (rest && memchr(work, '/', (size_t)(rest - work)))
	{
		static int s_reported;
		if (s_reported < 20)
		{
			++s_reported;
			amiga_log("Path with a volume inside: %s\n", path);
		}
	}
	if (rest)
	{
		size_t const n = (size_t)(rest - work) + 1;
		memcpy(out, work, n);
		o = n;
		++rest;
	}
	else
	{
		rest = work;
	}

	/* Leading slashes are AmigaDOS parents, as this function writes them
	 * ("../x" -> "/x"), so a fixed name fixed again stays the same: fopen
	 * fixes every name (__wrap_fopen) and some come already fixed. (The game
	 * makes no POSIX absolute paths on the Amiga.) */
	if (o == 0)
	{
		while (*rest == '/' && count < MAX_PARTS)
		{
			parts[count++] = "..";
			++rest;
		}
	}
	else
	{
		while (*rest == '/') ++rest;
	}

	for (p = strtok(rest, "/"); p; p = strtok(NULL, "/"))
	{
		if (p[0] == 0 || strcmp(p, ".") == 0)
		{
			continue;
		}
		if (strcmp(p, "..") == 0)
		{
			if (count > 0 && strcmp(parts[count - 1], "..") != 0)
			{
				--count;
				continue;
			}
		}
		if (count < MAX_PARTS) parts[count++] = p;
	}

	for (i = 0; i < count; ++i)
	{
		const char * part = parts[i];
		size_t       len;

		if (strcmp(part, "..") == 0)
		{
			/* The parent: an empty part, "/". */
			if (o + 1 < FIX_SIZE) out[o++] = '/';
			continue;
		}

		len = strlen(part);
		if (o + len + 1 >= FIX_SIZE) break;
		memcpy(out + o, part, len);
		o += len;
		if (i + 1 < count && o + 1 < FIX_SIZE) out[o++] = '/';
	}

	out[o] = 0;
	return out;
}

int amiga_fullpath(const char *path, char *out, size_t size)
{
	char   joined[FIX_SIZE];
	BPTR   lock;
	size_t n;

	if (!out || size == 0) return 0;
	if (!path) path = "";

	if (strchr(path, ':'))
	{
		strncpy(out, amiga_fix_path(path), size - 1);
		out[size - 1] = 0;
		return 1;
	}

	/* The current directory (the game's drawer) by its full name. */
	lock = Lock((CONST_STRPTR)"", ACCESS_READ);
	if (!lock) return 0;
	if (!NameFromLock(lock, (STRPTR)joined, (LONG)sizeof(joined)))
	{
		UnLock(lock);
		return 0;
	}
	UnLock(lock);

	/* Joined in POSIX form and fixed once, so the ".." parts resolve against
	 * the drawer's name. Not AddPart: it writes the parent as an empty part
	 * ("ctp///data"), which amiga_fix_path would later drop as ".". */
	n = strlen(joined);
	if (n + 1 < sizeof(joined))
	{
		joined[n++] = '/';
		strncpy(joined + n, path, sizeof(joined) - n - 1);
		joined[sizeof(joined) - 1] = 0;
	}

	strncpy(out, amiga_fix_path(joined), size - 1);
	out[size - 1] = 0;
	return 1;
}

/* ---- ctp/cifm.h: what the Linux build's case-insensitive layer offers ------ */

const char *CI_FixName(const char *name)
{
	return amiga_fix_path(name);
}

/* Every fopen of the program lands here (linked with --wrap=fopen): c3.h
 * sends fopen to ci_fopen with a macro, but <cstdio> #undefs it, so most C++
 * files and the C ones (the SLIC lexer, FreeType) call fopen itself. */
FILE *__real_fopen(const char *name, const char *mode);

/* WarpOS: newlib refills and writes a stream 1 KB at a time, and each of
 * those is a dos.library call, a switch to the 68k and back. A larger buffer
 * (set before the first read or write, so newlib allocates it) means fewer. */
static FILE *open_stream(const char *name, const char *mode)
{
	FILE * f = __real_fopen(name, mode);
#if defined(CTP2_AMIGA_WOS)
	if (f) setvbuf(f, NULL, _IOFBF, 32768);
#endif
	return f;
}

FILE *__wrap_fopen(const char *name, const char *mode)
{
	return open_stream(name ? amiga_fix_path(name) : NULL, mode);
}

FILE *ci_fopen(const char *name, const char *mode)
{
	return open_stream(amiga_fix_path(name), mode);
}

int CI_FileExists(const char *name)
{
	BPTR lock = Lock((CONST_STRPTR)amiga_fix_path(name), ACCESS_READ);
	if (lock)
	{
		UnLock(lock);
		return 1;
	}
	return 0;
}

/* glibc's fcloseall: the game calls it before exit() after a bad sprite file;
 * exit() closes the streams itself, so flushing them is enough. */
int fcloseall(void)
{
	fflush(NULL);
	return 0;
}

#if defined(CTP2_AMIGA_WOS)
/* POSIX basename (libgen.h, which os/nowin32 uses), which the WarpOS newlib
 * lacks: the last part of a path, "/" and ":" both separating. Defined under
 * another C name and given the symbol "basename": with _GNU_SOURCE (the
 * prelude) string.h declares GNU's basename, of another type. */
char *ctp2_posix_basename(char *path) __asm__("basename");

char *ctp2_posix_basename(char *path)
{
	char * last = path;
	char * p;

	if (!path || !*path) return (char *)".";

	for (p = path; *p; ++p)
	{
		if ((*p == '/' || *p == ':') && p[1]) last = p + 1;
	}
	return last;
}
#endif

/* ---- stat, directories: dos.library ------------------------------------------ */

/* fib_Date counts from 1978-01-01, 252460800 s after the Unix epoch. */
static long amiga_date_to_unix(const struct DateStamp *ds)
{
	return 252460800L + ds->ds_Days * 86400L + ds->ds_Minute * 60L + ds->ds_Tick / TICKS_PER_SECOND;
}

int ctp2_stat(const char *path, struct stat *buf)
{
	BPTR                   lock;
	struct FileInfoBlock * fib;
	int                    ok = 0;

	lock = Lock((CONST_STRPTR)amiga_fix_path(path), ACCESS_READ);
	if (!lock)
	{
		errno = ENOENT;
		return -1;
	}

	/* AllocDosObject: a FileInfoBlock must be longword aligned. */
	fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
	if (fib && Examine(lock, fib))
	{
		memset(buf, 0, sizeof(*buf));
		buf->st_mode  = (fib->fib_DirEntryType >= 0) ? (S_IFDIR | 0755) : (S_IFREG | 0644);
		buf->st_size  = fib->fib_Size;
		buf->st_mtime = amiga_date_to_unix(&fib->fib_Date);
		buf->st_atime = buf->st_mtime;
		buf->st_ctime = buf->st_mtime;
		buf->st_nlink = 1;
		ok = 1;
	}

	if (fib) FreeDosObject(DOS_FIB, fib);
	UnLock(lock);

	if (!ok)
	{
		errno = EIO;
		return -1;
	}
	return 0;
}

struct ctp2_dir
{
	BPTR                   lock;
	struct FileInfoBlock * fib;
	struct dirent          entry;
};

DIR *ctp2_opendir(const char *path)
{
	DIR * dir = (DIR *)calloc(1, sizeof(DIR));
	if (!dir) return NULL;

	dir->lock = Lock((CONST_STRPTR)amiga_fix_path(path), ACCESS_READ);
	dir->fib  = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);

	if (!dir->lock || !dir->fib || !Examine(dir->lock, dir->fib) || dir->fib->fib_DirEntryType < 0)
	{
		ctp2_closedir(dir);
		errno = ENOENT;
		return NULL;
	}

	return dir;
}

struct dirent *ctp2_readdir(DIR *dir)
{
	if (!dir || !ExNext(dir->lock, dir->fib)) return NULL;

	strncpy(dir->entry.d_name, (const char *)dir->fib->fib_FileName, sizeof(dir->entry.d_name) - 1);
	dir->entry.d_name[sizeof(dir->entry.d_name) - 1] = 0;
	return &dir->entry;
}

int ctp2_closedir(DIR *dir)
{
	if (!dir) return -1;
	if (dir->fib)  FreeDosObject(DOS_FIB, dir->fib);
	if (dir->lock) UnLock(dir->lock);
	free(dir);
	return 0;
}
