/*
 * dlopen for the Amiga builds: the map generator plugins (ctp2_code/mapgen)
 * are linked into the executable, and dlopen finds them by name
 * (os/amiga/amiga_dl.c). This header comes before the system's on the include
 * path (libnix's dlfcn.h is for libraries built with -shared).
 */
#ifndef CTP2_AMIGA_DLFCN_H
#define CTP2_AMIGA_DLFCN_H 1

#define RTLD_LAZY   0
#define RTLD_NOW    1
#define RTLD_GLOBAL 2
#define RTLD_LOCAL  4

#ifdef __cplusplus
extern "C" {
#endif

void *       ctp2_dlopen(const char *file, int mode);
void *       ctp2_dlsym(void *handle, const char *name);
int          ctp2_dlclose(void *handle);
const char * ctp2_dlerror(void);

#ifdef __cplusplus
}
#endif

#define dlopen  ctp2_dlopen
#define dlsym   ctp2_dlsym
#define dlclose ctp2_dlclose
#define dlerror ctp2_dlerror

#endif
