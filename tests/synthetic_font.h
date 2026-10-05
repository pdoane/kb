// Synthetic fonts for the tests: an sfnt holding a GSUB, cmap and maxp,
// written out byte by byte so the layout under test is stated in the test, not
// inferred from a font file.
//
// The GSUB lists SyntheticFeatureTags[0..FeatureCount), each feature owning one
// single-substitution lookup that maps glyph 1 to glyph 2, and one script table
// per synthetic_script. Include after kb_text_shape.h.
#ifndef SYNTHETIC_FONT_H
#define SYNTHETIC_FONT_H

#include <stdlib.h>
#include <string.h>

// Registered, non-default features, sorted by tag.
#define SYNTHETIC_FEATURE_COUNT 43
static const char *SyntheticFeatureTags[SYNTHETIC_FEATURE_COUNT] =
{
  "afrc", "c2pc", "c2sc", "cswh", "dlig", "hist", "hlig", "lnum",
  "onum", "ordn", "pcap", "pnum", "ruby", "salt", "sinf", "smcp",
  "ss01", "ss02", "ss03", "ss04", "ss05", "ss06", "ss07", "ss08",
  "ss09", "ss10", "ss11", "ss12", "ss13", "ss14", "ss15", "ss16",
  "ss17", "ss18", "ss19", "ss20", "subs", "sups", "swsh", "titl",
  "tnum", "unic", "zero",
};

typedef struct synthetic_script
{
  const char *Tag;
  int DefaultFirstFeature;
  int DefaultFeatureCount;
  const char *LangSysTag; // 0 for a script with only a default language system
  int LangSysFirstFeature;
  int LangSysFeatureCount;
} synthetic_script;

static unsigned char SyntheticGsub[1 << 16];
static unsigned int SyntheticGsubSize;

static void SyntheticWriteU16(unsigned int At, unsigned int Value)
{
  SyntheticGsub[At + 0] = (unsigned char)(Value >> 8);
  SyntheticGsub[At + 1] = (unsigned char)(Value >> 0);
}

static unsigned int SyntheticPush(unsigned int Size)
{
  unsigned int At = SyntheticGsubSize;
  SyntheticGsubSize += Size;
  return At;
}

static unsigned int SyntheticPushLangSys(int FirstFeature, int FeatureCount)
{
  unsigned int At = SyntheticPush(6 + 2 * (unsigned int)FeatureCount);
  SyntheticWriteU16(At + 0, 0);      // LookupOrder
  SyntheticWriteU16(At + 2, 0xFFFF); // RequiredFeatureIndex
  SyntheticWriteU16(At + 4, (unsigned int)FeatureCount);
  for(int Index = 0; Index < FeatureCount; ++Index)
  {
    SyntheticWriteU16(At + 6 + 2 * (unsigned int)Index, (unsigned int)(FirstFeature + Index));
  }
  return At;
}

static void SyntheticBuildGsub(const synthetic_script *Scripts, int ScriptCount, int FeatureCount)
{
  SyntheticGsubSize = 0;

  unsigned int Header = SyntheticPush(10);
  SyntheticWriteU16(Header + 0, 1);
  SyntheticWriteU16(Header + 2, 0);

  unsigned int ScriptList = SyntheticPush(2 + 6 * (unsigned int)ScriptCount);
  SyntheticWriteU16(Header + 4, ScriptList - Header);
  SyntheticWriteU16(ScriptList, (unsigned int)ScriptCount);

  for(int ScriptIndex = 0; ScriptIndex < ScriptCount; ++ScriptIndex)
  {
    const synthetic_script *Spec = &Scripts[ScriptIndex];
    unsigned int Record = ScriptList + 2 + 6 * (unsigned int)ScriptIndex;
    memcpy(SyntheticGsub + Record, Spec->Tag, 4);

    int LangSysCount = Spec->LangSysTag ? 1 : 0;
    unsigned int Script = SyntheticPush(4 + 6 * (unsigned int)LangSysCount);
    SyntheticWriteU16(Record + 4, Script - ScriptList);
    SyntheticWriteU16(Script + 2, (unsigned int)LangSysCount);
    if(LangSysCount) memcpy(SyntheticGsub + Script + 4, Spec->LangSysTag, 4);

    unsigned int DefaultLangSys = SyntheticPushLangSys(Spec->DefaultFirstFeature, Spec->DefaultFeatureCount);
    SyntheticWriteU16(Script + 0, DefaultLangSys - Script);

    if(LangSysCount)
    {
      unsigned int LangSys = SyntheticPushLangSys(Spec->LangSysFirstFeature, Spec->LangSysFeatureCount);
      SyntheticWriteU16(Script + 8, LangSys - Script);
    }
  }

  unsigned int FeatureList = SyntheticPush(2 + 6 * (unsigned int)FeatureCount);
  SyntheticWriteU16(Header + 6, FeatureList - Header);
  SyntheticWriteU16(FeatureList, (unsigned int)FeatureCount);
  for(int FeatureIndex = 0; FeatureIndex < FeatureCount; ++FeatureIndex)
  {
    unsigned int Record = FeatureList + 2 + 6 * (unsigned int)FeatureIndex;
    memcpy(SyntheticGsub + Record, SyntheticFeatureTags[FeatureIndex], 4);

    unsigned int Feature = SyntheticPush(6);
    SyntheticWriteU16(Record + 4, Feature - FeatureList);
    SyntheticWriteU16(Feature + 0, 0); // FeatureParams
    SyntheticWriteU16(Feature + 2, 1); // LookupIndexCount
    SyntheticWriteU16(Feature + 4, (unsigned int)FeatureIndex);
  }

  unsigned int LookupList = SyntheticPush(2 + 2 * (unsigned int)FeatureCount);
  SyntheticWriteU16(Header + 8, LookupList - Header);
  SyntheticWriteU16(LookupList, (unsigned int)FeatureCount);
  for(int LookupIndex = 0; LookupIndex < FeatureCount; ++LookupIndex)
  {
    unsigned int Lookup = SyntheticPush(8);
    SyntheticWriteU16(LookupList + 2 + 2 * (unsigned int)LookupIndex, Lookup - LookupList);
    SyntheticWriteU16(Lookup + 0, 1); // Single substitution
    SyntheticWriteU16(Lookup + 2, 0); // LookupFlag
    SyntheticWriteU16(Lookup + 4, 1); // SubTableCount

    unsigned int Subtable = SyntheticPush(6);
    SyntheticWriteU16(Lookup + 6, Subtable - Lookup);
    SyntheticWriteU16(Subtable + 0, 1); // Format 1
    SyntheticWriteU16(Subtable + 4, 1); // DeltaGlyphId

    unsigned int Coverage = SyntheticPush(6);
    SyntheticWriteU16(Subtable + 2, Coverage - Subtable);
    SyntheticWriteU16(Coverage + 0, 1); // Format 1
    SyntheticWriteU16(Coverage + 2, 1); // GlyphCount
    SyntheticWriteU16(Coverage + 4, 1);
  }
}

// A GSUB with one latn script whose default language system lists the first
// [FeatureCount] features.
static void SyntheticBuildLatinGsub(int FeatureCount)
{
  synthetic_script Latin = { "latn", 0, FeatureCount, 0, 0, 0 };
  SyntheticBuildGsub(&Latin, 1, FeatureCount);
}

// maxp 0.5, so the blob has a glyph count to size its lookup matrices with.
static const unsigned char SyntheticMaxp[] = { 0x00, 0x00, 0x50, 0x00, 0x00, 0x08 };

static void SyntheticWriteU32(unsigned char *At, unsigned int Value)
{
  At[0] = (unsigned char)(Value >> 24);
  At[1] = (unsigned char)(Value >> 16);
  At[2] = (unsigned char)(Value >> 8);
  At[3] = (unsigned char)(Value >> 0);
}

// cmap with one format 4 subtable holding only the closing 0xFFFF segment, so
// the font maps no codepoint but every loader finds a cmap.
static const unsigned char SyntheticCmap[] =
{
  0x00, 0x00, 0x00, 0x01,                         // version, numTables
  0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0C, // Windows Unicode BMP, offset 12
  0x00, 0x04, 0x00, 0x18, 0x00, 0x00,             // format 4, length 24, language
  0x00, 0x02, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, // segCountX2, searchRange, entrySelector, rangeShift
  0xFF, 0xFF, 0x00, 0x00,                         // endCode, reservedPad
  0xFF, 0xFF, 0x00, 0x01, 0x00, 0x00,             // startCode, idDelta, idRangeOffset
};

// An sfnt holding the GSUB built last plus cmap and maxp, allocated with
// malloc.
static unsigned char *SyntheticBuildFont(unsigned int *SizeOut)
{
  unsigned int TableCount = 3;
  unsigned int DirectorySize = 12 + 16 * TableCount;
  unsigned int GsubAt = (DirectorySize + 3) & ~3u;
  unsigned int CmapAt = (GsubAt + SyntheticGsubSize + 3) & ~3u;
  unsigned int MaxpAt = (CmapAt + (unsigned int)sizeof(SyntheticCmap) + 3) & ~3u;
  unsigned int Size = MaxpAt + (unsigned int)sizeof(SyntheticMaxp);

  unsigned char *Font = (unsigned char *)calloc(1, Size);
  SyntheticWriteU32(Font + 0, 0x00010000);
  Font[4] = 0;
  Font[5] = (unsigned char)TableCount;

  // Table records sorted by tag.
  memcpy(Font + 12, "GSUB", 4);
  SyntheticWriteU32(Font + 12 + 8, GsubAt);
  SyntheticWriteU32(Font + 12 + 12, SyntheticGsubSize);
  memcpy(Font + GsubAt, SyntheticGsub, SyntheticGsubSize);

  memcpy(Font + 28, "cmap", 4);
  SyntheticWriteU32(Font + 28 + 8, CmapAt);
  SyntheticWriteU32(Font + 28 + 12, (unsigned int)sizeof(SyntheticCmap));
  memcpy(Font + CmapAt, SyntheticCmap, sizeof(SyntheticCmap));

  memcpy(Font + 44, "maxp", 4);
  SyntheticWriteU32(Font + 44 + 8, MaxpAt);
  SyntheticWriteU32(Font + 44 + 12, (unsigned int)sizeof(SyntheticMaxp));
  memcpy(Font + MaxpAt, SyntheticMaxp, sizeof(SyntheticMaxp));

  *SizeOut = Size;
  return Font;
}

// Loads [FontData] into [Font], placing the blob in a malloc'd buffer written
// to [BlobOut]. Returns the load error.
static kbts_load_font_error SyntheticLoadFont(kbts_font *Font, unsigned char *FontData, unsigned int FontSize, void **BlobOut)
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
  return Error;
}

// Nonzero if an override on [FeatureTag] turns on any of [Config]'s lookups.
static int SyntheticOverrideEnablesLookup(kbts_shape_config *Config, const char *FeatureTag)
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

// The interned info of the font on top of [Context]'s stack, the key the
// context's shape config cache is asked with.
static kbts__interned_context_font_info *SyntheticTopFontInfo(kbts_shape_context *Context)
{
  kbts__context_font_block *Block = (kbts__context_font_block *)Context->FontBlockSentinel.Prev;
  kbts__context_font *Font = &Block->Fonts[(Context->FontCount - 1) & (KBTS__CONTEXT_FONTS_PER_BLOCK - 1)];
  return kbts__InternContextFontInfo(Context, &Font->Info);
}

static kbts_un SyntheticCachedShapeConfigCount(kbts_shape_context *Context)
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

#endif
