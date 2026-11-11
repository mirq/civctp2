#ifdef HAVE_PRAGMA_ONCE
#pragma once
#endif
#ifndef _NET_UTIL_H_
#define _NET_UTIL_H_

#if defined(WIN32) || !defined(WORDS_BIGENDIAN)
#define NET_UTIL_LITTLE_ENDIAN
#endif

#ifdef NET_UTIL_LITTLE_ENDIAN
inline sint8 getbyte(uint8 *b)
{
	return *(sint8*)b;
}

inline sint16 getshort(uint8* b)
{
	return *(sint16*)b;
}

inline sint32 getlong(uint8* b)
{
	return *(sint32*)b;
}

inline sint64 getlong64(uint8* b)
{
	return *(sint64*)b;
}

inline double getdouble(uint8* b)
{
	return *(double*)b;
}

inline void putbyte(uint8* b, sint8 by)
{
	*b = by;
}

inline void putshort(uint8* b, sint16 s)
{
	*(sint16*)b = s;
}

inline void putlong(uint8* b, sint32 l)
{
	*(sint32*)b = l;
}

inline void putlong64(uint8* b, sint64 l)
{
	*(sint64*)b = l;
}
inline void putdouble(uint8* b, double l)
{
	*(double*)b = l;
}

#else

// Big-endian CPUs (the Amiga builds): the wire and the map files are
// little-endian, so the bytes are put together one at a time.
inline sint8 getbyte(uint8 *b)
{
	return *(sint8*)b;
}

inline sint16 getshort(uint8* b)
{
	return static_cast<sint16>(b[0] | (b[1] << 8));
}

inline sint32 getlong(uint8* b)
{
	return static_cast<sint32>(  static_cast<uint32>(b[0])
	                          | (static_cast<uint32>(b[1]) << 8)
	                          | (static_cast<uint32>(b[2]) << 16)
	                          | (static_cast<uint32>(b[3]) << 24));
}

inline sint64 getlong64(uint8* b)
{
	return static_cast<sint64>(  static_cast<uint64>(static_cast<uint32>(getlong(b)))
	                          | (static_cast<uint64>(static_cast<uint32>(getlong(b + 4))) << 32));
}

inline double getdouble(uint8* b)
{
	uint64 bits = static_cast<uint64>(getlong64(b));
	double d;
	memcpy(&d, &bits, sizeof(d));
	return d;
}

inline void putbyte(uint8* b, sint8 by)
{
	*b = by;
}

inline void putshort(uint8* b, sint16 s)
{
	b[0] = static_cast<uint8>(s);
	b[1] = static_cast<uint8>(static_cast<uint16>(s) >> 8);
}

inline void putlong(uint8* b, sint32 l)
{
	uint32 u = static_cast<uint32>(l);
	b[0] = static_cast<uint8>(u);
	b[1] = static_cast<uint8>(u >> 8);
	b[2] = static_cast<uint8>(u >> 16);
	b[3] = static_cast<uint8>(u >> 24);
}

inline void putlong64(uint8* b, sint64 l)
{
	uint64 u = static_cast<uint64>(l);
	putlong(b,     static_cast<sint32>(static_cast<uint32>(u)));
	putlong(b + 4, static_cast<sint32>(static_cast<uint32>(u >> 32)));
}

inline void putdouble(uint8* b, double l)
{
	uint64 bits;
	memcpy(&bits, &l, sizeof(bits));
	putlong64(b, static_cast<sint64>(bits));
}

#endif

#define NET_STRLEN(x)   static_cast<uint16>(strlen(x))

#define PUSHBYTE(x)    {buf[size++] = static_cast<uint8>(x);}
#define PUSHSHORT(x)  {putshort(&buf[size], static_cast<uint16>(x)); size += sizeof(uint16);}
#define PUSHLONG(x)   {putlong(&buf[size], static_cast<uint32>(x)); size += sizeof(uint32);}
#define PUSHLONG64(x) {putlong64(&buf[size], static_cast<uint64>(x)); size += sizeof(uint64);}
#define PUSHDOUBLE(x) {putdouble(&buf[size], (x)); size += sizeof(double);}
#define PUSHID(x) {buf[size++] = ((x) >> 8); buf[size++] = ((x) & 0xff); }
#define PUSHSTRING(x) { uint16 l = ((x) ? (NET_STRLEN(x)) : 0); PUSHSHORT(l); if(l > 0) {memcpy(&buf[size], x, l); size += l;}}

#define PULLBYTE(x)   {x = buf[pos++];}
#define PULLBYTETYPE(x,ty) {x = (ty)buf[pos++];}
#define PULLSHORT(x)  {x = getshort(&buf[pos]); pos += sizeof(uint16);}
#define PULLSHORTTYPE(x, ty)  {x = (ty)getshort(&buf[pos]); pos += sizeof(uint16);}
#define PULLLONG(x)   {x = getlong(&buf[pos]); pos += sizeof(uint32);}
#define PULLLONGTYPE(x,ty) {x = (ty)getlong(&buf[pos]); pos += sizeof(uint32);}
#define PULLLONG64(x) {x = getlong64(&buf[pos]); pos += sizeof(uint64);}
#define PULLDOUBLE(x) {x = getdouble(&buf[pos]); pos += sizeof(double);}
#define PULLID(x) { x = (buf[pos] << 8) | (buf[pos+1]); pos += 2;}
#define PULLSTRING(x) { uint16 l; PULLSHORT(l); if(l > 0) { memcpy(x, &buf[pos], l); (x)[l] = 0; pos += l; }}
#define PULLNEWSTRING(x) { uint16 l; PULLSHORT(l); if(l > 0) {x = new char[l + 1]; memcpy(x, &buf[pos], l); (x)[l] = 0; pos += l;} else { x = NULL; }}

#endif
