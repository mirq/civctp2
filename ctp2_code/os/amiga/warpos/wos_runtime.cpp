//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ source
// Description  : The C++ runtime pieces the WarpOS build provides itself
//
//----------------------------------------------------------------------------
//
// Disclaimer
//
// THIS FILE IS NOT GENERATED OR SUPPORTED BY ACTIVISION.
//
// This material has been developed at apolyton.net by the Apolyton CtP2
// Source Code Project. Contact the authors at ctp2source@apolyton.net.
//
//----------------------------------------------------------------------------
//
// The WarpOS build has no exceptions (no unwinder: the converter to a WarpOS
// executable cannot take the unwind tables' relocations). What of libstdc++
// would pull the exception runtime in is replaced here:
//
// - operator new[]/delete[], the nothrow and the sized forms over the game's
//   own operator new and delete (ctp/ctp2_utils/c3mem.cpp; libstdc++'s
//   versions throw bad_alloc);
// - __cxa_pure_virtual (libstdc++'s calls std::terminate);
// - the transactional-memory entry points libstdc++'s cow-stdexcept.o refers
//   to weakly; nothing here is compiled with -fgnu-tm, so none of them runs,
//   but the converter refuses any undefined symbol.
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <new>

void * operator new[](size_t size)
{
	return operator new(size);
}

void * operator new(size_t size, const std::nothrow_t &) throw()
{
	return malloc(size ? size : 1);
}

void * operator new[](size_t size, const std::nothrow_t &) throw()
{
	return malloc(size ? size : 1);
}

void operator delete[](void * p) throw()
{
	operator delete(p);
}

void operator delete(void * p, size_t) throw()
{
	operator delete(p);
}

void operator delete[](void * p, size_t) throw()
{
	operator delete(p);
}

extern "C" void __cxa_pure_virtual(void)
{
	fprintf(stderr, "Pure virtual function called\n");
	abort();
}

namespace
{
	void NoTransactionalMemory()
	{
		abort();
	}
}

extern "C" {

void _ITM_addUserCommitAction(void *, unsigned long long, void *) { NoTransactionalMemory(); }
void _ITM_memcpyRnWt(void *, const void *, size_t) { NoTransactionalMemory(); }
void _ITM_memcpyRtWn(void *, const void *, size_t) { NoTransactionalMemory(); }
unsigned char _ITM_RU1(const unsigned char *) { NoTransactionalMemory(); return 0; }
unsigned int _ITM_RU4(const unsigned int *) { NoTransactionalMemory(); return 0; }

// The transaction clones of operator delete(void*) and operator new[](size_t).
void _ZGTtdlPv(void *) { NoTransactionalMemory(); }
void * _ZGTtnaj(size_t) { NoTransactionalMemory(); return NULL; }

}
