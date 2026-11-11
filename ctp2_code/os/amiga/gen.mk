# Generated sources for the Amiga builds (Makefile.amiga68k, Makefile.warpos).
#
# Everything here runs on the Linux host: the database code generator (ctpdb)
# is built with the host compiler and run on gs/newdb/*.cdb, and the lexers and
# parsers come from the host's flex and yacc. Both cross builds then compile
# the same files from $(GEN).
#
# The rules mirror ctp2_code/gs/slic/Makefile.am, gs/newdb/Makefile.am,
# gs/dbgen/Makefile.am and ui/ldl (ctp2_code/Makefile.am): same prefixes, same
# output names.

GEN      ?= build/amiga-gen
CTP2     ?= ctp2_code
HOST_CXX ?= g++
HOST_CC  ?= gcc
LEX      ?= flex
# byacc, as configure.ac requires: bison -y rejects slic's grammars. Without a
# system byacc, extract the Debian package locally:
#   mkdir -p build/host-tools && cd build/host-tools && apt-get download byacc \
#     && dpkg-deb -x byacc_*.deb root
YACC     ?= $(firstword $(wildcard build/host-tools/root/usr/bin/byacc) $(shell command -v byacc))
YFLAGS   ?= -d

GEN_DBGEN := $(GEN)/dbgen
GEN_NEWDB := $(GEN)/newdb
GEN_SLIC  := $(GEN)/slic
GEN_LDL   := $(GEN)/ldl

CTPDB := $(GEN_DBGEN)/ctpdb

# The record types gs/newdb/Makefile.am generates, each from one .cdb file.
NEWDB_TYPES := \
	AdvanceBranchRecord AdvanceListRecord AdvanceRecord AgeCityStyleRecord \
	AgeRecord BuildListSequenceRecord BuildingBuildListRecord BuildingRecord \
	CitySizeRecord CityStyleRecord CivilisationRecord ConceptRecord \
	ConstRecord CultureRecord DifficultyRecord DiplomacyProposalRecord \
	DiplomacyRecord DiplomacyThreatRecord EndGameObjectRecord FeatRecord \
	GlobalWarmingRecord GoalRecord GovernmentRecord IconRecord \
	ImprovementListRecord LanguageRecord LeaderRecord MapIconRecord \
	MapRecord OrderRecord PersonalityRecord PollutionRecord PopRecord \
	ReligionRecord ResourceRecord RiskRecord SoundRecord \
	SpecialAttackInfoRecord SpecialEffectRecord SpriteRecord StrategyRecord \
	TerrainImprovementRecord TerrainRecord UnitBuildListRecord \
	UnitpromotionRecord UnitRecord WonderBuildListRecord WonderMovieRecord \
	WonderRecord

# The .cdb files gs/newdb/Makefile.am feeds to ctpdb (age.cdb is not one of them:
# AgeRecord comes from advance.cdb).
NEWDB_CDBS := $(addprefix $(CTP2)/gs/newdb/, \
	advance.cdb building.cdb citysize.cdb citystyle.cdb Civilisation.cdb \
	Concept.cdb Const.cdb culture.cdb Difficulty.cdb diplomacy.cdb \
	DiplomacyProposal.cdb DiplomacyThreat.cdb EndGameObject.cdb feat.cdb \
	GlobalWarming.cdb goal.cdb government.cdb icon.cdb Language.cdb Leader.cdb \
	Map.cdb order.cdb personality.cdb Pollution.cdb pop.cdb religion.cdb \
	Risk.cdb strategy.cdb terrain.cdb TerrainImprovement.cdb unit.cdb \
	unitpromotion.cdb wonder.cdb)

GEN_NEWDB_SRCS := $(addprefix $(GEN_NEWDB)/,$(addsuffix .cpp,$(NEWDB_TYPES))) $(GEN_NEWDB)/lex.dbl.c
GEN_SLIC_SRCS  := $(GEN_SLIC)/lex.sc.c $(GEN_SLIC)/lex.yy.c $(GEN_SLIC)/sc.tab.c $(GEN_SLIC)/y.tab.c
GEN_LDL_SRCS   := $(GEN_LDL)/lex.ldl.c $(GEN_LDL)/ldl.tab.c

GEN_SRCS := $(GEN_NEWDB_SRCS) $(GEN_SLIC_SRCS) $(GEN_LDL_SRCS)
GEN_INCS := -I$(GEN_NEWDB) -I$(GEN_SLIC) -I$(GEN_LDL)

GEN_STAMP := $(GEN)/.stamp

.PHONY: gen gen-clean
gen: $(GEN_STAMP)

# ---- ctpdb, the database code generator (host program) -----------------------

$(GEN_DBGEN)/y.tab.c $(GEN_DBGEN)/y.tab.h: $(CTP2)/gs/dbgen/ctpdb.y
	@mkdir -p $(GEN_DBGEN)
	cd $(GEN_DBGEN) && $(YACC) $(YFLAGS) -p yy -b y $(abspath $<)

$(GEN_DBGEN)/lex.yy.c: $(CTP2)/gs/dbgen/ctpdb.l $(GEN_DBGEN)/y.tab.h
	$(LEX) -i -Pyy -o$@ $<

CTPDB_SRCS := $(addprefix $(CTP2)/gs/dbgen/,Datum.cpp MemberClass.cpp RecordDescription.cpp ctpdb.cpp)

CTPDB_INCS := -DHAVE_CONFIG_H=1 -I$(CTP2)/os/amiga/host -I$(GEN_DBGEN) -I$(CTP2)/gs/dbgen \
	-I$(CTP2)/os/nowin32 -I$(CTP2)/os/include -I$(CTP2)/ctp/ctp2_utils

# The parser and lexer are C, as in the autotools build (C linkage for
# yyparse/yyrestart, which ctpdb.cpp declares extern "C").
$(GEN_DBGEN)/parser.o: $(GEN_DBGEN)/y.tab.c
	$(HOST_CC) -O1 -w $(CTPDB_INCS) -c $< -o $@

$(GEN_DBGEN)/lexer.o: $(GEN_DBGEN)/lex.yy.c
	$(HOST_CC) -O1 -w $(CTPDB_INCS) -c $< -o $@

$(CTPDB): $(CTPDB_SRCS) $(GEN_DBGEN)/parser.o $(GEN_DBGEN)/lexer.o
	$(HOST_CXX) -O1 -w -fpermissive $(CTPDB_INCS) $(CTPDB_SRCS) \
		$(GEN_DBGEN)/parser.o $(GEN_DBGEN)/lexer.o -o $@

# ---- gs/newdb: one ctpdb run per .cdb writes all of that file's records ------

$(GEN_NEWDB)/.records: $(NEWDB_CDBS) $(CTPDB)
	@mkdir -p $(GEN_NEWDB)
	@for f in $(NEWDB_CDBS); do \
		echo "  CTPDB $$f"; \
		(cd $(GEN_NEWDB) && $(abspath $(CTPDB)) . < $(abspath $$f)) || exit 1; \
	done
	@for t in $(NEWDB_TYPES); do \
		test -f $(GEN_NEWDB)/$$t.cpp -a -f $(GEN_NEWDB)/$$t.h || { echo "ctpdb did not write $$t"; exit 1; }; \
	done
	@touch $@

$(GEN_NEWDB)/lex.dbl.c: $(CTP2)/gs/newdb/DBLexer.l
	@mkdir -p $(GEN_NEWDB)
	$(LEX) -i -Pdbl -o$@ $<

# ---- gs/slic ---------------------------------------------------------------

$(GEN_SLIC)/sc.tab.c $(GEN_SLIC)/sc.tab.h: $(CTP2)/gs/slic/sliccmd.y
	@mkdir -p $(GEN_SLIC)
	cd $(GEN_SLIC) && $(YACC) $(YFLAGS) -p sc -b sc $(abspath $<)

$(GEN_SLIC)/y.tab.c $(GEN_SLIC)/y.tab.h: $(CTP2)/gs/slic/slic.y
	@mkdir -p $(GEN_SLIC)
	cd $(GEN_SLIC) && $(YACC) $(YFLAGS) -b y $(abspath $<)

$(GEN_SLIC)/lex.sc.c: $(CTP2)/gs/slic/sliccmd.l $(GEN_SLIC)/sc.tab.h
	$(LEX) -i -Psc -o$@ $<

$(GEN_SLIC)/lex.yy.c: $(CTP2)/gs/slic/slic.l $(GEN_SLIC)/y.tab.h
	$(LEX) -i -o$@ $<

# ---- ui/ldl ----------------------------------------------------------------

$(GEN_LDL)/ldl.tab.c $(GEN_LDL)/ldl.tab.h: $(CTP2)/ui/ldl/ldl.y
	@mkdir -p $(GEN_LDL)
	cd $(GEN_LDL) && $(YACC) $(YFLAGS) -b ldl $(abspath $<)

$(GEN_LDL)/lex.ldl.c: $(CTP2)/ui/ldl/ldl.l
	@mkdir -p $(GEN_LDL)
	$(LEX) -i -Pldl -o$@ $<

$(GEN_STAMP): $(GEN_NEWDB)/.records $(GEN_SRCS)
	@touch $@

# The record sources exist once the .records step ran.
$(addprefix $(GEN_NEWDB)/,$(addsuffix .cpp,$(NEWDB_TYPES)) $(addsuffix .h,$(NEWDB_TYPES))): $(GEN_NEWDB)/.records

gen-clean:
	rm -rf $(GEN)
