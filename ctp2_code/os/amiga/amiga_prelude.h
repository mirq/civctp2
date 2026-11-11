/*
 * Included before every source of the Amiga builds (-include, see
 * os/amiga/common.mk): what libnix and Bebbo's libstdc++ lack that the game
 * takes from glibc.
 */
#ifndef CTP2_AMIGA_PRELUDE_H
#define CTP2_AMIGA_PRELUDE_H 1

/* glibc's names in newlib-style headers (fcloseall, strndup, strcasestr). */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#if defined(CTP2_AMIGA_MOS)
/* MorphOS's libnix unistd.h includes proto/exec.h, proto/socket.h and
 * proto/usergroup.h, whose AmigaOS types (BYTE, WORD, LONG, struct Unit) clash
 * with os/nowin32/windows.h and the game's own. The game needs none of them:
 * their guards are set here, and amiga_os.h (the platform layer, which wants
 * exec) clears the first again. */
#define PROTO_EXEC_H 1
#define PROTO_SOCKET_H 1
#define PROTO_USERGROUP_H 1
/* The same for sys/param.h's libraries/usergroup.h, and sys/select.h's inline
 * select() over bsdsocket's WaitSelect (the game opens no sockets here). */
#define _LIBRARIES_USERGROUP_H_ 1
#define _SELECT_DECLARED 1
/* PRId64 and the like in C++ too (the SDK's inttypes.h asks for this). */
#define __STDC_FORMAT_MACROS 1
#endif

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Closes every open stdio stream (glibc); os/amiga/amiga_posix.c. */
int fcloseall(void);

/* stat() on dos.library, through the AmigaDOS name fixing
 * (os/amiga/amiga_posix.c): newlib's faults under MOS2WOS. A function-like
 * macro, so "struct stat" is left alone. */
struct stat;
int ctp2_stat(const char *path, struct stat *buf);

#ifdef __cplusplus
}
#endif

#include <sys/types.h>
#include <sys/stat.h>
#define stat(path, buf) ctp2_stat(path, buf)

#ifdef __cplusplus
#if !defined(CTP2_AMIGA_WOS)
/* Bebbo's libstdc++ was built without C99 math in namespace std; the
 * functions themselves are in libm. */
#include <cmath>
#include <math.h>
namespace std
{
	using ::round;
	using ::lround;
	using ::trunc;
}
#endif
#if defined(CTP2_AMIGA_MOS)
/* std::numeric_limits, which the other libstdc++ builds bring in with <cmath>. */
#include <limits>
#endif
#endif

#endif
