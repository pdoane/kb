// Shape-config construction tests: which language system a config picks, and
// how many of that language system's features it bakes.
//
// Both run against synthetic GSUB tables, so the layout under test is written
// out right here instead of being inferred from a font file.
//
// Script selection: the OpenType lookup order is the requested script, then
// DFLT. kbts used to accept the first script in the list as a fallback, so a
// font without DFLT answered a request for one script with another script's
// language system -- an Arabic GSUB applied to Greek text.
//
// Feature baking: the user feature stage bakes every feature the language
// system lists, and the array it bakes into was sized by
// KBTS_MAX_SIMULTANEOUS_FEATURES. A language system listing more features than
// that lost the ones past the limit: their lookups never entered the config, so
// no kbts_feature_override could turn them on. FontWithFancyFeatures.otf, from
// the CSS fonts test suite, lists 50 features and never applied smcp.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

// Registered, non-default features, sorted by tag. Everything past index 31 is
// what the old limit dropped.
#define FEATURE_COUNT 43
static const char *FeatureTags[FEATURE_COUNT] =
{
  "afrc", "c2pc", "c2sc", "cswh", "dlig", "hist", "hlig", "lnum",
  "onum", "ordn", "pcap", "pnum", "ruby", "salt", "sinf", "smcp",
  "ss01", "ss02", "ss03", "ss04", "ss05", "ss06", "ss07", "ss08",
  "ss09", "ss10", "ss11", "ss12", "ss13", "ss14", "ss15", "ss16",
  "ss17", "ss18", "ss19", "ss20", "subs", "sups", "swsh", "titl",
  "tnum", "unic", "zero",
};

typedef struct script_spec
{
  const char *Tag;
  int DefaultFirstFeature;
  int DefaultFeatureCount;
  const char *LangSysTag; // 0 for a script with only a default language system
  int LangSysFirstFeature;
  int LangSysFeatureCount;
} script_spec;

static unsigned char Gsub[1 << 16];
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

static unsigned int PushLangSys(int FirstFeature, int FeatureCount)
{
  unsigned int At = Push(6 + 2 * (unsigned int)FeatureCount);
  WriteU16(At + 0, 0);      // LookupOrder
  WriteU16(At + 2, 0xFFFF); // RequiredFeatureIndex
  WriteU16(At + 4, (unsigned int)FeatureCount);
  for(int Index = 0; Index < FeatureCount; ++Index)
  {
    WriteU16(At + 6 + 2 * (unsigned int)Index, (unsigned int)(FirstFeature + Index));
  }
  return At;
}

// A GSUB whose feature list is FeatureTags, one single-substitution lookup per
// feature, and one script table per spec.
static void BuildGsub(const script_spec *Scripts, int ScriptCount)
{
  GsubSize = 0;

  unsigned int Header = Push(10);
  WriteU16(Header + 0, 1);
  WriteU16(Header + 2, 0);

  unsigned int ScriptList = Push(2 + 6 * (unsigned int)ScriptCount);
  WriteU16(Header + 4, ScriptList - Header);
  WriteU16(ScriptList, (unsigned int)ScriptCount);

  for(int ScriptIndex = 0; ScriptIndex < ScriptCount; ++ScriptIndex)
  {
    const script_spec *Spec = &Scripts[ScriptIndex];
    unsigned int Record = ScriptList + 2 + 6 * (unsigned int)ScriptIndex;
    memcpy(Gsub + Record, Spec->Tag, 4);

    int LangSysCount = Spec->LangSysTag ? 1 : 0;
    unsigned int Script = Push(4 + 6 * (unsigned int)LangSysCount);
    WriteU16(Record + 4, Script - ScriptList);
    WriteU16(Script + 2, (unsigned int)LangSysCount);
    if(LangSysCount) memcpy(Gsub + Script + 4, Spec->LangSysTag, 4);

    unsigned int DefaultLangSys = PushLangSys(Spec->DefaultFirstFeature, Spec->DefaultFeatureCount);
    WriteU16(Script + 0, DefaultLangSys - Script);

    if(LangSysCount)
    {
      unsigned int LangSys = PushLangSys(Spec->LangSysFirstFeature, Spec->LangSysFeatureCount);
      WriteU16(Script + 8, LangSys - Script);
    }
  }

  unsigned int FeatureList = Push(2 + 6 * FEATURE_COUNT);
  WriteU16(Header + 6, FeatureList - Header);
  WriteU16(FeatureList, FEATURE_COUNT);
  for(int FeatureIndex = 0; FeatureIndex < FEATURE_COUNT; ++FeatureIndex)
  {
    unsigned int Record = FeatureList + 2 + 6 * (unsigned int)FeatureIndex;
    memcpy(Gsub + Record, FeatureTags[FeatureIndex], 4);

    unsigned int Feature = Push(6);
    WriteU16(Record + 4, Feature - FeatureList);
    WriteU16(Feature + 0, 0); // FeatureParams
    WriteU16(Feature + 2, 1); // LookupIndexCount
    WriteU16(Feature + 4, (unsigned int)FeatureIndex);
  }

  unsigned int LookupList = Push(2 + 2 * FEATURE_COUNT);
  WriteU16(Header + 8, LookupList - Header);
  WriteU16(LookupList, FEATURE_COUNT);
  for(int LookupIndex = 0; LookupIndex < FEATURE_COUNT; ++LookupIndex)
  {
    unsigned int Lookup = Push(8);
    WriteU16(LookupList + 2 + 2 * (unsigned int)LookupIndex, Lookup - LookupList);
    WriteU16(Lookup + 0, 1); // Single substitution
    WriteU16(Lookup + 2, 0); // LookupFlag
    WriteU16(Lookup + 4, 1); // SubTableCount

    unsigned int Subtable = Push(6);
    WriteU16(Lookup + 6, Subtable - Lookup);
    WriteU16(Subtable + 0, 1); // Format 1
    WriteU16(Subtable + 4, 1); // DeltaGlyphId

    unsigned int Coverage = Push(6);
    WriteU16(Subtable + 2, Coverage - Subtable);
    WriteU16(Coverage + 0, 1); // Format 1
    WriteU16(Coverage + 2, 1); // GlyphCount
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

// An sfnt holding the GSUB built above plus maxp.
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

static kbts_font LoadFont(unsigned char *FontData, unsigned int FontSize, void **BlobOut)
{
  kbts_font Font = KBTS__ZERO;
  kbts_load_font_state State = KBTS__ZERO;
  int ScratchSize = 0;
  int OutputSize = 0;

  kbts_load_font_error Error = kbts_LoadFont(&Font, &State, FontData, (int)FontSize, 0, &ScratchSize, &OutputSize);
  if(Error == KBTS_LOAD_FONT_ERROR_NEED_TO_CREATE_BLOB)
  {
    void *Scratch = malloc((size_t)ScratchSize);
    void *Blob = malloc((size_t)OutputSize);
    Error = kbts_PlaceBlob(&Font, &State, Scratch, Blob);
    free(Scratch);
    *BlobOut = Blob;
  }
  CHECK(Error == KBTS_LOAD_FONT_ERROR_NONE, "kbts_LoadFont returned %u", Error);
  return Font;
}

// The language system a correct lookup lands on, found by walking the same
// tables the config walks.
static kbts__langsys *LangSysOf(kbts_font *Font, const char *ScriptTag, kbts_language Language)
{
  kbts__gsub_gpos *Header = kbts__BlobTableDataType(Font->Blob, KBTS_BLOB_TABLE_ID_GSUB, kbts__gsub_gpos);
  kbts__script_list *ScriptList = KBTS__POINTER_OFFSET(kbts__script_list, Header, Header->ScriptListOffset);
  kbts_u32 WantedTag = KBTS_FOURCC(ScriptTag[0], ScriptTag[1], ScriptTag[2], ScriptTag[3]);

  KBTS__FOR(ScriptIndex, 0, ScriptList->Count)
  {
    kbts__script_pointer ThisScript = kbts__GetScript(ScriptList, ScriptIndex);
    if(ThisScript.Tag != WantedTag) continue;

    kbts__langsys *Result = kbts__GetDefaultLangsys(ThisScript.Script);
    KBTS__FOR(LangSysIndex, 0, ThisScript.Script->Count)
    {
      kbts__langsys_pointer LangSys = kbts__GetLangsys(ThisScript.Script, LangSysIndex);
      if(LangSys.Tag == Language) Result = LangSys.Langsys;
    }
    return Result;
  }
  return 0;
}

static void CheckLangSys(const char *Name, kbts_font *Font, kbts_script Script, kbts_language Language,
                         const char *ExpectedScriptTag, kbts_language ExpectedLanguage)
{
  kbts_shape_config *Config = kbts_CreateShapeConfig(Font, Script, Language, 0, 0);
  CHECK(Config != 0, "%s: kbts_CreateShapeConfig failed", Name);
  if(!Config) return;

  kbts__langsys *Expected = LangSysOf(Font, ExpectedScriptTag, ExpectedLanguage);
  CHECK(Expected != 0, "%s: the test font has no %s language system", Name, ExpectedScriptTag);
  CHECK(Config->Langsys[KBTS_SHAPING_TABLE_GSUB] == Expected,
        "%s: config took language system %p, expected %s's %p", Name,
        (void *)Config->Langsys[KBTS_SHAPING_TABLE_GSUB], ExpectedScriptTag, (void *)Expected);

  kbts_DestroyShapeConfig(Config);
}

// Nonzero if an override on FeatureTag turns on a lookup in the config.
static int OverrideEnablesLookup(kbts_shape_config *Config, const char *FeatureTag)
{
  kbts_feature_override Override;
  Override.Tag = KBTS_FOURCC(FeatureTag[0], FeatureTag[1], FeatureTag[2], FeatureTag[3]);
  Override.Value = 1;

  kbts_glyph_config *GlyphConfig = kbts_CreateGlyphConfig(Config, &Override, 1, 0, 0);
  if(!GlyphConfig) return 0;

  kbts_un SequentialLookupCount = kbts__SequentialLookupCount(Config);
  kbts_un LastIndex = SequentialLookupCount ? (SequentialLookupCount - 1) : 0;
  kbts__matrix_index LastMatrixIndex = kbts__IdSequentialLookupMatrixIndex(LastIndex, 0, SequentialLookupCount);

  kbts_u32 Enabled = 0;
  KBTS__FOR(WordIndex, 0, LastMatrixIndex.WordIndex + 1)
  {
    Enabled |= GlyphConfig->EnabledLookupBits[WordIndex];
  }

  kbts_DestroyGlyphConfig(GlyphConfig);
  return Enabled != 0;
}

int main(void)
{
  // A font with DFLT, and a Latin script whose language systems differ.
  static const script_spec WithDefault[] =
  {
    { "DFLT", 0, 1, 0, 0, 0 },
    { "arab", 1, 1, 0, 0, 0 },
    { "latn", 0, FEATURE_COUNT, "TRK ", 0, 2 },
  };
  // The same font, without a DFLT script.
  static const script_spec WithoutDefault[] =
  {
    { "arab", 1, 1, 0, 0, 0 },
    { "latn", 0, FEATURE_COUNT, "TRK ", 0, 2 },
  };

  {
    unsigned int FontSize = 0;
    BuildGsub(WithDefault, 3);
    unsigned char *FontData = BuildFont(&FontSize);
    void *Blob = 0;
    kbts_font Font = LoadFont(FontData, FontSize, &Blob);

    if(kbts_FontIsValid(&Font))
    {
      CheckLangSys("requested script", &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW,
                   "latn", KBTS_LANGUAGE_DONT_KNOW);
      CheckLangSys("requested language system", &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_TURKISH,
                   "latn", KBTS_LANGUAGE_TURKISH);
      CheckLangSys("language the script does not list", &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_ARABIC,
                   "latn", KBTS_LANGUAGE_DONT_KNOW);
      CheckLangSys("script the font does not list", &Font, KBTS_SCRIPT_GREEK, KBTS_LANGUAGE_DONT_KNOW,
                   "DFLT", KBTS_LANGUAGE_DONT_KNOW);

      // Every feature the language system lists is available to an override,
      // including the ones past KBTS_MAX_SIMULTANEOUS_FEATURES.
      kbts_shape_config *Config = kbts_CreateShapeConfig(&Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW, 0, 0);
      CHECK(Config != 0, "kbts_CreateShapeConfig failed");
      if(Config)
      {
        CHECK(OverrideEnablesLookup(Config, FeatureTags[0]),
              "an override on the first listed feature (%s) enabled no lookup", FeatureTags[0]);
        CHECK(OverrideEnablesLookup(Config, FeatureTags[FEATURE_COUNT - 1]),
              "an override on feature %d of %d (%s) enabled no lookup: the config dropped it",
              FEATURE_COUNT, FEATURE_COUNT, FeatureTags[FEATURE_COUNT - 1]);
        kbts_DestroyShapeConfig(Config);
      }
    }

    kbts_FreeFont(&Font);
    free(Blob);
    free(FontData);
  }

  {
    unsigned int FontSize = 0;
    BuildGsub(WithoutDefault, 2);
    unsigned char *FontData = BuildFont(&FontSize);
    void *Blob = 0;
    kbts_font Font = LoadFont(FontData, FontSize, &Blob);

    if(kbts_FontIsValid(&Font))
    {
      // Neither the requested script nor DFLT is present. The Arabic script
      // table, which the old first-entry fallback landed on, is not an answer.
      kbts_shape_config *Config = kbts_CreateShapeConfig(&Font, KBTS_SCRIPT_GREEK, KBTS_LANGUAGE_DONT_KNOW, 0, 0);
      CHECK(Config != 0, "kbts_CreateShapeConfig failed");
      if(Config)
      {
        CHECK(Config->Langsys[KBTS_SHAPING_TABLE_GSUB] != LangSysOf(&Font, "arab", KBTS_LANGUAGE_DONT_KNOW),
              "a request for Greek took the Arabic language system");
        kbts_DestroyShapeConfig(Config);
      }
    }

    kbts_FreeFont(&Font);
    free(Blob);
    free(FontData);
  }

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("OK\n");
  return 0;
}
