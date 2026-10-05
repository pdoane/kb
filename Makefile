CC      ?= cc
CFLAGS  ?= -O0 -g -Wall -Wextra -Wno-unused-function -Wno-unused-variable -Wno-unused-parameter
SANFLAGS := -fsanitize=undefined,alignment -fno-sanitize-recover=undefined
ASANFLAGS := -fsanitize=address -fno-omit-frame-pointer
TESTDIR := tests
BUILDDIR := build

# The test fonts are not bundled. Drop them at the repo root with the names
# below (or override NOTOSANS_FONT / ROBOTOFLEX_FONT with another path):
#   - NotoSans[wdth,wght].ttf  -- https://fonts.google.com/noto/specimen/Noto+Sans
#   - RobotoFlex-VariableFont_GRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght.ttf
#                              -- https://fonts.google.com/specimen/Roboto+Flex
NOTOSANS_FONT   ?= NotoSans[wdth,wght].ttf
ROBOTOFLEX_FONT ?= RobotoFlex-VariableFont_GRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght.ttf

# Tests by the fonts they take. See tests/README.md for what each one checks.
NO_FONT_TESTS := feature_stage_limit config_eviction gsub_gpos_bounds dflt_fallback
NOTO_TESTS    := script_tag_runs user_feature_filter feature_stack_order pop_font \
                 delta_set_index_map_alignment
TWO_FONT_TESTS := config_cache_hash

TESTS := $(NO_FONT_TESTS) $(NOTO_TESTS) $(TWO_FONT_TESTS)
BINS  := $(addprefix $(BUILDDIR)/test_,$(TESTS))

.PHONY: all test ubsan asan clean

all: $(BINS)

# Runs every test, reports each failure, and fails if any test did.
test: all
	@failed=""; \
	for t in $(NO_FONT_TESTS); do $(BUILDDIR)/test_$$t || failed="$$failed $$t"; done; \
	for t in $(NOTO_TESTS); do $(BUILDDIR)/test_$$t "$(NOTOSANS_FONT)" || failed="$$failed $$t"; done; \
	for t in $(TWO_FONT_TESTS); do $(BUILDDIR)/test_$$t "$(NOTOSANS_FONT)" "$(ROBOTOFLEX_FONT)" || failed="$$failed $$t"; done; \
	if [ -n "$$failed" ]; then echo "FAILED:$$failed"; exit 1; fi; \
	echo "all $(words $(TESTS)) tests passed"

# Rebuild and run every test under -fsanitize=undefined,alignment. OpenType only
# 2-byte-aligns many table fields, so this catches a multi-byte font read that
# bypasses the kbts__ReadU16/U32Unaligned helpers. Runtime UB aborts the test
# so a diagnostic cannot pass silently.
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

# Flags a test always builds with, on top of CFLAGS.
TESTFLAGS :=

# The misaligned read is undefined behavior that runs fine on common hardware,
# so this test always checks alignment.
$(BUILDDIR)/test_delta_set_index_map_alignment: TESTFLAGS := -fsanitize=alignment -fno-sanitize-recover=alignment

# A header without kbts_ShapeForgetFont runs the half of the test that can fail
# on its absence.
$(BUILDDIR)/test_config_eviction: TESTFLAGS := $(shell grep -q kbts_ShapeForgetFont kb_text_shape.h || echo -DTEST_WITHOUT_FORGET_FONT)

$(BUILDDIR)/test_%: $(TESTDIR)/test_%.c $(wildcard $(TESTDIR)/*.h) kb_text_shape.h | $(BUILDDIR)
	$(CC) $(CFLAGS) $(TESTFLAGS) -o $@ $<

clean:
	rm -rf $(BUILDDIR)
