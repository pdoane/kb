// Regression test for the glyph-config stale-pointer cache bug.
//
// kbts__FindOrCreateGlyphConfig caches one kbts_glyph_config per distinct set
// of feature overrides. The override array a caller hands in lives in the
// shape context's ScratchArena, which is reset on every kbts_ShapeBegin, so the
// same address is handed back on a later shaping run with *different* contents.
//
// The buggy cache keyed each entry on the override array's *address* plus its
// count. When a later run's overrides land at the address of an earlier,
// still-cached entry, the (pointer, count) key collides and the caller is
// handed the earlier run's glyph config -- silently applying the wrong
// features.
//
// Upstream 2.19 added the shape config to the cache key, which is not enough:
// the override array's contents can still differ at the same address.
//
// Two tests. The first is white-box: it drives kbts__FindOrCreateGlyphConfig
// directly with a single stack buffer whose contents are mutated in place
// between calls -- precisely the "same address, different contents" condition
// the cache must distinguish. The second reproduces the aliasing through the
// public API alone: the ScratchArena alternates between two blocks across
// shaping runs on one context, so run 3's hoisted override lands at the same
// address as run 1's, and with a pointer key run 3 silently reuses run 1's
// glyph config.
#include <stdio.h>
#include <stdlib.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

#define MAX_SHAPED_GLYPHS 16

typedef struct shaped
{
  int Count;
  kbts_u32 Ids[MAX_SHAPED_GLYPHS];
} shaped;

static void DrainRuns(kbts_shape_context *Context, shaped *Out)
{
  Out->Count = 0;
  kbts_run Run;
  while(kbts_ShapeRun(Context, &Run))
  {
    kbts_glyph *Glyph;
    while(kbts_GlyphIteratorNext(&Run.Glyphs, &Glyph))
    {
      if(Out->Count < MAX_SHAPED_GLYPHS) Out->Ids[Out->Count] = Glyph->Id;
      Out->Count++;
    }
  }
}

static int SameShape(shaped *A, shaped *B)
{
  if(A->Count != B->Count) return 0;
  for(int Index = 0; Index < A->Count; ++Index)
  {
    if(A->Ids[Index] != B->Ids[Index]) return 0;
  }
  return 1;
}

// Shape Text in a fresh context with a single liga override.
static void ShapeFresh(kbts_font *Font, const char *Text, int Length, int LigaValue, shaped *Out)
{
  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  kbts_ShapePushFont(Context, Font);
  kbts_ShapePushFeature(Context, KBTS_FEATURE_TAG_liga, LigaValue);
  kbts_ShapeBegin(Context, KBTS_DIRECTION_LTR, KBTS_LANGUAGE_DONT_KNOW);
  kbts_ShapeUtf8(Context, Text, Length, KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
  kbts_ShapeEnd(Context);
  DrainRuns(Context, Out);
  kbts_DestroyShapeContext(Context);
}

int main(int argc, char **argv)
{
  if(argc < 2) { fprintf(stderr, "usage: %s <font.ttf>\n", argv[0]); return 2; }

  void *FileData = 0; int FileSize = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &FileData, &FileSize);
  CHECK(kbts_FontIsValid(&Font), "kbts_FontFromFile failed (error=%u)", Font.Error);
  if(!kbts_FontIsValid(&Font)) return Failures ? Failures : 1;

  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  kbts_shape_config *Config = kbts_CreateShapeConfig(&Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW, 0, 0);
  CHECK(Config != 0, "kbts_CreateShapeConfig failed");

  // One stack buffer, reused (mutated in place) across both lookups: this is
  // the address-aliasing the cache must defend against.
  kbts_feature_override Overrides[1];
  Overrides[0].Tag = KBTS_FEATURE_TAG_liga;

  Overrides[0].Value = 0;
  kbts_glyph_config *ConfigLigaOff = kbts__FindOrCreateGlyphConfig(Context, Config, Overrides, 1);

  Overrides[0].Value = 1;  // same address, different contents
  kbts_glyph_config *ConfigLigaOn = kbts__FindOrCreateGlyphConfig(Context, Config, Overrides, 1);

  printf("liga=0 config=%p\nliga=1 config=%p\n", (void *)ConfigLigaOff, (void *)ConfigLigaOn);

  CHECK(ConfigLigaOff != 0 && ConfigLigaOn != 0, "glyph config creation failed");

  // The two override sets differ, so they must resolve to different glyph
  // configs. The buggy cache matches on the (now-stale) pointer and returns the
  // liga=0 config for the liga=1 request.
  CHECK(ConfigLigaOff != ConfigLigaOn,
        "stale-pointer cache returned the same glyph config for liga=0 and "
        "liga=1 (the second request reused the first's config)");

  kbts_DestroyShapeConfig(Config);
  kbts_DestroyShapeContext(Context);

  { // Public-API repro: three shaping runs on one context, overriding
    // liga=1, liga=0, liga=0. Run 3's hoisted override aliases run 1's
    // address, so a pointer-keyed cache hands run 3 the liga=1 config and
    // "fi" ligates despite liga=0.
    shaped Liga0, Liga1, Runs[3];
    int LigaValues[3] = {1, 0, 0};

    ShapeFresh(&Font, "fi", 2, 0, &Liga0);
    ShapeFresh(&Font, "fi", 2, 1, &Liga1);
    CHECK(!SameShape(&Liga0, &Liga1), "font did not react to liga on \"fi\"; repro inconclusive");

    kbts_shape_context *ShapeContext = kbts_CreateShapeContext(0, 0);
    kbts_ShapePushFont(ShapeContext, &Font);
    KBTS__FOR(RunIndex, 0, 3)
    {
      if(RunIndex) kbts_ShapePopFeature(ShapeContext, KBTS_FEATURE_TAG_liga);
      kbts_ShapePushFeature(ShapeContext, KBTS_FEATURE_TAG_liga, LigaValues[RunIndex]);
      kbts_ShapeBegin(ShapeContext, KBTS_DIRECTION_LTR, KBTS_LANGUAGE_DONT_KNOW);
      kbts_ShapeUtf8(ShapeContext, "fi", 2, KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
      kbts_ShapeEnd(ShapeContext);
      DrainRuns(ShapeContext, &Runs[RunIndex]);

      shaped *Expected = LigaValues[RunIndex] ? &Liga1 : &Liga0;
      CHECK(SameShape(&Runs[RunIndex], Expected),
            "run %u ignored liga=%d and reused a stale glyph config",
            (kbts_u32)RunIndex + 1, LigaValues[RunIndex]);
    }
    kbts_DestroyShapeContext(ShapeContext);
  }

  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("OK\n");
  return 0;
}
