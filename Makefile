CC      ?= cc
CFLAGS  ?= -O0 -g -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
SANFLAGS := -fsanitize=undefined,alignment -fno-sanitize-recover=undefined
ASANFLAGS := -fsanitize=address -fno-omit-frame-pointer
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
GSUBGPOSSTUB_BIN := $(BUILDDIR)/test_gsub_gpos_stub
SHAPECONFIG_BIN  := $(BUILDDIR)/test_shape_config
EVICTION_BIN     := $(BUILDDIR)/test_config_eviction
CONFIGINDEX_BIN  := $(BUILDDIR)/test_config_index
VARIATIONAXES_BIN := $(BUILDDIR)/test_variation_axes
USERFEATURES_BIN := $(BUILDDIR)/test_user_features
SCRIPTRUNS_BIN   := $(BUILDDIR)/test_script_runs
HANGULINDEX_BIN  := $(BUILDDIR)/test_hangul_index
AVAR2_BIN        := $(BUILDDIR)/test_avar2_version

.PHONY: all test ubsan asan clean

all: $(NOTOSANS_BIN) $(ROBOTOFLEX_BIN) $(GLYPHCONFIG_BIN) $(BREAKPOS_BIN) $(MYANMAR_BIN) $(GSUBGPOSSTUB_BIN) $(SHAPECONFIG_BIN) $(EVICTION_BIN) $(CONFIGINDEX_BIN) $(VARIATIONAXES_BIN) $(USERFEATURES_BIN) $(SCRIPTRUNS_BIN) $(HANGULINDEX_BIN) $(AVAR2_BIN)

test: all
	$(BREAKPOS_BIN)
	$(GSUBGPOSSTUB_BIN)
	$(SHAPECONFIG_BIN)
	$(EVICTION_BIN)
	$(VARIATIONAXES_BIN)
	$(MYANMAR_BIN)     "$(NOTOSANS_FONT)"
	$(SCRIPTRUNS_BIN)  "$(NOTOSANS_FONT)"
	$(HANGULINDEX_BIN) "$(NOTOSANS_FONT)"
	$(CONFIGINDEX_BIN) "$(NOTOSANS_FONT)"
	$(AVAR2_BIN)       "$(NOTOSANS_FONT)"
	$(USERFEATURES_BIN) "$(NOTOSANS_FONT)"
	$(GLYPHCONFIG_BIN) "$(NOTOSANS_FONT)"
	$(NOTOSANS_BIN)    "$(NOTOSANS_FONT)"
	$(ROBOTOFLEX_BIN)  "$(ROBOTOFLEX_FONT)"

# Rebuild and run every test under -fsanitize=undefined,alignment. OpenType only
# 2-byte-aligns many table fields (ItemVariationStore data/region offsets,
# ConditionSet condition offsets), so this catches any multi-byte font read that
# bypasses the kbts__ReadU16/U32Unaligned helpers. Runtime UB aborts the run so
# a diagnostic cannot pass silently.
ubsan:
	$(MAKE) clean
	$(MAKE) test CFLAGS='$(CFLAGS) $(SANFLAGS)'

# Rebuild and run every test under -fsanitize=address. Font tables are
# byteswapped in place inside the blob allocation, so an offset or count that
# is followed without a bounds check writes out of that allocation.
asan:
	$(MAKE) clean
	$(MAKE) test CFLAGS='$(CFLAGS) $(ASANFLAGS)'

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

$(GSUBGPOSSTUB_BIN): $(TESTDIR)/test_gsub_gpos_stub.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(SHAPECONFIG_BIN): $(TESTDIR)/test_shape_config.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(EVICTION_BIN): $(TESTDIR)/test_config_eviction.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(CONFIGINDEX_BIN): $(TESTDIR)/test_config_index.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(VARIATIONAXES_BIN): $(TESTDIR)/test_variation_axes.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(USERFEATURES_BIN): $(TESTDIR)/test_user_features.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(SCRIPTRUNS_BIN): $(TESTDIR)/test_script_runs.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(HANGULINDEX_BIN): $(TESTDIR)/test_hangul_index.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

$(AVAR2_BIN): $(TESTDIR)/test_avar2_version.c kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(BUILDDIR)
