/*
 * Every AmigaOS header the platform layer (ctp2_code/os/amiga/*.c) uses, for
 * both builds. Only those files include it; the game sees amiga_platform.h.
 *
 * The PPC lays out structures with 4-byte alignment, the OS with the 68k's 2:
 * on WarpOS every OS header goes in under #pragma pack(2), or a structure the
 * OS fills is read at the wrong offsets (a 24-byte struct Message instead of 20).
 * The 68060 build has the same layout either way; the pragma does no harm.
 */
#ifndef CTP2_AMIGA_OS_H
#define CTP2_AMIGA_OS_H 1

#pragma pack(push, 2)

#if defined(CTP2_AMIGA_MOS)
#undef PROTO_EXEC_H /* set by amiga_prelude.h, to keep unistd.h off exec */
#endif

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfx.h>
#include <graphics/displayinfo.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <libraries/locale.h>
#include <utility/hooks.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/keymap.h>
#include <proto/locale.h>

#if defined(CTP2_AMIGA_WOS)
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <devices/ahi.h>
#include <proto/ahi.h>
#include <powerpc/powerpc.h>
#include <powerpc/powerpc_protos.h>
#elif defined(CTP2_AMIGA_MOS)
#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <devices/ahi.h>
#include <proto/ahi.h>
#include <proto/timer.h>
#include <emul/emulinterface.h>
#include <emul/emulregs.h>
#else
#include <proto/timer.h>
#include "amiga_cgx68k.h"
#endif

#pragma pack(pop)

/* The OS headers' macros that clash with the game's names stop here. */
#undef Remove
#undef Wait

/* Library bases this layer opens itself (amiga_platform.c): the startup code
 * provides only SysBase and DOSBase. */
extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase       *GfxBase;
extern struct Library       *CyberGfxBase;
extern struct Library       *KeymapBase;
#if defined(CTP2_AMIGA_MOS)
extern struct Library       *LocaleBase;  /* MorphOS's proto/locale.h */
#else
extern struct LocaleBase    *LocaleBase;
#endif
extern struct Library       *AHIBase;

/* The CPU differences below the OS calls (as OpenSWOS's rtg_clock.c):
 * WarpOS: the 68k does not snoop the PPC's data cache, so memory the 68k reads
 * (AHI's ring, the hook's word) is written back line by line; the 68060: one
 * processor, only the compiler is told. MorphOS: one PowerPC, AHI and the
 * OS run on it natively; nothing to flush either. */
static inline void amiga_cache_flush(const void *addr, unsigned long len)
{
#if defined(CTP2_AMIGA_WOS)
	unsigned long a   = (unsigned long)addr & ~31UL;
	unsigned long end = ((unsigned long)addr + len + 31) & ~31UL;
	for (; a < end; a += 32)
		__asm__ __volatile__ ("dcbf 0,%0" : : "r" (a) : "memory");
	__asm__ __volatile__ ("sync" ::: "memory");
#else
	(void)addr; (void)len;
	__asm__ __volatile__ ("" ::: "memory");
#endif
}

#endif
