/*
 * Input from the screen's backdrop window (amiga_platform.h).
 *
 * The port is drained only when it holds messages: whether it does is a read
 * of memory (the message list's head), no OS call; on WarpOS every OS call is a
 * trip to the 68k. Messages are turned into AmigaEvents in a small queue the
 * UI takes one at a time.
 */
#include <string.h>

#include "amiga_os.h"
#include "amiga_platform.h"

struct Window *amiga_video_window(void);
void amiga_video_offset(int *x, int *y);

#define QUEUE_SIZE 128

static AmigaEvent s_queue[QUEUE_SIZE];
static int        s_head;
static int        s_count;
static int        s_mouseX;
static int        s_mouseY;

static void push(const AmigaEvent *ev)
{
	/* Two moves in a row: the newer one is enough. */
	if (ev->type == AMIGA_EV_MOUSEMOVE && s_count > 0)
	{
		AmigaEvent *last = &s_queue[(s_head + s_count - 1) % QUEUE_SIZE];
		if (last->type == AMIGA_EV_MOUSEMOVE)
		{
			*last = *ev;
			return;
		}
	}

	if (s_count == QUEUE_SIZE)
	{
		/* Full: the oldest goes. */
		s_head = (s_head + 1) % QUEUE_SIZE;
		--s_count;
	}

	s_queue[(s_head + s_count) % QUEUE_SIZE] = *ev;
	++s_count;
}

static uint32_t s_lastInputMs;   /* when the window's last message came */

uint32_t amiga_input_idle_ms(void)
{
	return amiga_ticks_ms() - s_lastInputMs;
}

static int port_has_messages(struct MsgPort *port)
{
	/* exec's IsListEmpty() on the port's own list header: the queued messages
	 * themselves are not read. On WarpOS with a PCI PowerPC card (Sonnet,
	 * powerpc.library 17) the PPC sees only its own RAM: Intuition's input
	 * handler builds the IntuiMessages in the 68k's RAM, where any PPC access
	 * (even a dcbf) is a Data Storage exception. The port is the game's own
	 * (OpenWindow on its behalf), in memory the PPC sees. The 68k writes the
	 * header: the PPC's copy of those lines goes first (a no-op on the
	 * 68060). */
	amiga_cache_flush(&port->mp_MsgList, sizeof(port->mp_MsgList));
	return port->mp_MsgList.lh_TailPred != (struct Node *)&port->mp_MsgList;
}

/* A message's fields as the PPC may read them: on WarpOS a copy the 68k makes
 * (CopyMem runs there, it sees both memories; see port_has_messages), else
 * the message itself. */
static struct IntuiMessage *msg_fields(struct IntuiMessage *msg, struct IntuiMessage *copy)
{
#if defined(CTP2_AMIGA_WOS)
	amiga_cache_flush(copy, sizeof(*copy));
	CopyMem(msg, copy, sizeof(*copy));
	amiga_cache_flush(copy, sizeof(*copy));
	return copy;
#else
	(void)copy;
	return msg;
#endif
}

/* The word a dead key's IAddress points to (in input.device's memory: on
 * WarpOS read by the 68k, as above). */
static APTR dead_key_address(struct IntuiMessage *msg)
{
	APTR addr = NULL;

	if (!msg->IAddress) return NULL;
#if defined(CTP2_AMIGA_WOS)
	amiga_cache_flush(&addr, sizeof(addr));
	CopyMem(msg->IAddress, &addr, sizeof(addr));
	amiga_cache_flush(&addr, sizeof(addr));
#else
	addr = *(APTR *)msg->IAddress;
#endif
	return addr;
}

static char key_char(struct IntuiMessage *msg)
{
	struct InputEvent ie;
	char   buf[8];
	LONG   n;

	memset(&ie, 0, sizeof(ie));
	ie.ie_Class        = IECLASS_RAWKEY;
	ie.ie_Code         = msg->Code & ~IECODE_UP_PREFIX;
	ie.ie_Qualifier    = msg->Qualifier;
	ie.ie_EventAddress = dead_key_address(msg); /* dead keys */

	n = MapRawKey(&ie, (STRPTR)buf, sizeof(buf), NULL);
	return (n == 1) ? buf[0] : 0;
}

static void drain(void)
{
	struct Window *       window = amiga_video_window();
	struct IntuiMessage * got;
	struct IntuiMessage   copy;
	int                   ox, oy;

	if (!window || !port_has_messages(window->UserPort)) return;

	amiga_video_offset(&ox, &oy);

	while ((got = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
	{
		struct IntuiMessage * const msg = msg_fields(got, &copy);
		AmigaEvent ev;
		memset(&ev, 0, sizeof(ev));
		ev.time_ms   = amiga_ticks_ms();
		s_lastInputMs = ev.time_ms;
		ev.qualifier = msg->Qualifier & 0xFF;

		switch (msg->Class)
		{
		case IDCMP_RAWKEY:
		{
			UWORD const code = msg->Code & ~IECODE_UP_PREFIX;
			ev.down = !(msg->Code & IECODE_UP_PREFIX);

			/* NewMouse wheels arrive as raw keys 0x7A (up) and 0x7B (down). */
			if (code == 0x7A || code == 0x7B)
			{
				if (ev.down)
				{
					ev.type = AMIGA_EV_WHEEL;
					ev.code = (code == 0x7A) ? 1 : -1;
					push(&ev);
				}
				break;
			}

			/* Held arrows scroll the map until they are let go (the game
			 * keeps the key's state): their repeats only queued scroll
			 * steps that went on after the key was up. */
			if ((msg->Qualifier & IEQUALIFIER_REPEAT) && code >= 0x4C && code <= 0x4F)
			{
				break;
			}

			ev.type = AMIGA_EV_KEY;
			ev.code = code;
			ev.ch   = ev.down ? key_char(msg) : 0;
			push(&ev);
			break;
		}

		case IDCMP_MOUSEMOVE:
			s_mouseX = msg->MouseX - ox;
			s_mouseY = msg->MouseY - oy;
			ev.type  = AMIGA_EV_MOUSEMOVE;
			ev.x     = s_mouseX;
			ev.y     = s_mouseY;
			push(&ev);
			break;

		case IDCMP_MOUSEBUTTONS:
			s_mouseX = msg->MouseX - ox;
			s_mouseY = msg->MouseY - oy;
			ev.type  = AMIGA_EV_MOUSEBUTTON;
			ev.x     = s_mouseX;
			ev.y     = s_mouseY;
			switch (msg->Code)
			{
			case SELECTDOWN: ev.code = 0; ev.down = 1; break;
			case SELECTUP:   ev.code = 0; ev.down = 0; break;
			case MENUDOWN:   ev.code = 1; ev.down = 1; break;
			case MENUUP:     ev.code = 1; ev.down = 0; break;
			case MIDDLEDOWN: ev.code = 2; ev.down = 1; break;
			case MIDDLEUP:   ev.code = 2; ev.down = 0; break;
			default:         ev.type = AMIGA_EV_NONE; break;
			}
			if (ev.type != AMIGA_EV_NONE) push(&ev);
			break;

		default:
			break;
		}

		ReplyMsg((struct Message *)got);
	}
}

int amiga_input_peek_mouse(int *x, int *y)
{
	int const oldX = s_mouseX, oldY = s_mouseY;

	/* Into the queue, as the main loop would take them: nothing is lost,
	 * only the position is read ahead. */
	drain();
	*x = s_mouseX;
	*y = s_mouseY;
	return s_mouseX != oldX || s_mouseY != oldY;
}

int amiga_input_poll(AmigaEvent *ev)
{
	if (s_count == 0) drain();

	if (s_count == 0)
	{
		if (amiga_break_requested())
		{
			memset(ev, 0, sizeof(*ev));
			ev->type = AMIGA_EV_QUIT;
			return 1;
		}
		return 0;
	}

	*ev = s_queue[s_head];
	s_head = (s_head + 1) % QUEUE_SIZE;
	--s_count;
	return 1;
}

void amiga_input_wait(uint32_t timeout_ms)
{
	struct Window * window = amiga_video_window();
	uint32_t const  end    = amiga_ticks_ms() + timeout_ms;

	/* Short sleeps with a look at the port between them: the game's frames are
	 * 33 ms, and a sleep of 4 ms is as fine as the timer allows without
	 * spinning (WarpOS: WaitTime never leaves the PPC). */
	while (amiga_ticks_ms() < end)
	{
		if (s_count > 0) return;
		if (window && port_has_messages(window->UserPort)) return;
		if (amiga_break_requested()) return;

		{
			uint32_t const left = end - amiga_ticks_ms();
			amiga_delay_ms(left < 4 ? left : 4);
		}
	}
}

void amiga_input_mouse(int *x, int *y)
{
	struct Window * window = amiga_video_window();
	if (window)
	{
		int ox, oy;
		amiga_video_offset(&ox, &oy);
		s_mouseX = window->MouseX - ox;
		s_mouseY = window->MouseY - oy;
	}
	*x = s_mouseX;
	*y = s_mouseY;
}
