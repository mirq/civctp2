/*
 * Directory listings for the Amiga builds, on dos.library (Lock, Examine,
 * ExNext): newlib's opendir/readdir fault under MOS2WOS on WarpOS, and the
 * game's POSIX-style paths go through the same AmigaDOS name fixing as its
 * files (os/amiga/amiga_posix.c). Comes before the system's on the include
 * path. The game reads only d_name.
 */
#ifndef CTP2_AMIGA_DIRENT_H
#define CTP2_AMIGA_DIRENT_H 1

#ifdef __cplusplus
extern "C" {
#endif

struct dirent
{
	char d_name[108];
};

typedef struct ctp2_dir DIR;

DIR *           ctp2_opendir(const char *path);
struct dirent * ctp2_readdir(DIR *dir);
int             ctp2_closedir(DIR *dir);

#ifdef __cplusplus
}
#endif

#define opendir  ctp2_opendir
#define readdir  ctp2_readdir
#define closedir ctp2_closedir

#endif
