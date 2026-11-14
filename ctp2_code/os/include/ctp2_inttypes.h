/**
 * $Id$
 */
#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif
#ifndef __os_include_ctp2_inttypes_h__
#define __os_include_ctp2_inttypes_h__ 1

#if defined(_MSC_VER)

typedef          char        schar;
typedef unsigned char        uchar;

typedef          char        sint8;
typedef unsigned char        uint8;
typedef          short int   sint16;
typedef unsigned short int   uint16;
typedef          int         sint32;
typedef unsigned int         uint32;
typedef          __int64     sint64;
typedef unsigned __int64     uint64;

typedef char                 CHAR;
#else

#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif
#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

typedef signed char          schar;
typedef unsigned char        uchar;

typedef int8_t               sint8;
typedef uint8_t              uint8;
typedef int16_t              sint16;
typedef uint16_t             uint16;
#if defined(CTP2_AMIGA_BUILD)
// Some Amiga compilers make int32_t a long (m68k-amigaos-gcc 16), which
// the game's overloads on int and unsigned (Army(sint32), Unit(uint32),
// std::min of a sint32 and a literal) cannot tell from an int: plain int
// here, 32 bits on every Amiga target.
typedef int                  sint32;
typedef unsigned int         uint32;
#else
typedef int32_t              sint32;
typedef uint32_t             uint32;
#endif
typedef int64_t              sint64;
typedef uint64_t             uint64;

#endif// !defined(_MSC_VER)

#endif
