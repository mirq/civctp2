/*
 * The Amiga platform layer: startup, libraries, the program's drawer, CTRL-C,
 * the preferred language and requesters (amiga_platform.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amiga_os.h"
#include "amiga_platform.h"

struct IntuitionBase *IntuitionBase;
struct GfxBase       *GfxBase;
struct Library       *CyberGfxBase;
struct Library       *KeymapBase;
#if defined(CTP2_AMIGA_MOS)
struct Library       *LocaleBase;
#else
struct LocaleBase    *LocaleBase;
#endif

#if !defined(CTP2_AMIGA_WOS)
/* libnix's stack cookie: when the Shell's stack is smaller, the startup
 * (swapstack.o, pulled in by Makefile.amiga68k) allocates one this big with
 * AllocVec, switches to it with StackSwap before main() and frees it at exit:
 * no "stack" command is needed before running the game. 2 MB, what the game
 * has always been tested with; its parsers and the AI recurse deeply. The
 * WarpOS loader gives its own 1 MB stack. */
unsigned long __stack = 2048UL * 1024UL;
#endif

/* Defined in amiga_clock.c, amiga_log.c, amiga_video.c, amiga_audio.c. */
int  amiga_clock_init(void);
void amiga_clock_exit(void);
void amiga_log_exit(void);

static BPTR s_oldDir;
static BPTR s_progDir;
static APTR s_oldWindowPtr;
static int  s_windowPtrSet;
static int  s_inited;
static int  s_break;

int amiga_platform_init(void)
{
	if (s_inited) return 1;

	IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
	GfxBase       = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
	KeymapBase    = OpenLibrary((CONST_STRPTR)"keymap.library", 37);
	LocaleBase    = (void *)OpenLibrary((CONST_STRPTR)"locale.library", 38);
	CyberGfxBase  = OpenLibrary((CONST_STRPTR)CYBERGFXNAME, 41);

	if (!IntuitionBase || !GfxBase || !KeymapBase)
	{
		fprintf(stderr, "Call to Power II needs AmigaOS 3.0 or newer (intuition, graphics, keymap V39).\n");
		amiga_platform_exit();
		return 0;
	}

	if (!CyberGfxBase)
	{
		fprintf(stderr, "Call to Power II needs cybergraphics.library V41 (Picasso96 or CyberGraphX) and an RTG card.\n");
		amiga_platform_exit();
		return 0;
	}

	if (!amiga_clock_init())
	{
		fprintf(stderr, "Call to Power II could not open timer.device.\n");
		amiga_platform_exit();
		return 0;
	}

	/* Everything the game reads and writes is relative to its own drawer, as
	 * the other ports run from theirs. Lock + CurrentDir, never newlib's chdir
	 * (on WarpOS it unlocked the Shell's own directory lock). */
	s_progDir = Lock((CONST_STRPTR)"PROGDIR:", ACCESS_READ);
	if (s_progDir)
	{
		s_oldDir = CurrentDir(s_progDir);
	}

	/* No "Please insert volume" requesters: the game looks files up in
	 * several places and expects a miss to fail quietly, as on the other
	 * systems (a requester on the Workbench screen also stops the game
	 * behind its own screen). PROGDIR: above is locked before this. */
	{
		struct Process * self = (struct Process *)FindTask(NULL);
		s_oldWindowPtr = self->pr_WindowPtr;
		self->pr_WindowPtr = (APTR)-1;
		s_windowPtrSet = 1;
	}

	s_inited = 1;

	amiga_log("Call to Power II for AmigaOS (%s), started\n",
#if defined(CTP2_AMIGA_WOS)
	          "WarpOS"
#elif defined(CTP2_AMIGA_MOS)
	          "MorphOS"
#else
	          "68060"
#endif
	         );

	return 1;
}

void amiga_platform_exit(void)
{
	amiga_audio_close();
	amiga_video_close();
	amiga_log_exit();

	if (s_windowPtrSet)
	{
		((struct Process *)FindTask(NULL))->pr_WindowPtr = s_oldWindowPtr;
		s_windowPtrSet = 0;
	}

	if (s_progDir)
	{
		CurrentDir(s_oldDir);
		UnLock(s_progDir);
		s_progDir = 0;
	}

	amiga_clock_exit();

	if (CyberGfxBase)  { CloseLibrary(CyberGfxBase); CyberGfxBase = NULL; }
	if (LocaleBase)    { CloseLibrary((struct Library *)LocaleBase); LocaleBase = NULL; }
	if (KeymapBase)    { CloseLibrary(KeymapBase);   KeymapBase = NULL; }
	if (GfxBase)       { CloseLibrary((struct Library *)GfxBase); GfxBase = NULL; }
	if (IntuitionBase) { CloseLibrary((struct Library *)IntuitionBase); IntuitionBase = NULL; }

	s_inited = 0;
}

int amiga_break_requested(void)
{
	/* One look at the task's signals, no message: SetSignal is cheap. */
	if (!s_break && (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C))
	{
		s_break = 1;
		amiga_log("CTRL-C: quitting\n");
	}
	return s_break;
}

/* ---- Language ------------------------------------------------------------- */

/* Locale preferences name languages in their own words (Latin-1); the game's
 * LanguageRecords carry ISO 639-1 codes (gamedata/Language.txt). ASCII
 * prefixes, so "français", "español" and "português" match too. */
static const struct
{
	const char * prefix;
	const char * iso;
} s_languages[] =
{
	{ "english",    "en" }, { "deutsch",    "de" }, { "fran",       "fr" },
	{ "italiano",   "it" }, { "espa",       "es" }, { "polski",     "pl" },
	{ "czech",      "cs" }, { "nederlands", "nl" }, { "svenska",    "sv" },
	{ "dansk",      "da" }, { "norsk",      "no" }, { "suomi",      "fi" },
	{ "portu",      "pt" }, { "magyar",     "hu" }, { "russian",    "ru" },
	{ "japanese",   "ja" }, { "chinese",    "zh" },
};

static int has_prefix_nocase(const char *s, const char *prefix)
{
	for (; *prefix; ++s, ++prefix)
	{
		char a = *s, b = *prefix;
		if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
		if (a != b) return 0;
	}
	return 1;
}

const char *amiga_preferred_language(int n)
{
	static char env[8];

	int index = 0;

	/* ENV:CTP2_LANGUAGE first. */
	if (GetVar((CONST_STRPTR)"CTP2_LANGUAGE", (STRPTR)env, sizeof(env), GVF_GLOBAL_ONLY) > 0)
	{
		if (n == index) return env;
		++index;
	}

	if (LocaleBase)
	{
		struct Locale * locale = OpenLocale(NULL);
		if (locale)
		{
			const char * found = NULL;
			int i;
			for (i = 0; i < 10 && locale->loc_PrefLanguages[i] && !found; ++i)
			{
				const char * name = (const char *)locale->loc_PrefLanguages[i];
				size_t       k;
				for (k = 0; k < sizeof(s_languages) / sizeof(s_languages[0]); ++k)
				{
					if (has_prefix_nocase(name, s_languages[k].prefix))
					{
						if (n == index) found = s_languages[k].iso;
						++index;
						break;
					}
				}
			}
			CloseLocale(locale);
			if (found) return found;
		}
	}

	return NULL;
}

/* ---- Requesters ------------------------------------------------------------- */

int amiga_message_box(const char *title, const char *text, const char *buttons)
{
	struct EasyStruct es;
	char   body[512];
	int    count = 1;
	const char * p;
	LONG   r;

	if (!IntuitionBase)
	{
		fprintf(stderr, "%s: %s\n", title ? title : "", text ? text : "");
		return 1;
	}

	/* EasyRequest formats its text: a % in the game's message must not be. */
	{
		size_t o = 0;
		const char * s = text ? text : "";
		for (; *s && o + 2 < sizeof(body); ++s)
		{
			if (*s == '%') body[o++] = '%';
			body[o++] = *s;
		}
		body[o] = 0;
	}

	for (p = buttons; *p; ++p)
	{
		if (*p == '|') ++count;
	}

	es.es_StructSize   = sizeof(es);
	es.es_Flags        = 0;
	es.es_Title        = (UBYTE *)(title ? title : "Call to Power II");
	es.es_TextFormat   = (UBYTE *)body;
	es.es_GadgetFormat = (UBYTE *)buttons;

	amiga_log("Requester: %s: %s\n", title ? title : "", text ? text : "");
	amiga_log_flush();

	r = EasyRequestArgs(NULL, &es, NULL, NULL);
	amiga_log("Requester answered %ld\n", (long)r);

	/* Intuition: 1, 2, ... from the left, 0 for the rightmost. */
	return (r == 0) ? count : (int)r;
}
