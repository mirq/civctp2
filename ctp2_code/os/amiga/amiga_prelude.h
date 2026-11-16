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

#if defined(CTP2_AMIGA_AROS) && defined(__cplusplus)
/* AROS's libstdc++ is built with threads: <string> and the others include
 * <pthread.h>, which includes exec/types.h and exec/semaphores.h (lists,
 * nodes, ports, tasks). Their BYTE, WORD and BOOL differ from
 * os/nowin32/windows.h's, struct Message, List and Node clash with the
 * game's, and the list macros (GetHead, GetTail, REMOVE...) with its
 * methods. It is included here first with those names renamed, the macros
 * dropped; the game never uses exec. The platform layer is C
 * (os/amiga/*.c, amiga_os.h) and gets exec as it is. */
#define BYTE    ctp2_exec_BYTE
#define WORD    ctp2_exec_WORD
#define BOOL    ctp2_exec_BOOL
#define Message ctp2_exec_Message
#define List    ctp2_exec_List
#define Node    ctp2_exec_Node
/* Its sched.h leaves struct sched_param out when __AMIGA__ is defined (as
 * common.mk does for the game), and pthread.h needs it. */
#pragma push_macro("__AMIGA__")
#undef __AMIGA__
#include <pthread.h>
#pragma pop_macro("__AMIGA__")
#undef BYTE
#undef WORD
#undef BOOL
#undef Message
#undef List
#undef Node
#undef GetHead
#undef GetTail
#undef GetSucc
#undef GetPred
#undef REMOVE
#undef REMHEAD
#undef REMTAIL
#undef ADDHEAD
#undef ADDTAIL
#undef NEWLIST
#undef IsListEmpty
#undef IsMinListEmpty
#undef IsMsgPortEmpty
#undef ListLength
#undef ForeachNode
#undef ForeachNodeSafe
#undef SetNodeName
#undef GetNodeName
#undef CONST
#undef STATIC
#undef GLOBAL
#undef IMPORT
#undef REGISTER
#undef VOLATILE
#undef STACKED
#undef VOID
#undef TRUE   /* 1L, 0L: windows.h has its own */
#undef FALSE
#endif
#if defined(CTP2_AMIGA_AROS)
/* PRId64 and the like in C++ too. */
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
