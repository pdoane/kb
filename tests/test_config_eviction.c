// Shape config cache: what a context keeps of a font that is gone.
//
// The context keys its cached shape configs by font address (hashed with the
// font's variation vector). A font loaded at the address of one the context
// has shaped with matches that font's configs, which read the tables of the
// font that is gone. Two ways to land there:
//
//   - A caller-owned kbts_font is freed and reloaded at the same address. The
//     caller says so with kbts_ShapeForgetFont before freeing it.
//   - A font the context allocated itself (kbts_ShapePushFontFromMemory) is
//     freed when popped, and the next such push lands at the same arena
//     address. The pop forgets that font's configs.
//
// The fonts are synthetic GSUBs differing in how many features they list, so a
// stale config is told apart by its lookup count. They have no variation axes,
// so the cache key is the font address alone.
//
// The Makefile builds with -DTEST_WITHOUT_FORGET_FONT against a header that
// has no kbts_ShapeForgetFont. The caller-owned half then runs with the call
// removed and fails on the freed font's config. The context-owned half needs a
// working kbts_ShapePopFont, so it is left out there.
#include <stdio.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"
#include "synthetic_font.h"

#ifdef TEST_WITHOUT_FORGET_FONT
#define kbts_ShapeForgetFont(Context, Font) ((void)0)
#endif

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

#define FIRST_FEATURE_COUNT 2
#define SECOND_FEATURE_COUNT 4

// The lookup count of the config the context answers for its top font.
static int TopFontLookupCount(kbts_shape_context *Context)
{
  kbts_shape_config *Config = kbts__FindOrCreateShapeConfig(Context, SyntheticTopFontInfo(Context), KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW);
  CHECK(Config != 0, "kbts__FindOrCreateShapeConfig failed");
  return Config ? (int)kbts__SequentialLookupCount(Config) : -1;
}

// A fresh copy of a synthetic font listing [FeatureCount] features. Loading
// a font byteswaps its data in place, so every load takes its own copy.
static unsigned char *BuildFont(int FeatureCount, unsigned int *SizeOut)
{
  SyntheticBuildLatinGsub(FeatureCount);
  return SyntheticBuildFont(SizeOut);
}

int main(void)
{

  // A caller-owned font, freed and reloaded at the same address while another
  // font's config stays cached. Neither font is popped.
  {
    static kbts_font Font;
    static kbts_font Other;
    kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
    unsigned int FirstSize = 0, SecondSize = 0, OtherSize = 0;
    unsigned char *FirstData = BuildFont(FIRST_FEATURE_COUNT, &FirstSize);
    unsigned char *SecondData = BuildFont(SECOND_FEATURE_COUNT, &SecondSize);
    unsigned char *OtherData = BuildFont(FIRST_FEATURE_COUNT, &OtherSize);

    void *OtherBlob = 0;
    CHECK(SyntheticLoadFont(&Other, OtherData, OtherSize, &OtherBlob) == KBTS_LOAD_FONT_ERROR_NONE, "the other font failed to load");
    kbts_ShapePushFont(Context, &Other);
    CHECK(TopFontLookupCount(Context) == FIRST_FEATURE_COUNT, "the other font's config has the wrong lookup count");

    void *Blob = 0;
    CHECK(SyntheticLoadFont(&Font, FirstData, FirstSize, &Blob) == KBTS_LOAD_FONT_ERROR_NONE, "the first font failed to load");
    kbts_ShapePushFont(Context, &Font);
    CHECK(TopFontLookupCount(Context) == FIRST_FEATURE_COUNT, "the first font's config has the wrong lookup count");

    // Forgetting one font keeps the configs of the others.
    kbts_ShapeForgetFont(Context, &Font);
    CHECK(SyntheticCachedShapeConfigCount(Context) == 1, "forgetting one font left %u configs cached, expected the other font's 1",
          (unsigned int)SyntheticCachedShapeConfigCount(Context));
    kbts_FreeFont(&Font);
    free(Blob);

    CHECK(SyntheticLoadFont(&Font, SecondData, SecondSize, &Blob) == KBTS_LOAD_FONT_ERROR_NONE, "the second font failed to load");
    int Count = TopFontLookupCount(Context);
    CHECK(Count == SECOND_FEATURE_COUNT,
          "the reloaded font's config holds %d lookups, expected %d: it is the freed font's config",
          Count, SECOND_FEATURE_COUNT);

    kbts_DestroyShapeContext(Context);
    kbts_FreeFont(&Other);
    free(OtherBlob);
    kbts_FreeFont(&Font);
    free(Blob);
    free(FirstData);
    free(SecondData);
    free(OtherData);
  }

#ifndef TEST_WITHOUT_FORGET_FONT

  // A context-owned font, popped and replaced by another push from memory.
  {
    kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
    unsigned int FirstSize = 0, SecondSize = 0;
    unsigned char *FirstData = BuildFont(FIRST_FEATURE_COUNT, &FirstSize);
    unsigned char *SecondData = BuildFont(SECOND_FEATURE_COUNT, &SecondSize);

    kbts_font *First = kbts_ShapePushFontFromMemory(Context, FirstData, (int)FirstSize, 0);
    CHECK(First != 0, "pushing the first font from memory failed");
    CHECK(TopFontLookupCount(Context) == FIRST_FEATURE_COUNT, "the first font's config has the wrong lookup count");
    kbts_ShapePopFont(Context);
    CHECK(SyntheticCachedShapeConfigCount(Context) == 0, "popping a context-owned font left %u configs cached",
          (unsigned int)SyntheticCachedShapeConfigCount(Context));

    kbts_font *Second = kbts_ShapePushFontFromMemory(Context, SecondData, (int)SecondSize, 0);
    CHECK(Second != 0, "pushing the second font from memory failed");
    printf("context-owned font address %s across the pop\n", (Second == First) ? "reused" : "not reused");
    int Count = TopFontLookupCount(Context);
    CHECK(Count == SECOND_FEATURE_COUNT,
          "the second font's config holds %d lookups, expected %d: it is the popped font's config",
          Count, SECOND_FEATURE_COUNT);
    kbts_ShapePopFont(Context);

    kbts_DestroyShapeContext(Context);
    free(FirstData);
    free(SecondData);
  }
#endif

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("test_config_eviction: OK\n");
  return 0;
}
