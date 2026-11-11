/* config.h for the HOST tools of the Amiga builds (ctpdb, run on Linux by
 * ctp2_code/os/amiga/gen.mk). The values configure would write on Linux. */
#ifndef CTP2_AMIGA_HOST_CONFIG_H
#define CTP2_AMIGA_HOST_CONFIG_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_UNISTD_H 1
#define HAVE_LIMITS_H 1
#define LINUX 1
#define FILE_SEP "/"
#define FILE_SEPC '/'
#define PATH_SEP ":"
#define PATH_SEPC ':'
#include <limits.h>
#define MAX_PATH PATH_MAX
#define _MAX_PATH 260
#define USE_COM_REPLACEMENT 1
#ifdef __cplusplus
#include <unistd.h>
#endif
#endif
