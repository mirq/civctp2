/*
 * elf2warpos -- a WarpOS executable from a relocatable PowerPC ELF, on the
 * build host (plain C, no dependencies).
 *
 *     elf2warpos [--stack BYTES] IN.warpelf OUT   convert (stack: at least this, default 1 MB)
 *     elf2warpos --dump EXE                       the hunks and relocation table of a WarpOS exe
 *
 * The input is what Makefile.warpos links: an ET_REL ELF (ppc-amigaos-gcc +
 * MOS2WOS, `ld -r`, startupwos.o first so _start is at .text+0) with only the
 * relocations R_PPC_ADDR32, R_PPC_ADDR16_LO, R_PPC_ADDR16_HA and R_PPC_REL24.
 * The output is the hunk file Elf2Exe2 makes from the same input: the same
 * layout, relocation table format and PPCArgs to startupwos.o.
 *
 *     hunk 0       CODE  the 68k stub (stub.s): opens powerpc.library, relocates, RunPPC
 *     hunk 1       BSS   empty: ELF section 0
 *     hunk 2..n-2  DATA/BSS: every allocated ELF section, in section order (.text first)
 *     hunk n-1     DATA  the relocation table the stub applies (format in stub.s)
 *
 * Branches within one section are resolved here; every other relocation goes
 * in the table. A relocation this does not know is an error, not a guess.
 *
 * A C rewrite of OpenSWOS's tools/elf2warpos/elf2warpos.py (same output, byte
 * for byte), with the stub fixed for CyberStorm PPC / Blizzard PPC, which have
 * no MEMF_PPC memory (see stub.s).
 */
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t s_stub[] = {
#include "stub.inc"
};

#define DEFAULT_STACK   (1024u * 1024u)

#define HUNK_HEADER     0x3F3u
#define HUNK_CODE       0x3E9u
#define HUNK_DATA       0x3EAu
#define HUNK_BSS        0x3EBu
#define HUNK_END        0x3F2u

#define SHT_PROGBITS    1u
#define SHT_SYMTAB      2u
#define SHT_RELA        4u
#define SHT_NOBITS      8u
#define SHF_ALLOC       2u
#define SHN_UNDEF       0u
#define SHN_ABS         0xFFF1u
#define SHN_COMMON      0xFFF2u

#define R_PPC_ADDR32    1u
#define R_PPC_ADDR16_LO 4u
#define R_PPC_ADDR16_HA 6u
#define R_PPC_REL24     10u

#define OFF_MAX         0x0FFFFFFFu     /* a table entry's offset field: 28 bits */

static const char *s_prog = "elf2warpos";

static void die(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s: ", s_prog);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

static void *xalloc(size_t n)
{
	void *p = calloc(n ? n : 1, 1);
	if (!p) die("out of memory");
	return p;
}

/* ---- big-endian access, bounds-checked against the input ---- */

typedef struct
{
	uint8_t *data;
	size_t   size;
} Buf;

static void need(const Buf *b, uint64_t off, uint64_t len, const char *what)
{
	if (off > b->size || len > b->size - off) die("truncated file (%s)", what);
}

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static void wr16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

static Buf read_file(const char *name)
{
	Buf   b = { NULL, 0 };
	FILE *f = fopen(name, "rb");
	long  n;

	if (!f) die("%s: %s", name, strerror(errno));
	if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) die("%s: cannot size it", name);
	b.size = (size_t)n;
	b.data = xalloc(b.size);
	if (b.size && fread(b.data, 1, b.size, f) != b.size) die("%s: read error", name);
	fclose(f);
	return b;
}

/* ---- growing output ---- */

typedef struct
{
	uint8_t *p;
	size_t   len, cap;
} Out;

static void out_bytes(Out *o, const void *src, size_t n)
{
	if (o->len + n > o->cap)
	{
		size_t cap = o->cap ? o->cap : 4096;
		while (cap < o->len + n) cap *= 2;
		o->p = realloc(o->p, cap);
		if (!o->p) die("out of memory");
		o->cap = cap;
	}
	memcpy(o->p + o->len, src, n);
	o->len += n;
}

static void out32(Out *o, uint32_t v)
{
	uint8_t b[4];
	wr32(b, v);
	out_bytes(o, b, 4);
}

/* ---- ELF ---- */

typedef struct
{
	uint32_t    type, flags, off, size, link, info;
	const char *name;
	int         hunk;       /* -1: not loaded */
	uint8_t    *image;      /* PROGBITS: a patched copy */
} Section;

typedef struct
{
	const char *name;
	uint32_t    value;
	uint16_t    shndx;
} Symbol;

typedef struct
{
	uint32_t patch, shunk, seq;
	uint32_t word, extra;
	int      has_extra;
} Entry;

static const char *cstr(const Buf *b, uint32_t off, const char *what)
{
	need(b, off, 1, what);
	if (!memchr(b->data + off, 0, b->size - off)) die("unterminated string (%s)", what);
	return (const char *)b->data + off;
}

static const char *reloc_name(uint32_t t)
{
	static char buf[24];
	switch (t)
	{
	case R_PPC_ADDR32:    return "R_PPC_ADDR32";
	case R_PPC_ADDR16_LO: return "R_PPC_ADDR16_LO";
	case R_PPC_ADDR16_HA: return "R_PPC_ADDR16_HA";
	case R_PPC_REL24:     return "R_PPC_REL24";
	}
	snprintf(buf, sizeof buf, "type %u", t);
	return buf;
}

static int cmp_entry(const void *a, const void *b)
{
	const Entry *x = a, *y = b;
	if (x->patch != y->patch) return x->patch < y->patch ? -1 : 1;
	if (x->shunk != y->shunk) return x->shunk < y->shunk ? -1 : 1;
	return x->seq < y->seq ? -1 : x->seq > y->seq;
}

static Out convert(const Buf *in, uint32_t stack, unsigned *counts, int *nalloc_out)
{
	const uint8_t *d = in->data;
	Section  *secs;
	Symbol   *syms;
	Entry    *ents = NULL;
	size_t    nents = 0, capents = 0;
	uint32_t  shoff, nsyms = 0, i;
	uint16_t  shentsize, shnum, shstrndx;
	int       nalloc = 0, nhunks, h, symtab = -1;
	Out       o = { NULL, 0, 0 };
	Out       reloc = { NULL, 0, 0 };
	uint32_t *hkind, *hbytes;
	const uint8_t **hdata;
	uint8_t  *stub;

	need(in, 0, 0x34, "ELF header");
	if (memcmp(d, "\177ELF", 4) || d[4] != 1 || d[5] != 2) die("not a 32-bit big-endian ELF");
	if (rd16(d + 16) != 1 || rd16(d + 18) != 20) die("not a relocatable PowerPC ELF (ET_REL, EM_PPC)");
	shoff     = rd32(d + 0x20);
	shentsize = rd16(d + 0x2E);
	shnum     = rd16(d + 0x30);
	shstrndx  = rd16(d + 0x32);
	if (shentsize < 40 || shstrndx >= shnum) die("bad section header table");
	need(in, shoff, (uint64_t)shentsize * shnum, "section headers");

	secs = xalloc(sizeof *secs * shnum);
	for (i = 0; i < shnum; ++i)
	{
		const uint8_t *s = d + shoff + i * shentsize;
		secs[i].type  = rd32(s + 4);
		secs[i].flags = rd32(s + 8);
		secs[i].off   = rd32(s + 16);
		secs[i].size  = rd32(s + 20);
		secs[i].link  = rd32(s + 24);
		secs[i].info  = rd32(s + 28);
		secs[i].hunk  = -1;
		if (secs[i].type != SHT_NOBITS) need(in, secs[i].off, secs[i].size, "section contents");
	}
	for (i = 0; i < shnum; ++i)
		secs[i].name = cstr(in, secs[shstrndx].off + rd32(d + shoff + i * shentsize), "section name");

	/* The symbols. */
	for (i = 0; i < shnum; ++i)
		if (secs[i].type == SHT_SYMTAB)
		{
			if (symtab >= 0) die("expected one symbol table, found more");
			symtab = (int)i;
		}
	if (symtab < 0) die("expected one symbol table, found 0");
	if (secs[symtab].link >= shnum) die("bad symbol string table");
	nsyms = secs[symtab].size / 16;
	syms  = xalloc(sizeof *syms * (nsyms + 1));
	for (i = 0; i < nsyms; ++i)
	{
		const uint8_t *s = d + secs[symtab].off + i * 16;
		syms[i].name  = cstr(in, secs[secs[symtab].link].off + rd32(s), "symbol name");
		syms[i].value = rd32(s + 4);
		syms[i].shndx = rd16(s + 14);
	}

	/* Hunks: 0 the stub, 1 empty (ELF section 0), then every allocated section. */
	for (i = 0; i < shnum; ++i)
	{
		if (!(secs[i].flags & SHF_ALLOC)) continue;
		if (secs[i].type != SHT_PROGBITS && secs[i].type != SHT_NOBITS)
			die("an allocated section of a kind this does not place: %s", secs[i].name);
		if (nalloc == 0 && strcmp(secs[i].name, ".text"))
			die(".text must be the first allocated section (_start at hunk 2 +0)");
		secs[i].hunk = 2 + nalloc++;
		if (secs[i].type == SHT_PROGBITS)
		{
			secs[i].image = xalloc(secs[i].size);
			memcpy(secs[i].image, d + secs[i].off, secs[i].size);
		}
	}
	if (!nalloc) die(".text must be the first allocated section (_start at hunk 2 +0)");

	for (i = 0; i < shnum; ++i)
	{
		const Section *rs = &secs[i];
		Section *ts;
		uint32_t r, nrel;

		if (rs->type != SHT_RELA) continue;
		if (rs->info >= shnum) die("relocations for a section that does not exist");
		ts = &secs[rs->info];
		if (ts->hunk < 0)
		{
			if (ts->flags & SHF_ALLOC) die("relocations for %s, which is not placed", ts->name);
			continue;                       /* .rela.debug_* and the like: nothing to load */
		}
		if (ts->type == SHT_NOBITS) die("relocations into %s, which has no contents", ts->name);

		nrel = rs->size / 12;
		for (r = 0; r < nrel; ++r)
		{
			const uint8_t *e = d + rs->off + r * 12;
			uint32_t const off   = rd32(e);
			uint32_t const info  = rd32(e + 4);
			uint32_t const type  = info & 0xFF, symi = info >> 8;
			int32_t  const add   = (int32_t)rd32(e + 8);
			const Symbol  *sym;
			uint32_t value, word = 0, extra = 0, shunk = 0;
			int absolute, table = 0, has_extra = 0;
			uint8_t *at;
			char where[256];

			if (symi >= nsyms) die("relocation against a symbol that does not exist in %s", ts->name);
			sym = &syms[symi];
			snprintf(where, sizeof where, "%s+%#x (%s against %s)", ts->name, off, reloc_name(type),
			         sym->name[0] ? sym->name : "a section symbol");
			if (sym->shndx == SHN_UNDEF || sym->shndx == SHN_COMMON) die("undefined or COMMON symbol at %s", where);
			value    = sym->value + (uint32_t)add;
			absolute = sym->shndx == SHN_ABS;
			if (!absolute)
			{
				if (sym->shndx >= shnum || secs[sym->shndx].hunk < 0) die("symbol in a section that is not loaded at %s", where);
				shunk = (uint32_t)secs[sym->shndx].hunk;
			}
			if (off > ts->size || ts->size - off < (type == R_PPC_ADDR32 || type == R_PPC_REL24 ? 4u : 2u))
				die("relocation outside its section at %s", where);
			at = ts->image + off;

			switch (type)
			{
			case R_PPC_ADDR32:
				wr32(at, value);
				if (absolute) { ++counts[0]; break; }
				if (off & 0x80000000u) die("offset too large at %s", where);
				word = off; table = 1; ++counts[1];
				break;
			case R_PPC_ADDR16_LO:
				wr16(at, (uint16_t)value);
				if (absolute) { ++counts[0]; break; }
				word = 0xD0000000u; table = 1; ++counts[3];
				break;
			case R_PPC_ADDR16_HA:
				wr16(at, absolute ? (uint16_t)((value + 0x8000u) >> 16) : (uint16_t)(value >> 16));
				if (absolute) { ++counts[0]; break; }
				word = 0xC0000000u; extra = value; has_extra = 1; table = 1; ++counts[2];
				break;
			case R_PPC_REL24:
			{
				uint32_t const insn = rd32(at);
				if (absolute) die("a branch to an absolute address at %s", where);
				if (shunk == (uint32_t)ts->hunk)
				{
					uint32_t const disp = value - off;
					int64_t  const sd   = (int32_t)disp;
					if (sd < -0x2000000 || sd >= 0x2000000 || (sd & 3)) die("branch out of range at %s", where);
					wr32(at, (insn & 0xFC000003u) | (disp & 0x03FFFFFCu));
					++counts[0];
					break;
				}
				wr32(at, (insn & 0xFC000003u) | (value & 0x03FFFFFCu));
				word = 0xE0000000u; table = 1; ++counts[4];
				break;
			}
			default:
				die("a relocation this converter does not know at %s", where);
			}

			if (!table) continue;
			if (type != R_PPC_ADDR32)
			{
				if (off > OFF_MAX) die("offset too large for the table at %s", where);
				word |= off;
			}
			if (nents == capents)
			{
				capents = capents ? capents * 2 : 65536;
				ents = realloc(ents, sizeof *ents * capents);
				if (!ents) die("out of memory");
			}
			ents[nents].patch     = (uint32_t)ts->hunk;
			ents[nents].shunk     = shunk;
			ents[nents].seq       = (uint32_t)nents;
			ents[nents].word      = word;
			ents[nents].extra     = extra;
			ents[nents].has_extra = has_extra;
			++nents;
		}
	}

	/* The table: { patch; { count; symbol hunk; entries }...; 0 }...; 0 */
	if (nents) qsort(ents, nents, sizeof *ents, cmp_entry);
	for (i = 0; i < nents; )
	{
		uint32_t const patch = ents[i].patch;
		out32(&reloc, patch);
		while (i < nents && ents[i].patch == patch)
		{
			uint32_t const shunk = ents[i].shunk;
			size_t j = i;
			while (j < nents && ents[j].patch == patch && ents[j].shunk == shunk) ++j;
			out32(&reloc, (uint32_t)(j - i));
			out32(&reloc, shunk);
			for (; i < j; ++i)
			{
				out32(&reloc, ents[i].word);
				if (ents[i].has_extra) out32(&reloc, ents[i].extra);
			}
		}
		out32(&reloc, 0);
	}
	out32(&reloc, 0);

	/* The stub, with the stack size in its "STK!" slot. */
	stub = xalloc(sizeof s_stub);
	memcpy(stub, s_stub, sizeof s_stub);
	{
		uint8_t *slot = NULL, *p = stub;
		while ((p = memchr(p, 'S', (size_t)(stub + sizeof s_stub - p))) != NULL && p + 4 <= stub + sizeof s_stub)
		{
			if (!memcmp(p, "STK!", 4))
			{
				if (slot || ((p - stub) & 1)) die("the stub has no (or more than one) stack slot");
				slot = p;
			}
			++p;
		}
		if (!slot) die("the stub has no (or more than one) stack slot");
		wr32(slot, stack);
	}

	/* The hunk file. */
	nhunks = nalloc + 3;
	hkind  = xalloc(sizeof *hkind * nhunks);
	hbytes = xalloc(sizeof *hbytes * nhunks);
	hdata  = xalloc(sizeof *hdata * nhunks);
	hkind[0] = HUNK_CODE; hbytes[0] = sizeof s_stub; hdata[0] = stub;
	hkind[1] = HUNK_BSS;  hbytes[1] = 0;
	for (i = 0, h = 2; i < shnum; ++i)
	{
		if (secs[i].hunk < 0) continue;
		hkind[h]  = secs[i].type == SHT_NOBITS ? HUNK_BSS : HUNK_DATA;
		hbytes[h] = secs[i].size;
		hdata[h]  = secs[i].image;
		++h;
	}
	hkind[h] = HUNK_DATA; hbytes[h] = (uint32_t)reloc.len; hdata[h] = reloc.p;

	out32(&o, HUNK_HEADER);
	out32(&o, 0);
	out32(&o, (uint32_t)nhunks);
	out32(&o, 0);
	out32(&o, (uint32_t)nhunks - 1);
	for (h = 0; h < nhunks; ++h) out32(&o, (hbytes[h] + 3) / 4);
	for (h = 0; h < nhunks; ++h)
	{
		uint32_t const longs = (hbytes[h] + 3) / 4;
		out32(&o, hkind[h]);
		out32(&o, longs);
		if (hkind[h] != HUNK_BSS)
		{
			static const uint8_t zero[4] = { 0, 0, 0, 0 };
			if (hbytes[h]) out_bytes(&o, hdata[h], hbytes[h]);
			out_bytes(&o, zero, longs * 4 - hbytes[h]);
		}
		out32(&o, HUNK_END);
	}

	*nalloc_out = nalloc;
	return o;
}

/* ---- --dump ---- */

static void dump(const Buf *in)
{
	size_t p = 0, ntab = 0, q = 0;
	uint32_t first, last, n;
	const uint8_t *tab = NULL;
	unsigned kinds[5] = { 0, 0, 0, 0, 0 };   /* ADDR32, pair, HA, LO, REL24 */
	int idx = 0;

#define U32() (need(in, p, 4, "hunk file"), p += 4, rd32(in->data + p - 4))
	if (U32() != HUNK_HEADER) die("not a hunk file");
	while ((n = U32()) != 0) p += (size_t)n * 4;
	U32();
	first = U32();
	last  = U32();
	p += (size_t)(last - first + 1) * 4;
	while (p < in->size)
	{
		uint32_t const t = U32() & 0x3FFFFFFFu;
		if (t == HUNK_CODE || t == HUNK_DATA)
		{
			n = U32();
			need(in, p, (uint64_t)n * 4, "hunk");
			printf("hunk %2d %-4s %8u bytes\n", idx++, t == HUNK_CODE ? "CODE" : "DATA", n * 4);
			tab = in->data + p; ntab = (size_t)n * 4;
			p += (size_t)n * 4;
		}
		else if (t == HUNK_BSS)
		{
			n = U32();
			printf("hunk %2d %-4s %8u bytes\n", idx++, "BSS", n * 4);
		}
		else if (t != HUNK_END)
			die("hunk type %#x this does not read", t);
	}
#undef U32
	if (!tab) die("no relocation table");

#define G() (q + 4 > ntab ? (die("relocation table overrun"), 0u) : (q += 4, rd32(tab + q - 4)))
	while (G() != 0)
	{
		uint32_t cnt;
		while ((cnt = G()) != 0)
		{
			G();
			while (cnt--)
			{
				uint32_t const e = G();
				if (!(e & 0x80000000u))      ++kinds[0];
				else if (!(e & 0x40000000u)) ++kinds[1];
				else if (e & 0x20000000u)    ++kinds[4];
				else if (e & 0x10000000u)    ++kinds[3];
				else                         { ++kinds[2]; G(); }
			}
		}
	}
#undef G
	printf("relocation table: %zu of %zu bytes read; ADDR32 %u, HA/LO pair %u, HA %u, LO %u, REL24 %u\n",
	       q, ntab, kinds[0], kinds[1], kinds[2], kinds[3], kinds[4]);
}

static int usage(void)
{
	fprintf(stderr,
	        "usage: %s [--stack BYTES] IN.warpelf OUT   convert (stack: at least this, default 1 MB)\n"
	        "       %s --dump EXE                       the hunks and relocation table of a WarpOS exe\n",
	        s_prog, s_prog);
	return 2;
}

int main(int argc, char **argv)
{
	uint32_t stack = DEFAULT_STACK;
	unsigned counts[5] = { 0, 0, 0, 0, 0 };   /* resolved here, ADDR32, HA, LO, REL24 */
	int      nalloc = 0;
	Buf      in;
	Out      out;
	FILE    *f;

	if (argc == 3 && !strcmp(argv[1], "--dump"))
	{
		in = read_file(argv[2]);
		dump(&in);
		return 0;
	}
	if (argc >= 3 && !strcmp(argv[1], "--stack"))
	{
		char *end;
		unsigned long v = strtoul(argv[2], &end, 0);
		if (*end || !*argv[2] || v > 0xFFFFFFFFul) return usage();
		stack = (uint32_t)v;
		argv += 2;
		argc -= 2;
	}
	if (argc != 3) return usage();

	in  = read_file(argv[1]);
	out = convert(&in, stack, counts, &nalloc);

	f = fopen(argv[2], "wb");
	if (!f) die("%s: %s", argv[2], strerror(errno));
	if (fwrite(out.p, 1, out.len, f) != out.len || fclose(f))
	{
		remove(argv[2]);
		die("%s: write error", argv[2]);
	}
	printf("elf2warpos: %s -> %s, %zu bytes: %d sections; resolved here %u, in the table "
	       "%u ADDR32, %u HA, %u LO, %u REL24\n",
	       argv[1], argv[2], out.len, nalloc, counts[0], counts[1], counts[2], counts[3], counts[4]);
	return 0;
}
