/*
 * fptest: the FPU instructions that libnix's sscanf("%lg") (and so strtod,
 * and so every Float in the game's database) uses to build a number, one by
 * one. On a correct 68040/68060 every line says "ok". Results are printed as
 * the raw IEEE bits plus the value times 1000000 (converted with fmove.l),
 * so printing itself needs no float formatting.
 *
 *   m68k-amigaos-gcc -m68060 -mhard-float -noixemul -O1 -o fptest fptest.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

/* libnix: the decimal separator scanf/printf use, from the system Locale */
extern const char *__decimalpoint;

typedef union { double d; unsigned long w[2]; } Bits;

static int s_bad = 0;

/* ok when the result is within 1e-12 of the expected bits' value (the
   68k's extended intermediates may change the last bit) */
static void show(const char *what, double got, unsigned long hi, unsigned long lo)
{
	Bits b, e;
	long micro;
	double diff;
	int good;
	b.d = got;
	e.w[0] = hi; e.w[1] = lo;
	diff = got - e.d;
	if (diff < 0) diff = -diff;
	good = diff < 1e-12;
	micro = (long) (got * 1000000.0);
	printf("%-30s %08lx%08lx  x1e6=%ld  %s\n", what, b.w[0], b.w[1], micro,
	       good ? "ok" : "WRONG");
	if (!good) s_bad++;
}

int main(void)
{
	double r;
	long eight = 8, five = 5;

	/* fmovecr #$0f (0.0) and #$33 (10.0): the integer part */
	__asm__ volatile ("fmovecr #0x33,%0" : "=f" (r));
	show("fmovecr #$33 (10.0)", r, 0x40240000UL, 0UL);

	/* fmove.l Dn,FPn */
	__asm__ volatile ("fmove.l %1,%0" : "=f" (r) : "d" (eight));
	show("fmove.l d,fp (8)", r, 0x40200000UL, 0UL);

	/* fmove.d #imm,FPn: 0.1 */
	__asm__ volatile ("fmove.d #0r0.1,%0" : "=f" (r));
	show("fmove.d #0.1,fp", r, 0x3fb99999UL, 0x9999999aUL);

	/* fmul.l Dn,FPn: 0.1 * 8 */
	__asm__ volatile ("fmove.d #0r0.1,%0\n\tfmul.l %1,%0" : "=&f" (r) : "d" (eight));
	show("fmul.l d,fp (0.1*8)", r, 0x3fe99999UL, 0x9999999aUL);

	/* fmul.d #imm,FPn: 0.1 * 0.1 */
	__asm__ volatile ("fmove.d #0r0.1,%0\n\tfmul.d #0r0.1,%0" : "=&f" (r));
	show("fmul.d #0.1,fp (0.1*0.1)", r, 0x3f847ae1UL, 0x47ae147cUL);

	/* the whole fraction loop for "0.85": 0 + 0.1*8 + 0.01*5 */
	{
		double acc = 0.0, scale = 0.1, t;
		__asm__ volatile (
			"fmove.x %3,%2\n\tfmul.l %4,%2\n\tfadd.x %2,%0\n\tfmul.d #0r0.1,%1\n\t"
			"fmove.x %1,%2\n\tfmul.l %5,%2\n\tfadd.x %2,%0"
			: "+f" (acc), "+f" (scale), "=&f" (t) : "f" (scale), "d" (eight), "d" (five));
		show("fraction loop \"0.85\"", acc, 0x3feb3333UL, 0x33333333UL);
	}

	/* the library itself */
	show("strtod(\"0.85\")", strtod("0.85", NULL), 0x3feb3333UL, 0x33333333UL);
	show("strtod(\"12.0\")", strtod("12.0", NULL), 0x40280000UL, 0UL);
	{
		double v = 0.0;
		sscanf("0.75", "%lg", &v);
		show("sscanf(\"0.75\", \"%lg\")", v, 0x3fe80000UL, 0UL);
	}

	/* the locale: what the system Locale gave libnix at startup */
	printf("system decimal point: '%s'\n", __decimalpoint);
	{
		const char *saved = __decimalpoint;
		/* what a Polish Locale does, then the game's own fix attempt */
		__decimalpoint = ",";
		setlocale(LC_NUMERIC, "C");
		printf("after setlocale(LC_NUMERIC, \"C\"): '%s'\n", __decimalpoint);
		show("strtod(\"0.85\"), Polish locale", strtod("0.85", NULL), 0x3feb3333UL, 0x33333333UL);
		s_bad--; /* expected to fail: shows the bug, not the machine */
		__decimalpoint = saved;
	}

	printf("%s\n", s_bad ? "SOME FPU RESULTS ARE WRONG" : "all ok");
	return s_bad ? 5 : 0;
}
