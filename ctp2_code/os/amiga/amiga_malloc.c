/*
 * malloc, free, realloc and calloc for the 68060 and WarpOS builds, in place
 * of their C libraries'.
 *
 * libnix keeps the heap as a list of exec MemHeaders (one per 16 KB step
 * taken from the system) and serves each call with exec's first-fit
 * Allocate(); free() first walks the whole list to find the header that owns
 * the block. The game holds tens of thousands of small blocks (the AI's STL
 * lists and maps above all), so every call walked long lists: an AI turn spent
 * most of its time there (a list of 1,200 settle targets cost 1 to 2 s to
 * build and erase on a 68060).
 *
 * Here:
 * - up to 8 KB, size classes (16-byte steps to 256, then four per power of
 *   two), each with a free list: malloc and free are O(1). Blocks are carved
 *   from slabs of 64 KB (256 KB for the large classes) and go back to their
 *   class's list, never to the system, until the program ends;
 * - above 8 KB, a block of its own from exec (AllocMem), kept on a list.
 * Every block has a longword before it: its class, or LARGE for an exec one.
 * All of it goes back to the system when the program ends (libnix's exit
 * list, at the priority libnix's own malloc used): AmigaOS does not take a
 * program's memory back by itself.
 *
 * WarpOS (newlib): its malloc is powerpc.library's AllocVecPPC for every
 * block, with the same kind of first-fit walk. Here the slabs and the large
 * blocks come from AllocVecPPC, which stays on the PowerPC; there is no lock
 * (one PowerPC task allocates), and blocks are 8-byte aligned (doubles). The
 * memory is freed by a destructor that runs after all the others (the WarpOS
 * link script puts it last), and a free() after it is ignored.
 *
 * MorphOS keeps its libnix allocator.
 */
#if defined(CTP2_AMIGA_060) || defined(CTP2_AMIGA_WOS)

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#if !defined(CTP2_AMIGA_WOS)
#include <stabs.h>
#endif

#include "amiga_os.h"

#define SMALL_MAX    8192u
#define LARGE_TAG    0xFFFFFFFFu
#define MAX_CLASSES  48

#if defined(CTP2_AMIGA_WOS)
#define HEADER          8u          /* pad, tag: 8-byte aligned blocks */
#define LOCK()          ((void)0)
#define UNLOCK()        ((void)0)
#define SYS_ALLOC(n)    AllocVecPPC((n), MEMF_ANY, 32)
#define SYS_FREE(p, n)  ((void)(n), FreeVecPPC(p))
#else
#define HEADER          4u          /* tag */
#define LOCK()          Forbid()
#define UNLOCK()        Permit()
#define SYS_ALLOC(n)    AllocMem((n), MEMF_ANY)
#define SYS_FREE(p, n)  FreeMem((p), (n))
#endif

typedef struct Slab
{
	struct Slab * next;
	ULONG         bytes;
} Slab;

typedef struct Large
{
	struct Large * next;
	struct Large * prev;
	ULONG          bytes;      /* what the system gave */
	ULONG          tag;        /* LARGE_TAG, the longword before the block */
} Large;                       /* 16 bytes: the block stays 8-byte aligned */

static ULONG   s_classSize[MAX_CLASSES];        /* usable bytes */
static UBYTE   s_classOf[SMALL_MAX / 16 + 1];   /* (size + 15) / 16 -> class */
static int     s_classes;
static void *  s_free[MAX_CLASSES];             /* free blocks, linked through their first word */
static UBYTE * s_bump[MAX_CLASSES];             /* the rest of the current slab */
static UBYTE * s_bumpEnd[MAX_CLASSES];
static Slab *  s_slabs;
static Large * s_large;
static int     s_ready;
static int     s_closed;                        /* after the exit: free() ignores all */

static void init_classes(void)
{
	ULONG size;
	ULONG i;
	int   c = 0;

	for (size = 16; size <= 256; size += 16) s_classSize[c++] = size;
	for (size = 256; size < SMALL_MAX; size <<= 1)
	{
		ULONG const step = size / 4;
		ULONG k;
		for (k = 1; k <= 4; ++k) s_classSize[c++] = size + step * k;
	}
	s_classes = c;

	for (i = 0, c = 0; i <= SMALL_MAX / 16; ++i)
	{
		while (s_classSize[c] < i * 16) ++c;
		s_classOf[i] = (UBYTE)c;
	}
	s_ready = 1;
}

static void *small_alloc(int c)
{
	UBYTE * user = (UBYTE *)s_free[c];

	if (user)
	{
		s_free[c] = *(void **)user;
	}
	else
	{
		ULONG const slot = s_classSize[c] + HEADER;
		if (s_bump[c] + slot > s_bumpEnd[c])
		{
			ULONG const bytes = (slot <= 4096) ? 65536 : 262144;
			Slab * slab = (Slab *)SYS_ALLOC(bytes);
			if (!slab) return NULL;
			slab->next  = s_slabs;
			slab->bytes = bytes;
			s_slabs     = slab;
			s_bump[c]    = (UBYTE *)(slab + 1);
			s_bumpEnd[c] = (UBYTE *)slab + bytes;
		}
		user = s_bump[c] + HEADER;
		s_bump[c] += slot;
	}

	((ULONG *)user)[-1] = (ULONG)c;
	return user;
}

static void *large_alloc(size_t size)
{
	ULONG const bytes = (ULONG)(sizeof(Large) + size);
	Large * l;

	if (bytes < size) return NULL;          /* wrapped */
	l = (Large *)SYS_ALLOC(bytes);
	if (!l) return NULL;

	l->bytes = bytes;
	l->tag   = LARGE_TAG;
	l->prev  = NULL;
	l->next  = s_large;
	if (s_large) s_large->prev = l;
	s_large = l;
	return l + 1;
}

static size_t usable(void *ptr)
{
	ULONG const tag = ((ULONG *)ptr)[-1];
	if (tag == LARGE_TAG) return ((Large *)ptr - 1)->bytes - sizeof(Large);
	return s_classSize[tag];
}

/* The allocation itself, under a name the compiler does not take for malloc:
 * calloc's malloc + memset would otherwise be turned back into a call to
 * calloc, itself. */
static void *allocate(size_t size)
{
	void * p;

	LOCK();
	if (!s_ready) init_classes();
	p = (size <= SMALL_MAX) ? small_alloc(s_classOf[(size + 15) >> 4]) : large_alloc(size);
	UNLOCK();
	return p;
}

void *malloc(size_t size)
{
	return allocate(size);
}

void free(void *ptr)
{
	ULONG tag;

	if (!ptr || s_closed) return;

	LOCK();
	tag = ((ULONG *)ptr)[-1];
	if (tag == LARGE_TAG)
	{
		Large * l = (Large *)ptr - 1;
		if (l->prev) l->prev->next = l->next; else s_large = l->next;
		if (l->next) l->next->prev = l->prev;
		SYS_FREE(l, l->bytes);
	}
	else if (tag < (ULONG)s_classes)    /* else not ours: left alone */
	{
		*(void **)ptr = s_free[tag];
		s_free[tag]   = ptr;
	}
	UNLOCK();
}

void *realloc(void *ptr, size_t size)
{
	size_t old;
	void * p;

	if (!ptr || s_closed) return allocate(size);
	if (size == 0)
	{
		free(ptr);
		return NULL;
	}

	old = usable(ptr);
	if (size <= old) return ptr;

	p = allocate(size);
	if (!p) return NULL;
	memcpy(p, ptr, old);
	free(ptr);
	return p;
}

void *calloc(size_t count, size_t size)
{
	size_t const bytes = count * size;
	void * p;

	if (size && bytes / size != count) return NULL;
	p = allocate(bytes);
	if (p) memset(p, 0, bytes);
	return p;
}

void amiga_malloc_exit(void);
void amiga_malloc_exit(void)
{
	while (s_large)
	{
		Large * const l = s_large;
		s_large = l->next;
		SYS_FREE(l, l->bytes);
	}
	while (s_slabs)
	{
		Slab * const s = s_slabs;
		s_slabs = s->next;
		SYS_FREE(s, s->bytes);
	}
	memset(s_free, 0, sizeof(s_free));
	memset(s_bump, 0, sizeof(s_bump));
	memset(s_bumpEnd, 0, sizeof(s_bumpEnd));
}

#if defined(CTP2_AMIGA_WOS)

/* newlib's other entry points (its mallocglue.o, which would bring back the
 * AllocVecPPC malloc, must not be linked). */
struct _reent;

void *_malloc_r(struct _reent *r, size_t size)            { (void)r; return allocate(size); }
void  _free_r(struct _reent *r, void *ptr)                 { (void)r; free(ptr); }
void *_realloc_r(struct _reent *r, void *ptr, size_t size) { (void)r; return realloc(ptr, size); }
void *_calloc_r(struct _reent *r, size_t n, size_t size)   { (void)r; return calloc(n, size); }

/* Every block is 8-byte aligned; more is not asked for here. */
void *memalign(size_t align, size_t size)
{
	return (align <= 8) ? allocate(size) : NULL;
}

void *_memalign_r(struct _reent *r, size_t align, size_t size)
{
	(void)r;
	return memalign(align, size);
}

/* After every other destructor (os/amiga/warpos/warpos-etrel.ld puts this
 * file's .dtors entry last): the program's objects are gone. A free() after
 * this (newlib's exit list) is ignored. */
static void amiga_malloc_close(void) __attribute__((destructor));

static void amiga_malloc_close(void)
{
	amiga_malloc_exit();
	s_closed = 1;
}

#else

/* Where libnix's own malloc freed its memory: after the program's
 * destructors and libnix's other clean-up. */
ADD2EXIT(amiga_malloc_exit, -50);

#endif

#endif /* CTP2_AMIGA_060 || CTP2_AMIGA_WOS */
