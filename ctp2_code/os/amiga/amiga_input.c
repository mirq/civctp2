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

static int port_has_messages(struct MsgPort *port)
{
	/* exec's IsMsgPortEmpty(), which is a macro over this. On WarpOS the 68k
	 * writes the list: the PPC's copy of those lines goes first (a no-op on
	 * the 68060). */
	amiga_cache_flush(&port->mp_MsgList, sizeof(port->mp_MsgList));
	if (port->mp_MsgList.lh_Head)
	{
		amiga_cache_flush(port->mp_MsgList.lh_Head, sizeof(struct Node));
	}
	return port->mp_MsgList.lh_Head->ln_Succ != NULL;
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
	ie.ie_EventAddress = msg->IAddress ? *(APTR *)msg->IAddress : NULL; /* dead keys */

	n = MapRawKey(&ie, (STRPTR)buf, sizeof(buf), NULL);
	return (n == 1) ? buf[0] : 0;
}

static void drain(void)
{
	struct Window *       window = amiga_video_window();
	struct IntuiMessage * msg;
	int                   ox, oy;

	if (!window || !port_has_messages(window->UserPort)) return;

	amiga_video_offset(&ox, &oy);

	while ((msg = (struct IntuiMessage *)GetMsg(window->UserPort)) != NULL)
	{
		AmigaEvent ev;
		memset(&ev, 0, sizeof(ev));
		ev.time_ms   = amiga_ticks_ms();
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

		ReplyMsg((struct Message *)msg);
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
