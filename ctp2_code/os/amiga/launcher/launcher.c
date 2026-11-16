/* launcher.c -- the Call to Power II launcher for AmigaOS and MorphOS.
 *
 * A GadTools window on the Workbench screen that sets what the game reads
 * before it starts, then runs it:
 *   - the program: ctp2.060, ctp2.wos or ctp2.mos (those present), with
 *     C:InitPPC first for WarpOS on a Sonnet;
 *   - the screen mode: one of the card's 16-bit RTG modes the game accepts
 *     (ENV:CTP2_MODE), whose size becomes the game's resolution
 *     (ScreenResWidth/Height in userprofile.txt), or the automatic choice;
 *   - autosave on or off (AutoSave in userprofile.txt);
 *   - the ENV: settings of ctp2_code/os/amiga/README.md a player may want.
 *
 * It finds the game in PROGDIR:ctp2_code/ctp (the launcher at the top of the
 * CTP2 drawer) or in PROGDIR: itself, and keeps its own settings there in
 * launcher.prefs. ENV: variables at their default value are deleted, so the
 * game's own defaults apply. Built for the 68020 (Makefile.amiga68k
 * "launcher"), so one program serves every machine.
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <graphics/text.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <inline/macros.h>

/* CyberGraphX for the 68k compiler, whose NDK lacks it (as the game). */
#include "../amiga_cgx68k.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char version[] __attribute__((used)) = "$VER: CTP2 Launcher 1.0 (16.11.2026)";

struct IntuitionBase * IntuitionBase;
struct GfxBase *       GfxBase;
struct Library *       GadToolsBase;
struct Library *       CyberGfxBase;

/* ---- Settings ------------------------------------------------------------- */

enum { PROG_060, PROG_WOS, PROG_MOS, PROG_COUNT };
static const char * const s_progFile[PROG_COUNT] = { "ctp2.060", "ctp2.wos", "ctp2.mos" };
static const char * const s_progName[PROG_COUNT] = { "68060", "WarpOS (PowerPC)", "MorphOS" };

static const char * const s_langName[] = { "System (Locale)", "English", "Deutsch", "Fran\xe7" "ais", "Italiano", "Espa\xf1" "ol", "Polski", "Czech", NULL };
static const char * const s_langCode[] = { "",                "en",      "de",      "fr",                "it",       "es",               "pl",     "cs" };

static const char * const s_rateName[] = { "Default", "11025 Hz", "16000 Hz", "22050 Hz", "32000 Hz", "44100 Hz", NULL };
static const char * const s_rateCode[] = { "",        "11025",    "16000",    "22050",    "32000",    "44100" };

static const char * const s_preloadName[] = { "When first opened", "While a game loads", "When idle on the map", NULL };
static const char * const s_preloadCode[] = { "",                  "start",              "idle" };

struct Settings
{
	int  program;           /* PROG_* */
	int  initPPC;
	ULONG mode;             /* INVALID_ID: automatic */
	int  autosave;
	int  bgsave;
	int  savelevel;         /* 0..9 */
	int  language;          /* index in s_lang* */
	int  rate;              /* index in s_rate* */
	int  preload;           /* index in s_preload* */
	LONG spritecache;
	LONG scrub;
	LONG cardsettle;
	int  perf;
};

static struct Settings s_set;
static int  s_have[PROG_COUNT];
static int  s_haveInitPPC;
static BPTR s_gameDir;
static char s_gamePath[256];

/* ---- Small helpers -------------------------------------------------------- */

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c; }

static int prefix_nocase(const char *s, const char *p)
{
	for (; *p; ++s, ++p)
		if (lower((unsigned char)*s) != lower((unsigned char)*p)) return 0;
	return 1;
}

static int exists(const char *path)
{
	BPTR l = Lock((CONST_STRPTR)path, ACCESS_READ);
	if (!l) return 0;
	UnLock(l);
	return 1;
}

static void game_file(char *buf, int size, const char *name)
{
	strncpy(buf, s_gamePath, (size_t)size - 1);
	buf[size - 1] = '\0';
	AddPart((STRPTR)buf, (CONST_STRPTR)name, (ULONG)size);
}

static int env_get(const char *name, char *buf, int size)
{
	buf[0] = '\0';
	return GetVar((CONST_STRPTR)name, (STRPTR)buf, size, GVF_GLOBAL_ONLY) > 0;
}

static void env_set(const char *name, const char *value)
{
	if (value && *value)
		SetVar((CONST_STRPTR)name, (CONST_STRPTR)value, -1, GVF_GLOBAL_ONLY);
	else
		DeleteVar((CONST_STRPTR)name, GVF_GLOBAL_ONLY);
}

static void env_set_num(const char *name, LONG value, LONG deflt)
{
	char buf[16];
	if (value == deflt) { env_set(name, NULL); return; }
	sprintf(buf, "%ld", (long)value);
	env_set(name, buf);
}

static int index_of(const char * const *codes, int count, const char *code)
{
	int i;
	for (i = 0; i < count; ++i)
		if (strcmp(codes[i], code) == 0) return i;
	return 0;
}

/* A whole small text file into memory, NUL-terminated (NULL: none). */
static char *read_file(const char *path, LONG *length)
{
	BPTR  fh = Open((CONST_STRPTR)path, MODE_OLDFILE);
	char *buf;
	LONG  n;
	if (!fh) return NULL;
	buf = (char *)AllocVec(65536, MEMF_ANY);
	if (!buf) { Close(fh); return NULL; }
	n = Read(fh, buf, 65535);
	Close(fh);
	if (n < 0) n = 0;
	buf[n] = '\0';
	if (length) *length = n;
	return buf;
}

/* "Key=value" in a text, the value copied out. */
static int find_key(const char *text, const char *key, char *value, int size)
{
	size_t const klen = strlen(key);
	const char * line = text;
	while (line && *line)
	{
		if (prefix_nocase(line, key) && line[klen] == '=')
		{
			const char *v = line + klen + 1;
			int i = 0;
			while (v[i] && v[i] != '\n' && v[i] != '\r' && i < size - 1) { value[i] = v[i]; ++i; }
			value[i] = '\0';
			return 1;
		}
		line = strchr(line, '\n');
		if (line) ++line;
	}
	return 0;
}

/* ---- Defaults, launcher.prefs, ENV: and userprofile.txt ------------------- */

static int is_morphos(void)
{
	return FindResident((CONST_STRPTR)"MorphOS") != NULL;
}

static void defaults(void)
{
	char buf[64];
	char path[300];
	char *profile;
	int i;

	s_set.program = PROG_060;
	if (is_morphos() && s_have[PROG_MOS])                         s_set.program = PROG_MOS;
	else if (exists("LIBS:powerpc.library") && s_have[PROG_WOS])  s_set.program = PROG_WOS;
	else if (!s_have[PROG_060])
		for (i = 0; i < PROG_COUNT; ++i) if (s_have[i]) { s_set.program = i; break; }

	s_set.initPPC     = s_haveInitPPC;
	s_set.mode        = INVALID_ID;
	s_set.autosave    = 1;
	s_set.bgsave      = 1;
	s_set.savelevel   = 1;
	s_set.language    = 0;
	s_set.rate        = 0;
	s_set.preload     = 0;
	s_set.spritecache = 8;
	s_set.scrub       = 4;
	s_set.cardsettle  = 500;
	s_set.perf        = 0;

	/* What ENV: holds now (a player may have set some by hand). */
	if (env_get("CTP2_MODE", buf, sizeof(buf)))        s_set.mode = strtoul(buf, NULL, 16);
	if (env_get("CTP2_BGSAVE", buf, sizeof(buf)))      s_set.bgsave = buf[0] != '0';
	if (env_get("CTP2_SAVELEVEL", buf, sizeof(buf)))   s_set.savelevel = atoi(buf);
	if (env_get("CTP2_LANGUAGE", buf, sizeof(buf)))    s_set.language = index_of(s_langCode, 8, buf);
	if (env_get("CTP2_AHI_RATE", buf, sizeof(buf)))    s_set.rate = index_of(s_rateCode, 6, buf);
	if (env_get("CTP2_UIPRELOAD", buf, sizeof(buf)))   s_set.preload = index_of(s_preloadCode, 3, buf);
	if (env_get("CTP2_SPRITECACHE", buf, sizeof(buf))) s_set.spritecache = atol(buf);
	if (env_get("CTP2_SCRUB", buf, sizeof(buf)))       s_set.scrub = atol(buf);
	if (env_get("CTP2_CARDSETTLE", buf, sizeof(buf)))  s_set.cardsettle = atol(buf);
	if (env_get("CTP2_PERF", buf, sizeof(buf)))        s_set.perf = buf[0] != '0';

	game_file(path, sizeof(path), "userprofile.txt");
	profile = read_file(path, NULL);
	if (profile)
	{
		if (find_key(profile, "AutoSave", buf, sizeof(buf))) s_set.autosave = prefix_nocase(buf, "yes");
		FreeVec(profile);
	}
}

static void load_prefs(void)
{
	char path[300];
	char buf[64];
	char *text;
	int i;

	game_file(path, sizeof(path), "launcher.prefs");
	text = read_file(path, NULL);
	if (!text) return;

	if (find_key(text, "Program", buf, sizeof(buf)))
		for (i = 0; i < PROG_COUNT; ++i)
			if (strcmp(buf, s_progFile[i]) == 0 && s_have[i]) s_set.program = i;
	if (find_key(text, "InitPPC", buf, sizeof(buf)))     s_set.initPPC = atoi(buf) && s_haveInitPPC;
	if (find_key(text, "Mode", buf, sizeof(buf)))        s_set.mode = buf[0] ? strtoul(buf, NULL, 16) : INVALID_ID;
	if (find_key(text, "BgSave", buf, sizeof(buf)))      s_set.bgsave = atoi(buf);
	if (find_key(text, "SaveLevel", buf, sizeof(buf)))   s_set.savelevel = atoi(buf);
	if (find_key(text, "Language", buf, sizeof(buf)))    s_set.language = index_of(s_langCode, 8, buf);
	if (find_key(text, "AhiRate", buf, sizeof(buf)))     s_set.rate = index_of(s_rateCode, 6, buf);
	if (find_key(text, "UiPreload", buf, sizeof(buf)))   s_set.preload = index_of(s_preloadCode, 3, buf);
	if (find_key(text, "SpriteCache", buf, sizeof(buf))) s_set.spritecache = atol(buf);
	if (find_key(text, "Scrub", buf, sizeof(buf)))       s_set.scrub = atol(buf);
	if (find_key(text, "CardSettle", buf, sizeof(buf)))  s_set.cardsettle = atol(buf);
	if (find_key(text, "Perf", buf, sizeof(buf)))        s_set.perf = atoi(buf);
	FreeVec(text);

	if (s_set.savelevel < 0) s_set.savelevel = 0;
	if (s_set.savelevel > 9) s_set.savelevel = 9;
}

static void save_prefs(void)
{
	char path[300];
	char mode[16];
	char text[1024];
	BPTR fh;

	game_file(path, sizeof(path), "launcher.prefs");
	mode[0] = '\0';
	if (s_set.mode != INVALID_ID) sprintf(mode, "%08lx", (unsigned long)s_set.mode);
	sprintf(text,
	        "Program=%s\nInitPPC=%d\nMode=%s\nBgSave=%d\nSaveLevel=%d\nLanguage=%s\n"
	        "AhiRate=%s\nUiPreload=%s\nSpriteCache=%ld\nScrub=%ld\nCardSettle=%ld\nPerf=%d\n",
	        s_progFile[s_set.program], s_set.initPPC, mode, s_set.bgsave, s_set.savelevel,
	        s_langCode[s_set.language], s_rateCode[s_set.rate], s_preloadCode[s_set.preload],
	        (long)s_set.spritecache, (long)s_set.scrub, (long)s_set.cardsettle, s_set.perf);
	fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
	if (fh)
	{
		Write(fh, text, (LONG)strlen(text));
		Close(fh);
	}
}

static void apply_env(void)
{
	char buf[16];

	if (s_set.mode != INVALID_ID)
	{
		sprintf(buf, "%08lx", (unsigned long)s_set.mode);
		env_set("CTP2_MODE", buf);
	}
	else env_set("CTP2_MODE", NULL);

	env_set("CTP2_BGSAVE",    s_set.bgsave ? NULL : "0");
	env_set_num("CTP2_SAVELEVEL",   s_set.savelevel, 1);
	env_set("CTP2_LANGUAGE",  s_langCode[s_set.language]);
	env_set("CTP2_AHI_RATE",  s_rateCode[s_set.rate]);
	env_set("CTP2_UIPRELOAD", s_preloadCode[s_set.preload]);
	env_set_num("CTP2_SPRITECACHE", s_set.spritecache, 8);
	env_set_num("CTP2_SCRUB",       s_set.scrub, 4);
	env_set_num("CTP2_CARDSETTLE",  s_set.cardsettle, 500);
	env_set("CTP2_PERF",      s_set.perf ? "1" : NULL);
}

/* AutoSave and, with a chosen mode, ScreenResWidth/Height: the lines are
 * replaced or added, every other line is kept as it is. */
static void update_profile(void)
{
	char   path[300];
	char * old;
	char * out;
	char * o;
	const char * line;
	int    doneAuto = 0, doneW = 0, doneH = 0;
	int    setRes = (s_set.mode != INVALID_ID);
	ULONG  w = 0, h = 0;
	BPTR   fh;

	if (setRes)
	{
		w = GetCyberIDAttr(CYBRIDATTR_WIDTH, s_set.mode);
		h = GetCyberIDAttr(CYBRIDATTR_HEIGHT, s_set.mode);
		setRes = (w >= 800 && h >= 600);
	}

	game_file(path, sizeof(path), "userprofile.txt");
	old = read_file(path, NULL);
	out = (char *)AllocVec(65536 + 256, MEMF_ANY);
	if (!out) { if (old) FreeVec(old); return; }
	o = out;

	for (line = old ? old : ""; *line; )
	{
		const char * end = strchr(line, '\n');
		size_t const len = end ? (size_t)(end - line + 1) : strlen(line);

		if (prefix_nocase(line, "AutoSave="))
		{
			o += sprintf(o, "AutoSave=%s\n", s_set.autosave ? "Yes" : "No");
			doneAuto = 1;
		}
		else if (setRes && prefix_nocase(line, "ScreenResWidth="))
		{
			o += sprintf(o, "ScreenResWidth=%lu\n", (unsigned long)w);
			doneW = 1;
		}
		else if (setRes && prefix_nocase(line, "ScreenResHeight="))
		{
			o += sprintf(o, "ScreenResHeight=%lu\n", (unsigned long)h);
			doneH = 1;
		}
		else
		{
			memcpy(o, line, len);
			o += len;
			if (!end) *o++ = '\n';
		}
		line += len;
	}
	if (!doneAuto)          o += sprintf(o, "AutoSave=%s\n", s_set.autosave ? "Yes" : "No");
	if (setRes && !doneW)   o += sprintf(o, "ScreenResWidth=%lu\n", (unsigned long)w);
	if (setRes && !doneH)   o += sprintf(o, "ScreenResHeight=%lu\n", (unsigned long)h);

	fh = Open((CONST_STRPTR)path, MODE_NEWFILE);
	if (fh)
	{
		Write(fh, out, (LONG)(o - out));
		Close(fh);
	}
	FreeVec(out);
	if (old) FreeVec(old);
}

/* ---- Screen modes --------------------------------------------------------- */

#define MAX_MODES 64
static struct List s_modeList;
static struct Node s_modeNode[MAX_MODES + 1];
static char        s_modeText[MAX_MODES + 1][80];
static ULONG       s_modeId[MAX_MODES + 1];
static int         s_modeCount;

static int pixfmt_ok(ULONG fmt)
{
	switch (fmt)
	{
	case PIXFMT_RGB16: case PIXFMT_RGB16PC:
	case PIXFMT_BGR16: case PIXFMT_BGR16PC:
		return 1;
	default:
		return 0;
	}
}

static void list_modes(void)
{
	int i;
	s_modeList.lh_Head     = (struct Node *)&s_modeList.lh_Tail;
	s_modeList.lh_Tail     = NULL;
	s_modeList.lh_TailPred = (struct Node *)&s_modeList.lh_Head;

	strcpy(s_modeText[0], "Automatic (the game chooses)");
	s_modeId[0] = INVALID_ID;
	s_modeCount = 1;

	if (CyberGfxBase)
	{
		struct TagItem tags[] =
		{
			{ CYBRMREQ_MinDepth,  15 },
			{ CYBRMREQ_MaxDepth,  16 },
			{ CYBRMREQ_MinWidth,  800 },
			{ CYBRMREQ_MinHeight, 600 },
			{ TAG_DONE,           0 }
		};
		struct List * list = AllocCModeListTagList(tags);
		if (list)
		{
			struct CyberModeNode * node;
			for (node = (struct CyberModeNode *)list->lh_Head;
			     node->Node.ln_Succ && s_modeCount <= MAX_MODES;
			     node = (struct CyberModeNode *)node->Node.ln_Succ)
			{
				if (!pixfmt_ok(GetCyberIDAttr(CYBRIDATTR_PIXFMT, node->DisplayID))) continue;
				s_modeId[s_modeCount] = node->DisplayID;
				sprintf(s_modeText[s_modeCount], "%s", (const char *)node->ModeText);
				++s_modeCount;
			}
			FreeCModeList(list);
		}
	}

	for (i = 0; i < s_modeCount; ++i)
	{
		s_modeNode[i].ln_Name = s_modeText[i];
		AddTail(&s_modeList, &s_modeNode[i]);
	}
}

static int mode_index(ULONG id)
{
	int i;
	for (i = 1; i < s_modeCount; ++i)
		if (s_modeId[i] == id) return i;
	return 0;
}

/* ---- The window ----------------------------------------------------------- */

enum
{
	G_PROGRAM, G_INITPPC, G_MODES, G_AUTOSAVE, G_BGSAVE, G_SAVELEVEL, G_LANGUAGE,
	G_RATE, G_PRELOAD, G_SPRITECACHE, G_SCRUB, G_CARDSETTLE, G_PERF,
	G_PLAY, G_SAVE, G_QUIT, G_COUNT
};

static struct Window * s_win;
static struct Gadget * s_glist;
static struct Gadget * s_gad[G_COUNT];
static APTR            s_vi;
static const char *    s_progLabels[PROG_COUNT + 1];
static int             s_progMap[PROG_COUNT];     /* cycle index -> PROG_* */
static int             s_progCount;

static int prog_cycle_index(void)
{
	int i;
	for (i = 0; i < s_progCount; ++i)
		if (s_progMap[i] == s_set.program) return i;
	return 0;
}

static void update_ghosting(void)
{
	int const wos = (s_set.program == PROG_WOS);
	GT_SetGadgetAttrs(s_gad[G_INITPPC], s_win, NULL, GA_Disabled, (ULONG)(!wos || !s_haveInitPPC), TAG_DONE);
	GT_SetGadgetAttrs(s_gad[G_CARDSETTLE], s_win, NULL, GA_Disabled, (ULONG)!wos, TAG_DONE);
}

static LONG integer_of(struct Gadget *g)
{
	return ((struct StringInfo *)g->SpecialInfo)->LongInt;
}

static void read_integers(void)
{
	s_set.spritecache = integer_of(s_gad[G_SPRITECACHE]);
	s_set.scrub       = integer_of(s_gad[G_SCRUB]);
	s_set.cardsettle  = integer_of(s_gad[G_CARDSETTLE]);
	if (s_set.spritecache < 0) s_set.spritecache = 0;
	if (s_set.scrub < 0)       s_set.scrub = 0;
	if (s_set.cardsettle < 0)  s_set.cardsettle = 0;
}

static int open_window(void)
{
	static const char * const labels[] =
	{
		"Program", "Run C:InitPPC first", "Screen mode", "Autosave each turn",
		"Autosave in the background", "Save compression", "Language", "Sound rate",
		"Build the windows", "Unit sprites kept", "Card refresh rows", "Card settle (ms)",
		"Performance log"
	};
	struct Screen *   scr;
	struct NewGadget  ng;
	struct Gadget *   g;
	struct RastPort * rp;
	UWORD fontH, rowH, labelW = 0, fieldW, x0, y, innerW, innerH, gap;
	int   i;

	scr = LockPubScreen(NULL);
	if (!scr) return 0;
	s_vi = GetVisualInfo(scr, TAG_DONE);
	if (!s_vi) { UnlockPubScreen(NULL, scr); return 0; }

	rp     = &scr->RastPort;
	fontH  = scr->Font->ta_YSize;
	rowH   = (UWORD)(fontH + 6);
	gap    = 4;
	for (i = 0; i < (int)(sizeof(labels) / sizeof(labels[0])); ++i)
	{
		UWORD const w = (UWORD)TextLength(rp, (CONST_STRPTR)labels[i], (ULONG)strlen(labels[i]));
		if (w > labelW) labelW = w;
	}
	fieldW = (UWORD)TextLength(rp, (CONST_STRPTR)"RADEON: 1024x768 16Bit PC-Format (screen)", 41);
	if (fieldW < 200) fieldW = 200;
	x0     = (UWORD)(scr->WBorLeft + 8 + labelW + 8);

	s_glist = NULL;
	g = CreateContext(&s_glist);

	memset(&ng, 0, sizeof(ng));
	ng.ng_TextAttr   = scr->Font;
	ng.ng_VisualInfo = s_vi;
	ng.ng_Flags      = PLACETEXT_LEFT;
	ng.ng_LeftEdge   = x0;
	ng.ng_Width      = fieldW;
	ng.ng_Height     = rowH;
	y = (UWORD)(scr->WBorTop + fontH + 1 + 6);

#define ROW(id, kind, ...) do {                                   \
		ng.ng_TopEdge    = y;                                     \
		ng.ng_GadgetText = (UBYTE *)labels[id];                   \
		ng.ng_GadgetID   = id;                                    \
		g = s_gad[id] = CreateGadget(kind, g, &ng, __VA_ARGS__);  \
		y = (UWORD)(y + ng.ng_Height + gap);                      \
	} while (0)

	ROW(G_PROGRAM, CYCLE_KIND, GTCY_Labels, (ULONG)s_progLabels, GTCY_Active, (ULONG)prog_cycle_index(), TAG_DONE);
	ng.ng_Width = 26;
	ROW(G_INITPPC, CHECKBOX_KIND, GTCB_Checked, (ULONG)s_set.initPPC, GTCB_Scaled, TRUE, TAG_DONE);
	ng.ng_Width = fieldW;

	ng.ng_Height = (UWORD)(fontH * 7 + 4);
	ROW(G_MODES, LISTVIEW_KIND, GTLV_Labels, (ULONG)&s_modeList, GTLV_ShowSelected, 0,
	    GTLV_Selected, (ULONG)mode_index(s_set.mode), GTLV_MakeVisible, (ULONG)mode_index(s_set.mode), TAG_DONE);
	ng.ng_Height = rowH;

	ng.ng_Width = 26;
	ROW(G_AUTOSAVE, CHECKBOX_KIND, GTCB_Checked, (ULONG)s_set.autosave, GTCB_Scaled, TRUE, TAG_DONE);
	ROW(G_BGSAVE,   CHECKBOX_KIND, GTCB_Checked, (ULONG)s_set.bgsave,   GTCB_Scaled, TRUE, TAG_DONE);
	ng.ng_Width = (UWORD)(fieldW - 40);
	ROW(G_SAVELEVEL, SLIDER_KIND, GTSL_Min, 0, GTSL_Max, 9, GTSL_Level, (ULONG)s_set.savelevel,
	    GTSL_LevelFormat, (ULONG)"%ld", GTSL_MaxLevelLen, 2, GTSL_LevelPlace, PLACETEXT_RIGHT,
	    GA_RelVerify, TRUE, TAG_DONE);
	ng.ng_Width = fieldW;
	ROW(G_LANGUAGE, CYCLE_KIND, GTCY_Labels, (ULONG)s_langName,    GTCY_Active, (ULONG)s_set.language, TAG_DONE);
	ROW(G_RATE,     CYCLE_KIND, GTCY_Labels, (ULONG)s_rateName,    GTCY_Active, (ULONG)s_set.rate,     TAG_DONE);
	ROW(G_PRELOAD,  CYCLE_KIND, GTCY_Labels, (ULONG)s_preloadName, GTCY_Active, (ULONG)s_set.preload,  TAG_DONE);
	ng.ng_Width = 80;
	ROW(G_SPRITECACHE, INTEGER_KIND, GTIN_Number, (ULONG)s_set.spritecache, GTIN_MaxChars, 4, TAG_DONE);
	ROW(G_SCRUB,       INTEGER_KIND, GTIN_Number, (ULONG)s_set.scrub,       GTIN_MaxChars, 4, TAG_DONE);
	ROW(G_CARDSETTLE,  INTEGER_KIND, GTIN_Number, (ULONG)s_set.cardsettle,  GTIN_MaxChars, 5, TAG_DONE);
	ng.ng_Width = 26;
	ROW(G_PERF, CHECKBOX_KIND, GTCB_Checked, (ULONG)s_set.perf, GTCB_Scaled, TRUE, TAG_DONE);
#undef ROW

	/* The buttons, across the bottom. */
	{
		UWORD const left  = (UWORD)(scr->WBorLeft + 8);
		UWORD const total = (UWORD)(x0 + fieldW - left);
		UWORD const bw    = (UWORD)((total - 2 * 8) / 3);
		static const char * const names[3] = { "_Play", "_Save", "_Quit" };
		y = (UWORD)(y + 4);
		ng.ng_Flags  = PLACETEXT_IN;
		ng.ng_Width  = bw;
		ng.ng_Height = rowH;
		ng.ng_TopEdge = y;
		for (i = 0; i < 3; ++i)
		{
			ng.ng_LeftEdge   = (UWORD)(left + i * (bw + 8));
			ng.ng_GadgetText = (UBYTE *)names[i];
			ng.ng_GadgetID   = G_PLAY + i;
			g = s_gad[G_PLAY + i] = CreateGadget(BUTTON_KIND, g, &ng, GT_Underscore, '_', TAG_DONE);
		}
		y = (UWORD)(y + rowH + 6);
	}

	if (!g)
	{
		FreeGadgets(s_glist);
		FreeVisualInfo(s_vi);
		UnlockPubScreen(NULL, scr);
		return 0;
	}

	innerW = (UWORD)(x0 + fieldW + 8 - scr->WBorLeft);
	innerH = (UWORD)(y - (scr->WBorTop + fontH + 1));
	s_win = OpenWindowTags(NULL,
		WA_Title,       (ULONG)"Call to Power II",
		WA_ScreenTitle, (ULONG)(version + 6),
		WA_InnerWidth,  innerW,
		WA_InnerHeight, innerH,
		WA_AutoAdjust,  TRUE,
		WA_DragBar,     TRUE,
		WA_DepthGadget, TRUE,
		WA_CloseGadget, TRUE,
		WA_Activate,    TRUE,
		WA_PubScreen,   (ULONG)scr,
		WA_Gadgets,     (ULONG)s_glist,
		WA_IDCMP,       IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_VANILLAKEY | IDCMP_RAWKEY |
		                BUTTONIDCMP | CHECKBOXIDCMP | CYCLEIDCMP | SLIDERIDCMP |
		                LISTVIEWIDCMP | INTEGERIDCMP,
		TAG_DONE);
	UnlockPubScreen(NULL, scr);
	if (!s_win)
	{
		FreeGadgets(s_glist);
		FreeVisualInfo(s_vi);
		return 0;
	}
	GT_RefreshWindow(s_win, NULL);
	update_ghosting();
	return 1;
}

static void close_window(void)
{
	if (s_win)   { CloseWindow(s_win); s_win = NULL; }
	if (s_glist) { FreeGadgets(s_glist); s_glist = NULL; }
	if (s_vi)    { FreeVisualInfo(s_vi); s_vi = NULL; }
}

/* 1: play, 0: quit. */
static int run_window(void)
{
	for (;;)
	{
		struct IntuiMessage * msg;
		WaitPort(s_win->UserPort);
		while ((msg = GT_GetIMsg(s_win->UserPort)) != NULL)
		{
			ULONG const     cls  = msg->Class;
			UWORD const     code = msg->Code;
			struct Gadget * gad  = (struct Gadget *)msg->IAddress;
			GT_ReplyIMsg(msg);

			switch (cls)
			{
			case IDCMP_CLOSEWINDOW:
				return 0;

			case IDCMP_REFRESHWINDOW:
				GT_BeginRefresh(s_win);
				GT_EndRefresh(s_win, TRUE);
				break;

			case IDCMP_RAWKEY:
				/* Cursor up and down: the previous or next screen mode. */
				if (code == 0x4C || code == 0x4D)
				{
					int i = mode_index(s_set.mode);
					if (code == 0x4C && i > 0)               --i;
					if (code == 0x4D && i < s_modeCount - 1) ++i;
					s_set.mode = s_modeId[i];
					GT_SetGadgetAttrs(s_gad[G_MODES], s_win, NULL,
					                  GTLV_Selected, (ULONG)i, GTLV_MakeVisible, (ULONG)i, TAG_DONE);
				}
				break;

			case IDCMP_VANILLAKEY:
				switch (lower(code))
				{
				case 'p': case '\r': read_integers(); return 1;
				case 's': read_integers(); save_prefs(); update_profile(); apply_env(); break;
				case 'q': case 27: return 0;
				}
				break;

			case IDCMP_GADGETUP:
			case IDCMP_MOUSEMOVE:
				switch (gad->GadgetID)
				{
				case G_PROGRAM:   s_set.program = s_progMap[code]; update_ghosting(); break;
				case G_INITPPC:   s_set.initPPC = (gad->Flags & GFLG_SELECTED) != 0; break;
				case G_MODES:     s_set.mode = s_modeId[code]; break;
				case G_AUTOSAVE:  s_set.autosave = (gad->Flags & GFLG_SELECTED) != 0; break;
				case G_BGSAVE:    s_set.bgsave = (gad->Flags & GFLG_SELECTED) != 0; break;
				case G_SAVELEVEL: s_set.savelevel = (int)(WORD)code; break;
				case G_LANGUAGE:  s_set.language = code; break;
				case G_RATE:      s_set.rate = code; break;
				case G_PRELOAD:   s_set.preload = code; break;
				case G_PERF:      s_set.perf = (gad->Flags & GFLG_SELECTED) != 0; break;
				case G_PLAY:      read_integers(); return 1;
				case G_SAVE:      read_integers(); save_prefs(); update_profile(); apply_env(); break;
				case G_QUIT:      return 0;
				}
				break;
			}
		}
	}
}

/* ---- Running the game ----------------------------------------------------- */

static LONG run_command(const char *cmd, const char *outPath)
{
	BPTR in  = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
	BPTR out = Open((CONST_STRPTR)(outPath ? outPath : "NIL:"), MODE_NEWFILE);
	LONG rc;
	if (!out) out = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
	rc = SystemTags((CONST_STRPTR)cmd,
	                SYS_Input,  (ULONG)in,
	                SYS_Output, (ULONG)out,
	                NP_StackSize, 65536,
	                TAG_DONE);
	if (in)  Close(in);
	if (out) Close(out);
	return rc;
}

static void play(void)
{
	char cmd[400];
	char exe[300];
	char outPath[300];
	BPTR oldDir;

	save_prefs();
	update_profile();
	apply_env();

	oldDir = CurrentDir(s_gameDir);
	if (s_set.program == PROG_WOS && s_set.initPPC && s_haveInitPPC)
		run_command("C:InitPPC", NULL);

	game_file(exe, sizeof(exe), s_progFile[s_set.program]);
	game_file(outPath, sizeof(outPath), "ctp2out.txt");
	sprintf(cmd, "\"%s\" nointromovie", exe);
	run_command(cmd, outPath);
	CurrentDir(oldDir);
}

/* ---- main ----------------------------------------------------------------- */

static void message(const char *text)
{
	struct EasyStruct es = { sizeof(struct EasyStruct), 0, (UBYTE *)"Call to Power II", (UBYTE *)text, (UBYTE *)"OK" };
	if (IntuitionBase) EasyRequestArgs(NULL, &es, NULL, NULL);
}

int main(void)
{
	int i, result = 20, doPlay = 0;

	IntuitionBase = (struct IntuitionBase *)OpenLibrary((CONST_STRPTR)"intuition.library", 39);
	GfxBase       = (struct GfxBase *)OpenLibrary((CONST_STRPTR)"graphics.library", 39);
	GadToolsBase  = OpenLibrary((CONST_STRPTR)"gadtools.library", 39);
	CyberGfxBase  = OpenLibrary((CONST_STRPTR)CYBERGFXNAME, 41);
	if (!IntuitionBase || !GfxBase || !GadToolsBase) goto out;

	if (!CyberGfxBase)
	{
		message("Call to Power II needs cybergraphics.library V41\n(Picasso96 or CyberGraphX) and an RTG card.");
		goto out;
	}

	/* The game's drawer: PROGDIR:ctp2_code/ctp or PROGDIR: itself. */
	s_gameDir = Lock((CONST_STRPTR)"PROGDIR:ctp2_code/ctp", ACCESS_READ);
	if (!s_gameDir) s_gameDir = Lock((CONST_STRPTR)"PROGDIR:", ACCESS_READ);
	if (!s_gameDir || !NameFromLock(s_gameDir, (STRPTR)s_gamePath, sizeof(s_gamePath)))
		goto out;

	for (i = 0; i < PROG_COUNT; ++i)
	{
		char path[300];
		game_file(path, sizeof(path), s_progFile[i]);
		s_have[i] = exists(path);
		if (s_have[i])
		{
			s_progMap[s_progCount] = i;
			s_progLabels[s_progCount++] = s_progName[i];
		}
	}
	s_progLabels[s_progCount] = NULL;
	if (!s_progCount)
	{
		message("No ctp2.060, ctp2.wos or ctp2.mos found\nin ctp2_code/ctp beside the launcher.");
		goto out;
	}
	s_haveInitPPC = exists("C:InitPPC");

	defaults();
	load_prefs();
	list_modes();

	if (!open_window()) goto out;
	doPlay = run_window();
	close_window();
	if (doPlay) play();
	result = 0;

out:
	close_window();
	if (s_gameDir)     UnLock(s_gameDir);
	if (CyberGfxBase)  CloseLibrary(CyberGfxBase);
	if (GadToolsBase)  CloseLibrary(GadToolsBase);
	if (GfxBase)       CloseLibrary((struct Library *)GfxBase);
	if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
	return result;
}
