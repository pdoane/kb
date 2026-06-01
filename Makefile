CC      ?= cc
CFLAGS  ?= -O0 -g -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
SANFLAGS := -fsanitize=undefined,alignment
TESTDIR := tests
BUILDDIR := build

# The test fonts are not bundled. Drop them at the repo root with the names
# below (or override NOTOSANS_FONT / ROBOTOFLEX_FONT to a different path):
#   - NotoSans[wdth,wght].ttf  -- https://fonts.google.com/noto/specimen/Noto+Sans
#   - RobotoFlex-VariableFont_GRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght.ttf
#                              -- https://fonts.google.com/specimen/Roboto+Flex
# The glyph-config test is font-agnostic and reuses NOTOSANS_FONT.
NOTOSANS_FONT   ?= NotoSans[wdth,wght].ttf
ROBOTOFLEX_FONT ?= RobotoFlex-VariableFont_GRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght.ttf

NOTOSANS_BIN     := $(BUILDDIR)/test_noto_sans
ROBOTOFLEX_BIN   := $(BUILDDIR)/test_roboto_flex
GLYPHCONFIG_BIN  := $(BUILDDIR)/test_glyph_config
BREAKPOS_BIN     := $(BUILDDIR)/test_break_positions
MYANMAR_BIN      := $(BUILDDIR)/test_myanmar_cluster

.PHONY: all test ubsan clean

all: $(NOTOSANS_BIN) $(ROBOTOFLEX_BIN) $(GLYPHCONFIG_BIN) $(BREAKPOS_BIN) $(MYANMAR_BIN)

test: all
	$(BREAKPOS_BIN)
	$(MYANMAR_BIN)     "$(NOTOSANS_FONT)"
	$(GLYPHCONFIG_BIN) "$(NOTOSANS_FONT)"
	$(NOTOSANS_BIN)    "$(NOTOSANS_FONT)"
	$(ROBOTOFLEX_BIN)  "$(ROBOTOFLEX_FONT)"

# Rebuild and run every test under -fsanitize=undefined,alignment. OpenType only
# 2-byte-aligns many table fields (ItemVariationStore data/region offsets,
# ConditionSet condition offsets), so this catches any multi-byte font read that
# bypasses the kbts__ReadU16/U32Unaligned helpers.
ubsan:
	$(MAKE) clean
	$(MAKE) test CFLAGS='$(CFLAGS) $(SANFLAGS)'

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(NOTOSANS_BIN): $(TESTDIR)/test_noto_sans.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(ROBOTOFLEX_BIN): $(TESTDIR)/test_roboto_flex.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(GLYPHCONFIG_BIN): $(TESTDIR)/test_glyph_config.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BREAKPOS_BIN): $(TESTDIR)/test_break_positions.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(MYANMAR_BIN): $(TESTDIR)/test_myanmar_cluster.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BUILDDIR)
