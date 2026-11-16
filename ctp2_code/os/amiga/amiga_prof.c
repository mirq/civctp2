/*
 * ENV:CTP2_PROF: a sampling profiler for the 68060 build.
 *
 * A task above the game's priority wakes every millisecond (timer.device).
 * Waking it preempts the game's task, whose registers exec has then saved on
 * that task's stack: the sampler takes the interrupted PC from there, and the
 * next few code addresses further up the stack (return addresses, mostly, as
 * a hint of the callers). CTRL-E (break <process> E) writes what was sampled
 * to RAM:ctp2prof.bin and starts again; the main loop looks for it
 * (amiga_prof_poll). The host resolves the addresses against ctp2.debug
 * (tools: see os/amiga/README.md).
 *
 * File: "CTPP", version 2, entries per sample (2 + AMIGA_PROF_CALLERS), the
 * number of samples, the samples the game was waiting (not running), then the
 * samples: the PC and the callers, each an offset from amiga_log (0: none),
 * and the game event being processed (-1: none), big-endian 32-bit words as
 * the 68k writes them. ENV:CTP2_PROF=2 follows the a5 frame chain (a build
 * with -fno-omit-frame-pointer) instead of guessing callers from the stack.
 *
 * Exec's saved context is not a documented layout: see AMIGA_PROF_PC_AT.
 * A PC outside the game's code (Kickstart, a library) is written as 1.
 */
#include "amiga_os.h"
#include "amiga_platform.h"

/* 0 while the game does something else than what is to be sampled
 * (ENV:CTP2_PROF_EVENT: one game event, GameEventManager.cpp). */
volatile int amiga_prof_gate = 1;

/* What the game does (the game event's number, else -1): kept with each
 * sample, so that one run can be split by event on the host. All targets
 * set them; only the 68060 build samples. */
volatile int amiga_prof_tag = -1;

#if defined(CTP2_AMIGA_060)

#include <dos/dostags.h>
#include <string.h>
#include <stdio.h>

#define AMIGA_PROF_CALLERS 8
#define AMIGA_PROF_ENTRY   (1 + AMIGA_PROF_CALLERS + 1)   /* PC, callers, tag */
#define AMIGA_PROF_SAMPLES (128 * 1024)

static struct Task *     s_main;
static uintptr_t         s_codeStart, s_codeEnd;
static uint32_t *        s_samples;
static volatile uint32_t s_count;
static volatile uint32_t s_waiting;
static volatile uint32_t s_outside;
static volatile uint32_t s_framed;
static int               s_frames;   /* ENV:CTP2_PROF=2: follow the a5 frame chain */
static volatile int      s_quit;
static volatile int      s_running;


static int in_code(uint32_t v)
{
	return v >= s_codeStart && v < s_codeEnd && !(v & 1);
}

/* Where exec (with the 68060.library) leaves a task that uses the FPU: the
 * FPU's state and registers first (FPIAR, the last FPU instruction's address,
 * at +12, which looks like a PC but is not), the PC at +124, then SR and
 * d0-a6, and the task's own stack from +190. Found on an A4000 by dumping
 * the saved words of a few samples. */
#define AMIGA_PROF_PC_AT    124
#define AMIGA_PROF_A5_AT    182   /* SR at +128, d0-d7/a0-a4 from +130 */
#define AMIGA_PROF_STACK_AT 190

static void sample(void)
{
	if (!amiga_prof_gate) return;
	if (s_main->tc_State != TS_READY)
	{
		++s_waiting;
		return;
	}
	if (s_count >= AMIGA_PROF_SAMPLES) return;

	uint8_t const * sp   = (uint8_t const *)s_main->tc_SPReg;
	uint32_t *      out  = s_samples + s_count * AMIGA_PROF_ENTRY;
	uintptr_t const base = (uintptr_t)&amiga_log;
	int             n    = 0;

	/* The PC; outside the game's code (Kickstart, a library) it is 1, and
	 * the callers below show what called out. */
	uint32_t const pc = *(uint32_t const *)(sp + AMIGA_PROF_PC_AT);
	if (in_code(pc))
	{
		out[n++] = pc - base;
	}
	else
	{
		out[n++] = 1;
		++s_outside;
	}

	/* A build with frame pointers (-fno-omit-frame-pointer: a5, link/unlk)
	 * gives the callers exactly: each frame holds the one before it and the
	 * return address. A function without a frame of its own (a leaf, the C
	 * library) leaves its caller out, nothing more. */
	if (s_frames)
	{
		uintptr_t const lower = (uintptr_t)sp;
		uintptr_t const upper = (uintptr_t)s_main->tc_SPUpper;
		uintptr_t       fp    = *(uint32_t const *)(sp + AMIGA_PROF_A5_AT);
		int const       first = n;
		while (n < AMIGA_PROF_ENTRY - 1 && fp > lower && fp + 8 <= upper && !(fp & 1))
		{
			uint32_t const ret  = ((uint32_t const *)fp)[1];
			uintptr_t const next = ((uint32_t const *)fp)[0];
			if (!in_code(ret)) break;
			out[n++] = ret - base;
			if (next <= fp) break;
			fp = next;
		}
		if (n > first)
		{
			++s_framed;
			while (n < AMIGA_PROF_ENTRY - 1) out[n++] = 0;
			out[n] = (uint32_t)amiga_prof_tag;
			++s_count;
			return;
		}
	}

	/* Else code addresses further up the stack (2-byte steps: the 68k keeps
	 * it word-aligned): return addresses mostly, now and then a stale one. */
	uint8_t const * p = sp + AMIGA_PROF_STACK_AT;
	for (int i = 0; i < 2048 && n < AMIGA_PROF_ENTRY - 1; ++i, p += 2)
	{
		uint32_t const v = *(uint32_t const *)p;
		if (in_code(v))
		{
			out[n++] = v - base;
			p += 2;   /* not the same word again, shifted */
			++i;
		}
	}
	while (n < AMIGA_PROF_ENTRY - 1) out[n++] = 0;
	out[n] = (uint32_t)amiga_prof_tag;
	++s_count;
}

static void sampler(void)
{
	struct MsgPort *     port = CreateMsgPort();
	struct timerequest * tr   = port ? (struct timerequest *)CreateIORequest(port, sizeof(*tr)) : NULL;
	if (tr && OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)tr, 0) == 0)
	{
		while (!s_quit)
		{
			tr->tr_node.io_Command = TR_ADDREQUEST;
			tr->tr_time.tv_secs    = 0;
			tr->tr_time.tv_micro   = 1000;
			DoIO((struct IORequest *)tr);
			Forbid();
			sample();
			Permit();
		}
		CloseDevice((struct IORequest *)tr);
	}
	if (tr)   DeleteIORequest((struct IORequest *)tr);
	if (port) DeleteMsgPort(port);
	Forbid();               /* the flag is seen only once this task is gone */
	s_running = 0;
}

/* The code segment holding amiga_log, from the CLI's segment list. */
static int find_code(void)
{
	struct Process * self = (struct Process *)FindTask(NULL);
	struct CommandLineInterface * cli = (struct CommandLineInterface *)BADDR(self->pr_CLI);
	BPTR seg = cli ? cli->cli_Module : self->pr_SegList;
	uintptr_t const log = (uintptr_t)&amiga_log;
	while (seg)
	{
		ULONG * s     = (ULONG *)BADDR(seg);
		uintptr_t const start = (uintptr_t)(s + 1);
		uintptr_t const end   = (uintptr_t)s - 4 + s[-1];
		if (log >= start && log < end)
		{
			s_codeStart = start;
			s_codeEnd   = end;
			return 1;
		}
		seg = (BPTR)s[0];
	}
	return 0;
}

void amiga_prof_start(void)
{
	if (!amiga_env_flag("CTP2_PROF")) return;
	{
		char value[8];
		s_frames = amiga_env_value("CTP2_PROF", value, sizeof(value)) && value[0] == '2';
	}
	if (!find_code())
	{
		amiga_log("Prof: the code segment was not found\n");
		return;
	}
	s_samples = (uint32_t *)AllocVec(AMIGA_PROF_SAMPLES * AMIGA_PROF_ENTRY * 4, MEMF_ANY);
	if (!s_samples) return;
	s_main    = FindTask(NULL);
	s_quit    = 0;
	s_running = 1;
	if (!CreateNewProcTags(NP_Entry, (ULONG)sampler, NP_Name, (ULONG)"Call to Power II profiler",
	                       NP_Priority, 20, NP_StackSize, 8192, TAG_DONE))
	{
		s_running = 0;
		FreeVec(s_samples);
		s_samples = NULL;
		return;
	}
	amiga_log("Prof: sampling every 1 ms, code %08lx-%08lx; CTRL-E writes RAM:ctp2prof.bin\n",
	          (unsigned long)s_codeStart, (unsigned long)s_codeEnd);
}

static void dump(const char *path)
{
	uint32_t const count   = s_count;
	uint32_t const waiting = s_waiting;
	FILE * f = path ? fopen(path, "wb") : NULL;
	if (f)
	{
		uint32_t head[5] = { 0x43545050u, 2, AMIGA_PROF_ENTRY, count, waiting };
		fwrite(head, 4, 5, f);
		fwrite(s_samples, 4, count * AMIGA_PROF_ENTRY, f);
		fclose(f);
	}
	if (path)
		amiga_log("Prof: %lu samples (%lu outside the game's code, %lu with a frame chain, %lu waiting) to %s\n",
		          (unsigned long)count, (unsigned long)s_outside, (unsigned long)s_framed, (unsigned long)waiting, path);
	Forbid();
	s_count   = 0;
	s_waiting = 0;
	s_outside = 0;
	s_framed  = 0;
	Permit();
}

void amiga_prof_poll(void)
{
	if (!s_samples) return;
	if (SetSignal(0, SIGBREAKF_CTRL_E) & SIGBREAKF_CTRL_E) dump("RAM:ctp2prof.bin");
}

void amiga_prof_write(const char *path)
{
	if (s_samples) dump(path);
}

void amiga_prof_stop(void)
{
	if (!s_samples) return;
	s_quit = 1;
	for (int i = 0; s_running && i < 100; ++i) Delay(1);
	if (!s_running)
	{
		FreeVec(s_samples);
	}
	s_samples = NULL;
}

#else

void amiga_prof_start(void) {}
void amiga_prof_poll(void)  {}
void amiga_prof_write(const char *path) { (void)path; }
void amiga_prof_stop(void)  {}

#endif
