# What Makefile.amiga68k and Makefile.warpos share: defines, include paths,
# the object list and per-file flags. Each makefile adds its CPU, optimiser
# and link flags, and sets OBJDIR, CTP2 and GEN before including this.

# ---- Defines ----------------------------------------------------------------
# HAVE_CONFIG_H selects ctp2_code/os/amiga/config.h (ctp2_config.h includes
# "config.h" from the first include directory that has one: os/amiga).
AMIGA_DEFS := -DCTP2_AMIGA_BUILD=1 -DHAVE_CONFIG_H=1 -D__AMIGA__=1 -DNDEBUG \
              -Uamiga -Uamigaos -UAMIGA \
              -include $(CTP2)/os/amiga/amiga_prelude.h

# ---- Include paths: os/amiga first for config.h, then the autotools list -----
# (ctp2_code/os/autoconf/Makefile.common, CTP2_CPPFLAGS), ui/aui_amiga in
# place of ui/aui_sdl.
AMIGA_INCS := \
	-I$(CTP2)/os/amiga -I$(CTP2)/os/amiga/include \
	-I$(CTP2)/os/nowin32 -I$(CTP2) -I$(CTP2)/os/include \
	-I$(CTP2)/ctp -I$(CTP2)/ctp/ctp2_utils -I$(CTP2)/ctp/ctp2_rsrc \
	-I$(CTP2)/ctp/debugtools \
	-I$(CTP2)/ui/interface -I$(CTP2)/ui/netshell \
	-I$(CTP2)/robot/utility -I$(CTP2)/robot/pathing -I$(CTP2)/robot/aibackdoor \
	-I$(CTP2)/gfx/spritesys -I$(CTP2)/gfx/tilesys -I$(CTP2)/gfx/gfx_utils \
	-I$(CTP2)/gs/database -I$(CTP2)/gs/fileio -I$(CTP2)/gs/gameobj \
	-I$(CTP2)/gs/utility -I$(CTP2)/gs/world \
	-I$(CTP2)/net/io -I$(CTP2)/net/general \
	-I$(CTP2)/ui/aui_utils -I$(CTP2)/ui/aui_amiga -I$(CTP2)/ui/aui_ctp2 \
	-I$(CTP2)/ui/aui_common \
	-I$(CTP2)/libs/anet/h \
	-I$(CTP2)/gs/slic -I$(CTP2)/gfx/layers -I$(CTP2)/mapgen \
	-I$(CTP2)/ui/freetype -I$(CTP2)/sound -I$(CTP2)/ctp/fingerprint \
	-I$(CTP2)/ui/slic_debug -I$(CTP2)/gs/outcom -I$(CTP2)/ui/ldl \
	-I$(CTP2)/gs/events -I$(CTP2)/gs/newdb \
	-I$(CTP2)/ai/diplomacy -I$(CTP2)/ai/mapanalysis \
	-I$(CTP2)/ai/strategy/scheduler -I$(CTP2)/ai/strategy/agents \
	-I$(CTP2)/ai/strategy/goals -I$(CTP2)/ai/strategy/squads \
	-I$(CTP2)/ai -I$(CTP2)/ai/CityManagement \
	-I$(CTP2)/libs/zlib-1.3.1 \
	$(GEN_INCS)

# ---- Language flags -----------------------------------------------------------
# The autotools build's -fpermissive -fms-extensions are kept: the code relies
# on both. No exceptions (no unwinder on WarpOS; the same on the 68060 keeps the
# two builds alike), RTTI stays on for the four dynamic_casts.
AMIGA_COMMON_FLAGS := -MMD -MP -fno-strict-aliasing -fmessage-length=0 \
	-Wall -Wno-unused-variable -Wno-unused-function -Wno-parentheses \
	-Wno-comment -Wno-char-subscripts -Wno-unknown-pragmas \
	-Wno-sign-compare -Wno-misleading-indentation -Wno-unused-but-set-variable
AMIGA_C_FLAGS   := -std=gnu99 -fms-extensions
AMIGA_CXX_FLAGS := -std=gnu++11 -fpermissive -fms-extensions -fno-exceptions -frtti \
	-Wno-class-memaccess -Wno-deprecated-declarations -Wno-reorder \
	-Wno-write-strings -Wno-narrowing

# ---- Platform sources (ctp2_code-relative) ------------------------------------
AMIGA_PLATFORM_SRCS := \
	os/amiga/amiga_platform.c \
	os/amiga/amiga_clock.c \
	os/amiga/amiga_log.c \
	os/amiga/amiga_video.c \
	os/amiga/amiga_input.c \
	os/amiga/amiga_audio.c \
	os/amiga/amiga_mixer.cpp \
	os/amiga/amiga_posix.c \
	os/amiga/amiga_malloc.c \
	os/amiga/amiga_dl.c \
	os/amiga/anet_stub.c \
	ui/aui_amiga/aui_amiga.cpp \
	ui/aui_amiga/aui_amigaui.cpp \
	ui/aui_amiga/aui_amigasurface.cpp \
	ui/aui_amiga/aui_amigamouse.cpp \
	ui/aui_amiga/aui_amigakeyboard.cpp

# ---- Vendored libraries ---------------------------------------------------------
# FreeType 1.3.1 as one file (its own AmigaOS port), zlib for compressed saves.
AMIGA_FREETYPE_SRCS := libs/freetype-1.3.1/lib/arch/amigaos/freetype.c
AMIGA_ZLIB_SRCS := $(addprefix libs/zlib-1.3.1/, \
	adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c \
	gzwrite.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)

AMIGA_FILE_FLAGS_libs/freetype-1.3.1/lib/arch/amigaos/freetype := \
	-I$(CTP2)/libs/freetype-1.3.1/lib/arch/amigaos -I$(CTP2)/libs/freetype-1.3.1/lib -w
$(foreach f,$(AMIGA_ZLIB_SRCS),$(eval AMIGA_FILE_FLAGS_$(f:.c=) := -w))

# ---- Map generators, linked in under their own names (os/amiga/amiga_dl.c) ----
AMIGA_FILE_FLAGS_mapgen/Crater := -DCoCreateMapGenerator=CoCreateMapGenerator_crater
AMIGA_FILE_FLAGS_mapgen/FaultGen := -DCoCreateMapGenerator=CoCreateMapGenerator_fault
AMIGA_FILE_FLAGS_mapgen/PlasmaGen2 := -DCoCreateMapGenerator=CoCreateMapGenerator_plasma2
AMIGA_FILE_FLAGS_mapgen/Geometric := -DCoCreateMapGenerator=CoCreateMapGenerator_geometric
# The geometric plugin is Geometric.cpp plus its own copy of FaultGen.cpp built
# with -DGEOMETRIC (mapgen/Makefile.am); its classes get their own names so
# they don't clash with the fault plugin's.
AMIGA_FILE_FLAGS_mapgen/FaultGen_geometric := -DGEOMETRIC \
	-DCoCreateMapGenerator=CoCreateMapGenerator_geometric_fault \
	-DFaultGenerator=FaultGenerator_geometric -DShift=Shift_geometric

# ---- Objects ------------------------------------------------------------------------
AMIGA_CTP2_SRCS := $(AMIGA_GAME_SRCS) $(AMIGA_PLATFORM_SRCS) $(AMIGA_MAPGEN_SRCS) \
	$(AMIGA_FREETYPE_SRCS) $(AMIGA_ZLIB_SRCS)

AMIGA_OBJECTS := \
	$(addprefix $(OBJDIR)/,$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(AMIGA_CTP2_SRCS)))) \
	$(OBJDIR)/mapgen/FaultGen_geometric.o \
	$(patsubst $(GEN)/%,$(OBJDIR)/gen/%,$(patsubst %.cpp,%.o,$(patsubst %.c,%.o,$(GEN_SRCS))))

# What runs on every C++ object after the compiler (Makefile.warpos: strip the
# COMDAT groups); nothing on the 68060.
AMIGA_CXX_POST ?= true

# FaultGen.cpp compiled a second time for the geometric plugin.
$(OBJDIR)/mapgen/FaultGen_geometric.o: $(CTP2)/mapgen/FaultGen.cpp $(BUILD_FLAGS_STAMP) | $(GEN_STAMP)
	@mkdir -p $(dir $@)
	@echo "  CXX $< (geometric)"
	@$(CXX) $(CXXFLAGS) $(AMIGA_FILE_FLAGS_mapgen/FaultGen_geometric) -c $< -o $@
	@$(AMIGA_CXX_POST) $@

# Every source this list names must exist: a renamed file must not drop out of
# the build silently.
$(foreach f,$(AMIGA_GAME_SRCS) $(AMIGA_MAPGEN_SRCS) $(AMIGA_FREETYPE_SRCS) $(AMIGA_ZLIB_SRCS),$(if $(wildcard $(CTP2)/$(f)),,$(error os/amiga/sources.mk names $(f), which does not exist)))
