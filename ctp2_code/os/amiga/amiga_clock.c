/*
 * The clock and short waits (amiga_platform.h), as OpenSWOS's rtg_clock.c:
 *
 *   WarpOS   powerpc.library's GetSysTimePPC and WaitTime, which never leave
 *            the PPC; and the waker task (below), started and stopped
 *            with the clock.
 *   68060,   timer.device: ReadEClock for the clock (a library call, no I/O)
 *   MorphOS  and a UNIT_MICROHZ request for the waits.
 */
#include "amiga_os.h"
#include "amiga_platform.h"

#if defined(CTP2_AMIGA_WOS)

#include <powerpc/tasksPPC.h>

static uint64_t s_base;

static uint64_t now_us(void)
{
	struct timeval tv;
	GetSysTimePPC(&tv);
	return (uint64_t)tv.tv_secs * 1000000u + tv.tv_micro;
}

/* The waker: a PPC task that only wakes every 2 ms. When no PPC task is
 * ready, WarpOS puts the PPC in nap mode; in Amiberry's PowerPC emulation
 * (QEMU) the interrupt that ends a 68k call is then taken but the task that
 * waits for it is not run again until about 59 s later. The game's file reads
 * hit that again and again (it never reached the menu). With a task that
 * wakes this often the scheduler runs regularly and no call is left behind:
 * reading a 100 KB file 600 times took 34 s with it, 1224 s without (14
 * stalls). On a real board it costs a few hundred task switches a second.
 * It touches nothing but these flags: no C library, no heap. */
static volatile int s_wakerQuit;
static volatile int s_wakerRunning;

static void waker(void)
{
	while (!s_wakerQuit) WaitTime(0, 2000);
	s_wakerRunning = 0;
}

static void waker_start(void)
{
	struct TagItem tags[] =
	{
		{ TASKATTR_CODE,      (ULONG)waker },
		{ TASKATTR_NAME,      (ULONG)"Call to Power II waker" },
		{ TASKATTR_PRI,       (ULONG)-100 },   /* as tested: below the game, */
		{ TASKATTR_NICE,      20 },            /* and the least CPU share */
		{ TASKATTR_INHERITR2, TRUE },
		{ TAG_DONE,           0 }
	};

	s_wakerQuit    = 0;
	s_wakerRunning = 1;
	if (!CreateTaskPPC(tags)) s_wakerRunning = 0;
}

/* The task's code is in this program: it must be gone before the program
 * is unloaded. */
static void waker_stop(void)
{
	int i;

	s_wakerQuit = 1;
	for (i = 0; s_wakerRunning && i < 500; ++i) WaitTime(0, 2000);
	WaitTime(0, 2000);          /* its last instructions after the flag */
}

int amiga_clock_init(void)
{
	s_base = now_us();
	waker_start();
	return 1;
}

void amiga_clock_exit(void)
{
	waker_stop();
}

uint64_t amiga_ticks_us(void)
{
	return now_us() - s_base;
}

void amiga_delay_ms(uint32_t ms)
{
	if (ms) WaitTime(0, (ULONG)ms * 1000u);
}

#else /* the 68060 and MorphOS */

#if defined(CTP2_AMIGA_MOS)
struct Library *TimerBase;
#else
struct Device *TimerBase;
#endif

static struct MsgPort     *s_port;
static struct timerequest *s_req;
static int                 s_open;
static ULONG               s_efreq;   /* E-clock ticks a second */
static uint64_t            s_base;

int amiga_clock_init(void)
{
	struct EClockVal ev;

	if (s_open) return 1;

	if (!(s_port = CreateMsgPort())) return 0;
	if (!(s_req = (struct timerequest *)CreateIORequest(s_port, sizeof(*s_req)))) return 0;
	if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)s_req, 0) != 0)
	{
		DeleteIORequest((struct IORequest *)s_req);
		s_req = NULL;
		return 0;
	}

	TimerBase = (void *)s_req->tr_node.io_Device;
	s_efreq   = ReadEClock(&ev);
	s_base    = ((uint64_t)ev.ev_hi << 32) | ev.ev_lo;
	s_open    = 1;
	return 1;
}

void amiga_clock_exit(void)
{
	if (s_open) CloseDevice((struct IORequest *)s_req);
	if (s_req)  DeleteIORequest((struct IORequest *)s_req);
	if (s_port) DeleteMsgPort(s_port);
	s_open = 0;
	s_req  = NULL;
	s_port = NULL;
	TimerBase = NULL;
}

uint64_t amiga_ticks_us(void)
{
	struct EClockVal ev;
	uint64_t t;

	if (!s_open) return 0;

	ReadEClock(&ev);
	t = (((uint64_t)ev.ev_hi << 32) | ev.ev_lo) - s_base;
	/* Seconds and the rest apart, so the products stay small. */
	return (t / s_efreq) * 1000000u + ((t % s_efreq) * 1000000u) / s_efreq;
}

void amiga_delay_ms(uint32_t ms)
{
	if (!s_open || ms == 0) return;

	s_req->tr_node.io_Command = TR_ADDREQUEST;
	s_req->tr_time.tv_secs    = ms / 1000u;
	s_req->tr_time.tv_micro   = (ms % 1000u) * 1000u;
	DoIO((struct IORequest *)s_req);
}

#endif

uint32_t amiga_ticks_ms(void)
{
	return (uint32_t)(amiga_ticks_us() / 1000u);
}

/* libnix has usleep; the WarpOS newlib may not. The game's few calls are in
 * the network code, which does not run here. */
#if defined(CTP2_AMIGA_WOS)
int usleep(unsigned int microseconds)
{
	amiga_delay_ms((microseconds + 999u) / 1000u);
	return 0;
}
#endif
