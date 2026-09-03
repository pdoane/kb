// Shape-config cache tests: what a shape context keeps after a font is popped.
//
// The context caches shape configs by font pointer and glyph configs by shape
// config pointer, and both live in the context's config arena. Popping a font
// used to leave its entries in place, so a font parsed into the address of a
// popped one -- the same kbts_font, reloaded, or a fresh allocation the
// allocator handed back the same block -- was answered with configs built from
// the tables of the font that is gone.
//
// The fonts here are synthetic GSUBs, so the two loads differ in a way the test
// can name: they list a different number of features.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

#define MAX_FEATURE_COUNT 4
static const char *FeatureTags[MAX_FEATURE_COUNT] = { "liga", "smcp", "swsh", "zero" };

static unsigned char Gsub[1 << 12];
static unsigned int GsubSize;

static void WriteU16(unsigned int At, unsigned int Value)
{
  Gsub[At + 0] = (unsigned char)(Value >> 8);
  Gsub[At + 1] = (unsigned char)(Value >> 0);
}

static unsigned int Push(unsigned int Size)
{
  unsigned int At = GsubSize;
  GsubSize += Size;
  return At;
}

// A GSUB with one latn script, one default language system listing the first
// [FeatureCount] features, and one single-substitution lookup per feature.
static void BuildGsub(int FeatureCount)
{
  GsubSize = 0;

  unsigned int Header = Push(10);
  WriteU16(Header + 0, 1);
  WriteU16(Header + 2, 0);

  unsigned int ScriptList = Push(2 + 6);
  WriteU16(Header + 4, ScriptList - Header);
  WriteU16(ScriptList, 1);
  memcpy(Gsub + ScriptList + 2, "latn", 4);

  unsigned int Script = Push(4);
  WriteU16(ScriptList + 2 + 4, Script - ScriptList);
  WriteU16(Script + 2, 0);

  unsigned int LangSys = Push(6 + 2 * (unsigned int)FeatureCount);
  WriteU16(Script + 0, LangSys - Script);
  WriteU16(LangSys + 0, 0);
  WriteU16(LangSys + 2, 0xFFFF);
  WriteU16(LangSys + 4, (unsigned int)FeatureCount);
  for(int Index = 0; Index < FeatureCount; ++Index)
  {
    WriteU16(LangSys + 6 + 2 * (unsigned int)Index, (unsigned int)Index);
  }

  unsigned int FeatureList = Push(2 + 6 * (unsigned int)FeatureCount);
  WriteU16(Header + 6, FeatureList - Header);
  WriteU16(FeatureList, (unsigned int)FeatureCount);
  for(int FeatureIndex = 0; FeatureIndex < FeatureCount; ++FeatureIndex)
  {
    unsigned int Record = FeatureList + 2 + 6 * (unsigned int)FeatureIndex;
    memcpy(Gsub + Record, FeatureTags[FeatureIndex], 4);

    unsigned int Feature = Push(6);
    WriteU16(Record + 4, Feature - FeatureList);
    WriteU16(Feature + 0, 0);
    WriteU16(Feature + 2, 1);
    WriteU16(Feature + 4, (unsigned int)FeatureIndex);
  }

  unsigned int LookupList = Push(2 + 2 * (unsigned int)FeatureCount);
  WriteU16(Header + 8, LookupList - Header);
  WriteU16(LookupList, (unsigned int)FeatureCount);
  for(int LookupIndex = 0; LookupIndex < FeatureCount; ++LookupIndex)
  {
    unsigned int Lookup = Push(8);
    WriteU16(LookupList + 2 + 2 * (unsigned int)LookupIndex, Lookup - LookupList);
    WriteU16(Lookup + 0, 1);
    WriteU16(Lookup + 2, 0);
    WriteU16(Lookup + 4, 1);

    unsigned int Subtable = Push(6);
    WriteU16(Lookup + 6, Subtable - Lookup);
    WriteU16(Subtable + 0, 1);
    WriteU16(Subtable + 4, 1);

    unsigned int Coverage = Push(6);
    WriteU16(Subtable + 2, Coverage - Subtable);
    WriteU16(Coverage + 0, 1);
    WriteU16(Coverage + 2, 1);
    WriteU16(Coverage + 4, 1);
  }
}

// maxp 0.5, so the blob has a glyph count to size its lookup matrices with.
static const unsigned char Maxp[] = { 0x00, 0x00, 0x50, 0x00, 0x00, 0x08 };

static void WriteU32(unsigned char *At, unsigned int Value)
{
  At[0] = (unsigned char)(Value >> 24);
  At[1] = (unsigned char)(Value >> 16);
  At[2] = (unsigned char)(Value >> 8);
  At[3] = (unsigned char)(Value >> 0);
}

static unsigned char *BuildFont(unsigned int *SizeOut)
{
  unsigned int TableCount = 2;
  unsigned int DirectorySize = 12 + 16 * TableCount;
  unsigned int GsubAt = (DirectorySize + 3) & ~3u;
  unsigned int MaxpAt = (GsubAt + GsubSize + 3) & ~3u;
  unsigned int Size = MaxpAt + (unsigned int)sizeof(Maxp);

  unsigned char *Font = (unsigned char *)calloc(1, Size);
  WriteU32(Font + 0, 0x00010000);
  Font[4] = 0;
  Font[5] = (unsigned char)TableCount;

  memcpy(Font + 12, "GSUB", 4);
  WriteU32(Font + 12 + 8, GsubAt);
  WriteU32(Font + 12 + 12, GsubSize);
  memcpy(Font + GsubAt, Gsub, GsubSize);

  memcpy(Font + 28, "maxp", 4);
  WriteU32(Font + 28 + 8, MaxpAt);
  WriteU32(Font + 28 + 12, (unsigned int)sizeof(Maxp));
  memcpy(Font + MaxpAt, Maxp, sizeof(Maxp));

  *SizeOut = Size;
  return Font;
}

static void LoadFont(kbts_font *Font, unsigned char *FontData, unsigned int FontSize, void **BlobOut)
{
  kbts_load_font_state State = KBTS__ZERO;
  int ScratchSize = 0;
  int OutputSize = 0;

  *BlobOut = 0;
  kbts_load_font_error Error = kbts_LoadFont(Font, &State, FontData, (int)FontSize, 0, &ScratchSize, &OutputSize);
  if(Error == KBTS_LOAD_FONT_ERROR_NEED_TO_CREATE_BLOB)
  {
    void *Scratch = malloc((size_t)ScratchSize);
    void *Blob = malloc((size_t)OutputSize);
    Error = kbts_PlaceBlob(Font, &State, Scratch, Blob);
    free(Scratch);
    *BlobOut = Blob;
  }
  CHECK(Error == KBTS_LOAD_FONT_ERROR_NONE, "kbts_LoadFont returned %u", Error);
}

static kbts_un CachedShapeConfigCount(kbts_shape_context *Context)
{
  kbts_un Result = 0;
  for(kbts__existing_shape_config_block *Block = (kbts__existing_shape_config_block *)Context->ExistingShapeConfigBlockSentinel.Next;
      kbts__ExistingShapeConfigBlockIsValid(Context, Block);
      Block = (kbts__existing_shape_config_block *)Block->Header.Next)
  {
    Result += Block->Count;
  }
  return Result;
}

static kbts_un CachedGlyphConfigCount(kbts_shape_context *Context)
{
  kbts_un Result = 0;
  for(kbts__existing_glyph_config_block *Block = (kbts__existing_glyph_config_block *)Context->ExistingGlyphConfigBlockSentinel.Next;
      kbts__ExistingGlyphConfigBlockIsValid(Context, Block);
      Block = (kbts__existing_glyph_config_block *)Block->Header.Next)
  {
    Result += Block->Count;
  }
  return Result;
}

// Nonzero if [GlyphConfig] enables any of [Config]'s lookups.
static int GlyphConfigEnablesLookup(kbts_shape_config *Config, kbts_glyph_config *GlyphConfig)
{
  if(!Config || !GlyphConfig) return 0;

  kbts_un SequentialLookupCount = kbts__SequentialLookupCount(Config);
  kbts_un LastIndex = SequentialLookupCount ? (SequentialLookupCount - 1) : 0;
  kbts__matrix_index LastMatrixIndex = kbts__IdSequentialLookupMatrixIndex(LastIndex, 0, SequentialLookupCount);

  kbts_u32 Enabled = 0;
  KBTS__FOR(WordIndex, 0, LastMatrixIndex.WordIndex + 1)
  {
    Enabled |= GlyphConfig->EnabledLookupBits[WordIndex];
  }
  return Enabled != 0;
}

// An allocator that reports how many bytes the context holds.
typedef struct allocation_stats
{
  size_t LiveBytes;
} allocation_stats;

#define ALLOCATION_HEADER 16

static void CountingAllocator(void *Data, kbts_allocator_op *Op)
{
  allocation_stats *Stats = (allocation_stats *)Data;

  switch(Op->Kind)
  {
  case KBTS_ALLOCATOR_OP_KIND_ALLOCATE:
  {
    size_t Size = (size_t)Op->Allocate.Size;
    char *Base = (char *)malloc(Size + ALLOCATION_HEADER);
    if(Base)
    {
      memcpy(Base, &Size, sizeof(Size));
      Stats->LiveBytes += Size;
      Op->Allocate.Pointer = Base + ALLOCATION_HEADER;
    }
  } break;

  case KBTS_ALLOCATOR_OP_KIND_FREE:
  {
    char *Base = (char *)Op->Free.Pointer - ALLOCATION_HEADER;
    size_t Size = 0;
    memcpy(&Size, Base, sizeof(Size));
    Stats->LiveBytes -= Size;
    free(Base);
  } break;
  }
}

int main(void)
{
  // One kbts_font, loaded twice from different tables: the second load is a
  // font at the address of a font the context has seen. The override is on a
  // feature only the second load lists, so a glyph config held over from the
  // first enables no lookup where the second font's enables one.
  static kbts_font Font;
  kbts_feature_override Override;
  Override.Tag = KBTS_FOURCC('s','w','s','h');
  Override.Value = 1;

  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  CHECK(Context != 0, "kbts_CreateShapeContext failed");
  if(!Context) return 1;

  unsigned int FirstSize = 0;
  BuildGsub(2);
  unsigned char *FirstData = BuildFont(&FirstSize);
  void *FirstBlob = 0;
  LoadFont(&Font, FirstData, FirstSize, &FirstBlob);

  kbts_shape_config *FirstConfig = 0;
  kbts_glyph_config *FirstGlyphConfig = 0;
  if(kbts_FontIsValid(&Font))
  {
    kbts_ShapePushFont(Context, &Font);
    FirstConfig = kbts__FindOrCreateShapeConfig(Context, &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW);
    CHECK(FirstConfig != 0, "kbts__FindOrCreateShapeConfig failed");
    FirstGlyphConfig = kbts__FindOrCreateGlyphConfig(Context, FirstConfig, &Override, 1);
    CHECK(FirstGlyphConfig != 0, "kbts__FindOrCreateGlyphConfig failed");
    CHECK(!GlyphConfigEnablesLookup(FirstConfig, FirstGlyphConfig),
          "the first font does not list swsh, so its glyph config should enable no lookup");
    CHECK(CachedShapeConfigCount(Context) == 1, "expected one cached shape config, got %u",
          (unsigned int)CachedShapeConfigCount(Context));
    CHECK(CachedGlyphConfigCount(Context) == 1, "expected one cached glyph config, got %u",
          (unsigned int)CachedGlyphConfigCount(Context));

    kbts_ShapePopFont(Context);
  }

  CHECK(CachedShapeConfigCount(Context) == 0,
        "the popped font left %u shape configs cached",
        (unsigned int)CachedShapeConfigCount(Context));
  CHECK(CachedGlyphConfigCount(Context) == 0,
        "the popped font left %u glyph configs cached, so a shape config allocated at the "
        "address of one of the popped font's answers with the popped font's glyph config",
        (unsigned int)CachedGlyphConfigCount(Context));

  kbts_FreeFont(&Font);
  free(FirstBlob);
  free(FirstData);

  unsigned int SecondSize = 0;
  BuildGsub(MAX_FEATURE_COUNT);
  unsigned char *SecondData = BuildFont(&SecondSize);
  void *SecondBlob = 0;
  LoadFont(&Font, SecondData, SecondSize, &SecondBlob);

  if(kbts_FontIsValid(&Font))
  {
    kbts_ShapePushFont(Context, &Font);
    kbts_shape_config *SecondConfig = kbts__FindOrCreateShapeConfig(Context, &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW);
    CHECK(SecondConfig != 0, "kbts__FindOrCreateShapeConfig failed");
    printf("shape config address %s across the reload\n",
           (SecondConfig == FirstConfig) ? "reused" : "not reused");
    if(SecondConfig)
    {
      CHECK(SecondConfig->Font->Blob == Font.Blob,
            "the shape config reads a blob the font no longer holds");
      CHECK(kbts__SequentialLookupCount(SecondConfig) == MAX_FEATURE_COUNT,
            "the shape config holds %u lookups, expected the second font's %d: it is the "
            "first font's config",
            (unsigned int)kbts__SequentialLookupCount(SecondConfig), MAX_FEATURE_COUNT);

      kbts_glyph_config *SecondGlyphConfig = kbts__FindOrCreateGlyphConfig(Context, SecondConfig, &Override, 1);
      CHECK(SecondGlyphConfig != 0, "kbts__FindOrCreateGlyphConfig failed");
      printf("glyph config address %s across the reload\n",
             (SecondGlyphConfig == FirstGlyphConfig) ? "reused" : "not reused");
      CHECK(GlyphConfigEnablesLookup(SecondConfig, SecondGlyphConfig),
            "the override on swsh, which the second font lists, enabled no lookup: the "
            "glyph config is one the first font's shape config was keyed to");
    }
    kbts_ShapePopFont(Context);
  }

  // A font pushed twice keeps its configs until the last push is popped.
  if(kbts_FontIsValid(&Font))
  {
    kbts_ShapePushFont(Context, &Font);
    kbts_ShapePushFont(Context, &Font);
    kbts__FindOrCreateShapeConfig(Context, &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW);
    kbts_ShapePopFont(Context);
    CHECK(CachedShapeConfigCount(Context) == 1,
          "popping one of two pushes of a font dropped its configs");
    kbts_ShapePopFont(Context);
    CHECK(CachedShapeConfigCount(Context) == 0,
          "popping the last push of a font kept %u configs",
          (unsigned int)CachedShapeConfigCount(Context));
  }

  kbts_DestroyShapeContext(Context);

  // Push and pop churn: a config the context builds is freed with the font it
  // was built for, so the live bytes after many rounds are the live bytes after
  // the first one.
  if(kbts_FontIsValid(&Font))
  {
    allocation_stats Stats;
    Stats.LiveBytes = 0;
    kbts_shape_context *Churn = kbts_CreateShapeContext(CountingAllocator, &Stats);
    CHECK(Churn != 0, "kbts_CreateShapeContext failed");

    size_t AfterFirstRound = 0;
    for(int Round = 0; Round < 64; ++Round)
    {
      kbts_ShapePushFont(Churn, &Font);
      kbts_shape_config *Config = kbts__FindOrCreateShapeConfig(Churn, &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW);
      kbts__FindOrCreateGlyphConfig(Churn, Config, &Override, 1);
      kbts_ShapePopFont(Churn);

      if(Round == 0) AfterFirstRound = Stats.LiveBytes;
    }

    printf("live bytes after 1 round %u, after 64 rounds %u\n",
           (unsigned int)AfterFirstRound, (unsigned int)Stats.LiveBytes);
    CHECK(Stats.LiveBytes <= AfterFirstRound,
          "64 rounds of push and pop grew the context from %u to %u live bytes",
          (unsigned int)AfterFirstRound, (unsigned int)Stats.LiveBytes);

    kbts_DestroyShapeContext(Churn);
    CHECK(Stats.LiveBytes == 0, "destroying the context left %u live bytes",
          (unsigned int)Stats.LiveBytes);
  }

  // A full font stack: the push fails and writes nothing, where it used to
  // write the parsed font through the null the stack handed back.
  {
    kbts_shape_context *Full = kbts_CreateShapeContext(0, 0);
    CHECK(Full != 0, "kbts_CreateShapeContext failed");

    int Pushed = 0;
    for(int Index = 0; Index < KBTS_CONTEXT_MAX_FONT_COUNT; ++Index)
    {
      if(kbts_ShapePushFontFromMemory(Full, SecondData, (int)SecondSize, 0)) ++Pushed;
    }
    CHECK(Pushed == KBTS_CONTEXT_MAX_FONT_COUNT, "%d of %d pushes onto an empty stack failed",
          KBTS_CONTEXT_MAX_FONT_COUNT - Pushed, KBTS_CONTEXT_MAX_FONT_COUNT);
    CHECK(kbts_ShapePushFontFromMemory(Full, SecondData, (int)SecondSize, 0) == 0,
          "the push past the end of the font stack reported a font");

    kbts_DestroyShapeContext(Full);
  }

  kbts_FreeFont(&Font);
  free(SecondBlob);
  free(SecondData);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("OK\n");
  return 0;
}
