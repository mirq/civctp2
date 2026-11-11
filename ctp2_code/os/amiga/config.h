/* config.h for the Amiga builds (Makefile.amiga68k, Makefile.warpos).
 *
 * Stands in for the header configure writes (configure.ac): the same names,
 * with the values for AmigaOS. Both CPUs are big-endian. Not defined here, on
 * purpose: USE_SDL (the Amiga backend is ui/aui_amiga, selected by
 * __AUI_USE_AMIGA__ in ui/aui_common/auicfg.h) and LINUX (the Linux-only
 * paths: case emulation of file names, /proc, the XDG home directory).
 */
#ifndef CTP2_AMIGA_CONFIG_H
#define CTP2_AMIGA_CONFIG_H 1

/* CTP2_AMIGA_BUILD, CTP2_AMIGA_060 or CTP2_AMIGA_WOS come from the makefile. */
#if !defined(CTP2_AMIGA_BUILD)
#error "os/amiga/config.h is for the Amiga builds only"
#endif

#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_UNISTD_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1

#define WORDS_BIGENDIAN 1

/* No COM: the map generators and the other "COM" objects are plain C++. */
#define USE_COM_REPLACEMENT 1

/* AmigaDOS: '/' separates directories and ':' ends a volume name, so a list
 * of search paths cannot be separated by ':' (ctp/civapp.cpp splits on
 * PATH_SEPC). */
#define FILE_SEP "/"
#define FILE_SEPC '/'
#define PATH_SEP ";"
#define PATH_SEPC ';'
/* 260, as on Linux and Windows: the size is part of the save game format. */
#define _MAX_PATH 260

#define AUTOMAKE_INVOKES_YACC 1
#define YYDEBUG 9
#define _TIFF_DATA_TYPEDEFS_ 1
#define CTP2_ENABLE_SLICDEBUG 1
#if !defined(_DEBUG)
#define _BFR_ 1
#endif

/* The game's own drawer holds everything: data, saves, logs, settings. */
#define PACKAGE_DATADIR "PROGDIR:"
#define PACKAGE_SYSCONFDIR "PROGDIR:"

#endif /* CTP2_AMIGA_CONFIG_H */
