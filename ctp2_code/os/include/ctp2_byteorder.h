//----------------------------------------------------------------------------
//
// Project      : Call To Power 2
// File type    : C++ header
// Description  : Byte order of the game's data files
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
// The game's binary files (sprites, tile sets, images, archives, saves) are
// little-endian: they were written on x86. ctp2_le16/32/64 turn a value read
// from such a file into the machine's order and back; on a little-endian
// machine they do nothing. WORDS_BIGENDIAN comes from config.h (configure's
// AC_C_BIGENDIAN, or os/amiga/config.h).
//
// The ctp2_swap_* helpers fix whole arrays or the fields of a record in
// place, right after it was read.
//
//----------------------------------------------------------------------------

#ifndef __CTP2_BYTEORDER_H__
#define __CTP2_BYTEORDER_H__ 1

#include "ctp2_inttypes.h"
#include <stddef.h>

static inline uint16 ctp2_bswap16(uint16 v)
{
	return static_cast<uint16>((v << 8) | (v >> 8));
}

static inline uint32 ctp2_bswap32(uint32 v)
{
	return (v << 24) | ((v << 8) & 0x00FF0000u) | ((v >> 8) & 0x0000FF00u) | (v >> 24);
}

static inline uint64 ctp2_bswap64(uint64 v)
{
	return (static_cast<uint64>(ctp2_bswap32(static_cast<uint32>(v))) << 32)
	     | ctp2_bswap32(static_cast<uint32>(v >> 32));
}

#if defined(WORDS_BIGENDIAN)
#define CTP2_LITTLE_ENDIAN_HOST 0

template <typename T> inline T ctp2_le16(T v) { return static_cast<T>(ctp2_bswap16(static_cast<uint16>(v))); }
template <typename T> inline T ctp2_le32(T v) { return static_cast<T>(ctp2_bswap32(static_cast<uint32>(v))); }
template <typename T> inline T ctp2_le64(T v) { return static_cast<T>(ctp2_bswap64(static_cast<uint64>(v))); }

#else
#define CTP2_LITTLE_ENDIAN_HOST 1

template <typename T> inline T ctp2_le16(T v) { return v; }
template <typename T> inline T ctp2_le32(T v) { return v; }
template <typename T> inline T ctp2_le64(T v) { return v; }

#endif

// In place: one field of a record just read (any 2, 4 or 8-byte integer).
template <typename T> inline void ctp2_swap_le(T & v)
{
#if defined(WORDS_BIGENDIAN)
	switch (sizeof(T))
	{
	case 2: v = static_cast<T>(ctp2_bswap16(static_cast<uint16>(v))); break;
	case 4: v = static_cast<T>(ctp2_bswap32(static_cast<uint32>(v))); break;
	case 8: v = static_cast<T>(ctp2_bswap64(static_cast<uint64>(v))); break;
	default: break;
	}
#else
	(void) v;
#endif
}

// In place: count 16-bit words (pixels, indexes) at data.
inline void ctp2_swap_le16_array(void * data, size_t count)
{
#if defined(WORDS_BIGENDIAN)
	uint16 * p = static_cast<uint16 *>(data);
	for (size_t i = 0; i < count; ++i) p[i] = ctp2_bswap16(p[i]);
#else
	(void) data; (void) count;
#endif
}

// In place: count 32-bit words at data.
inline void ctp2_swap_le32_array(void * data, size_t count)
{
#if defined(WORDS_BIGENDIAN)
	uint32 * p = static_cast<uint32 *>(data);
	for (size_t i = 0; i < count; ++i) p[i] = ctp2_bswap32(p[i]);
#else
	(void) data; (void) count;
#endif
}

// Reading a little-endian file held in memory, field by field: the value
// at *p, *p moved past it.
inline uint16 ctp2_read_le16(uint8 ** p)
{
	uint8 const * b = *p;
	*p += 2;
	return static_cast<uint16>(b[0] | (b[1] << 8));
}

inline uint32 ctp2_read_le32(uint8 ** p)
{
	uint8 const * b = *p;
	*p += 4;
	return static_cast<uint32>(b[0]) | (static_cast<uint32>(b[1]) << 8)
	     | (static_cast<uint32>(b[2]) << 16) | (static_cast<uint32>(b[3]) << 24);
}

#endif
