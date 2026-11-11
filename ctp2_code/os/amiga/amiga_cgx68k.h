/*
 * The part of cybergraphics.library and ahi.device the 68060 build calls, for
 * Bebbo's m68k-amigaos-gcc, whose NDK has neither CyberGraphX's headers nor a
 * proto/ header for AHI. The WarpOS build includes the SDK's own headers.
 *
 * Tag and format values: CyberGraphX's published interface (cybergraphics.h
 * 41.x); offsets: cybergraphics_lib.fd (bias 30). AHI's inline macros are in
 * the NDK, under defines/. (As OpenSWOS's include/rtg_cgx68k.h.)
 */
#ifndef CTP2_AMIGA_CGX68K_H
#define CTP2_AMIGA_CGX68K_H 1

#include <exec/types.h>
#include <exec/nodes.h>
#include <utility/tagitem.h>
#include <graphics/displayinfo.h>
#include <graphics/rastport.h>

#define CYBERGFXNAME            "cybergraphics.library"

struct CyberModeNode
{
	struct Node     Node;
	char            ModeText[DISPLAYNAMELEN];
	ULONG           DisplayID;
	UWORD           Width;
	UWORD           Height;
	UWORD           Depth;
	struct TagItem *DisplayTagList;
};

#define CYBRMATTR_PIXFMT        (0x80000004)

#define CYBRIDATTR_PIXFMT       (0x80000001)
#define CYBRIDATTR_WIDTH        (0x80000002)
#define CYBRIDATTR_HEIGHT       (0x80000003)
#define CYBRIDATTR_DEPTH        (0x80000004)
#define CYBRIDATTR_BPPIX        (0x80000005)

#define CYBRMREQ_TB             (TAG_USER + 0x40000)
#define CYBRMREQ_MinDepth       (CYBRMREQ_TB + 0)
#define CYBRMREQ_MaxDepth       (CYBRMREQ_TB + 1)
#define CYBRMREQ_MinWidth       (CYBRMREQ_TB + 2)
#define CYBRMREQ_MaxWidth       (CYBRMREQ_TB + 3)
#define CYBRMREQ_MinHeight      (CYBRMREQ_TB + 4)
#define CYBRMREQ_MaxHeight      (CYBRMREQ_TB + 5)

#define CYBRBIDTG_TB            (TAG_USER + 0x50000)
#define CYBRBIDTG_Depth         (CYBRBIDTG_TB + 0)
#define CYBRBIDTG_NominalWidth  (CYBRBIDTG_TB + 1)
#define CYBRBIDTG_NominalHeight (CYBRBIDTG_TB + 2)

#define PIXFMT_LUT8             (0UL)
#define PIXFMT_RGB15            (1UL)
#define PIXFMT_BGR15            (2UL)
#define PIXFMT_RGB15PC          (3UL)
#define PIXFMT_BGR15PC          (4UL)
#define PIXFMT_RGB16            (5UL)
#define PIXFMT_BGR16            (6UL)
#define PIXFMT_RGB16PC          (7UL)
#define PIXFMT_BGR16PC          (8UL)

#define RECTFMT_RGB             (0UL)

#define LBMI_WIDTH              (0x84001001)
#define LBMI_HEIGHT             (0x84001002)
#define LBMI_DEPTH              (0x84001003)
#define LBMI_PIXFMT             (0x84001004)
#define LBMI_BYTESPERPIX        (0x84001005)
#define LBMI_BYTESPERROW        (0x84001006)
#define LBMI_BASEADDRESS        (0x84001007)

extern struct Library *CyberGfxBase;

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif

#define IsCyberModeID(___displayID) \
	LP1(0x36, BOOL, IsCyberModeID, ULONG, ___displayID, d0, \
	    , CyberGfxBase)

#define BestCModeIDTagList(___tags) \
	LP1(0x3c, ULONG, BestCModeIDTagList, struct TagItem *, ___tags, a0, \
	    , CyberGfxBase)

#define AllocCModeListTagList(___tags) \
	LP1(0x48, struct List *, AllocCModeListTagList, struct TagItem *, ___tags, a1, \
	    , CyberGfxBase)

#define FreeCModeList(___list) \
	LP1NR(0x4e, FreeCModeList, struct List *, ___list, a0, \
	      , CyberGfxBase)

#define GetCyberIDAttr(___attr, ___id) \
	LP2(0x66, ULONG, GetCyberIDAttr, ULONG, ___attr, d0, ULONG, ___id, d1, \
	    , CyberGfxBase)

#define WritePixelArray(___src, ___sx, ___sy, ___smod, ___rp, ___dx, ___dy, ___w, ___h, ___fmt) \
	LP10(0x7e, ULONG, WritePixelArray, APTR, ___src, a0, UWORD, ___sx, d0, UWORD, ___sy, d1, \
	     UWORD, ___smod, d2, struct RastPort *, ___rp, a1, UWORD, ___dx, d3, UWORD, ___dy, d4, \
	     UWORD, ___w, d5, UWORD, ___h, d6, UBYTE, ___fmt, d7, \
	     , CyberGfxBase)

#define LockBitMapTagList(___bm, ___tags) \
	LP2(0xa8, APTR, LockBitMapTagList, APTR, ___bm, a0, struct TagItem *, ___tags, a1, \
	    , CyberGfxBase)

#define UnLockBitMap(___handle) \
	LP1NR(0xae, UnLockBitMap, APTR, ___handle, a0, \
	      , CyberGfxBase)

/* ---- ahi.device's library calls ---------------------------------------- */

#include <devices/ahi.h>
#include <clib/ahi_protos.h>
#include <defines/ahi.h>        /* the NDK's m68k inline macros, despite the folder */

#endif
