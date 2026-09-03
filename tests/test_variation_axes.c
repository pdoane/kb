// kbts_GetFontVariation against the axis limit.
//
// A kbts_font_variation holds KBTS_MAX_VARIATION_AXES normalized coordinates.
// A font with more axes than that used to come back as the default instance
// with nothing said, so a caller that asked for wght=900 shaped at wght=400
// and had no way to find out. The design points here are asked for against
// synthetic fvar tables of 3, KBTS_MAX_VARIATION_AXES and one past it.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

static unsigned char Fvar[1 << 12];
static unsigned int FvarSize;

static void WriteU16(unsigned char *At, unsigned int Value)
{
  At[0] = (unsigned char)(Value >> 8);
  At[1] = (unsigned char)(Value >> 0);
}

static void WriteU32(unsigned char *At, unsigned int Value)
{
  At[0] = (unsigned char)(Value >> 24);
  At[1] = (unsigned char)(Value >> 16);
  At[2] = (unsigned char)(Value >> 8);
  At[3] = (unsigned char)(Value >> 0);
}

// An fvar of [AxisCount] axes. The first is wght over [0, 1000] with a default
// of 0, so a normalized coordinate is the user value over 1000; the rest are
// filler tagged 'ax00' upwards.
static void BuildFvar(int AxisCount)
{
  unsigned int AxesArrayOffset = 16;
  unsigned int AxisSize = 20;
  FvarSize = AxesArrayOffset + AxisSize * (unsigned int)AxisCount;
  memset(Fvar, 0, FvarSize);

  WriteU16(Fvar + 0, 1);
  WriteU16(Fvar + 2, 0);
  WriteU16(Fvar + 4, AxesArrayOffset);
  WriteU16(Fvar + 6, 2);
  WriteU16(Fvar + 8, (unsigned int)AxisCount);
  WriteU16(Fvar + 10, AxisSize);
  WriteU16(Fvar + 12, 0);
  WriteU16(Fvar + 14, 4 + 4 * (unsigned int)AxisCount); // no instances, but the size still has to fit one

  for(int Index = 0; Index < AxisCount; ++Index)
  {
    unsigned char *Axis = Fvar + AxesArrayOffset + AxisSize * (unsigned int)Index;
    if(Index == 0)
    {
      memcpy(Axis, "wght", 4);
    }
    else
    {
      Axis[0] = 'a';
      Axis[1] = 'x';
      Axis[2] = (unsigned char)('0' + (Index / 10));
      Axis[3] = (unsigned char)('0' + (Index % 10));
    }
    WriteU32(Axis + 4, 0);                 // min 0
    WriteU32(Axis + 8, 0);                 // default 0
    WriteU32(Axis + 12, 1000u << 16);      // max 1000
    WriteU16(Axis + 16, 0);
    WriteU16(Axis + 18, (unsigned int)(256 + Index));
  }
}

// maxp 0.5, so the blob has a glyph count.
static const unsigned char Maxp[] = { 0x00, 0x00, 0x50, 0x00, 0x00, 0x08 };

// An sfnt holding maxp, plus the fvar built above when [WithFvar].
static unsigned char *BuildFont(int WithFvar, unsigned int *SizeOut)
{
  unsigned int TableCount = WithFvar ? 2u : 1u;
  unsigned int DirectorySize = 12 + 16 * TableCount;
  unsigned int FvarAt = (DirectorySize + 3) & ~3u;
  unsigned int MaxpAt = WithFvar ? ((FvarAt + FvarSize + 3) & ~3u) : FvarAt;
  unsigned int Size = MaxpAt + (unsigned int)sizeof(Maxp);

  unsigned char *Font = (unsigned char *)calloc(1, Size);
  WriteU32(Font + 0, 0x00010000);
  WriteU16(Font + 4, TableCount);

  unsigned int MaxpRecord = 12;
  if(WithFvar)
  {
    memcpy(Font + 12, "fvar", 4);
    WriteU32(Font + 12 + 8, FvarAt);
    WriteU32(Font + 12 + 12, FvarSize);
    memcpy(Font + FvarAt, Fvar, FvarSize);
    MaxpRecord = 28;
  }

  memcpy(Font + MaxpRecord, "maxp", 4);
  WriteU32(Font + MaxpRecord + 8, MaxpAt);
  WriteU32(Font + MaxpRecord + 12, (unsigned int)sizeof(Maxp));
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

// The design point half way up the wght axis, and what the font said about it.
static void CheckHalfWeight(const char *Name, int AxisCount, int WithFvar,
                            kbts_b32 ExpectedResult, kbts_b32 ExpectedNonDefault)
{
  unsigned int FontSize = 0;
  BuildFvar(AxisCount);
  unsigned char *FontData = BuildFont(WithFvar, &FontSize);
  void *Blob = 0;
  kbts_font Font = KBTS__ZERO;
  LoadFont(&Font, FontData, FontSize, &Blob);

  if(kbts_FontIsValid(&Font))
  {
    kbts_axis_value Value;
    Value.Tag = KBTS_FOURCC('w','g','h','t');
    Value.Value = 500 << 16;

    kbts_font_variation Variation;
    kbts_b32 Result = kbts_GetFontVariation(&Font, &Value, 1, &Variation);

    CHECK(Result == ExpectedResult, "%s: kbts_GetFontVariation returned %d, expected %d",
          Name, (int)Result, (int)ExpectedResult);
    CHECK(Variation.HasNonDefaultCoordinate == ExpectedNonDefault,
          "%s: HasNonDefaultCoordinate is %d, expected %d",
          Name, (int)Variation.HasNonDefaultCoordinate, (int)ExpectedNonDefault);
    if(ExpectedNonDefault)
    {
      CHECK(Variation.NormalizedCoords[0] == 8192,
            "%s: wght=500 of [0, 1000] normalized to %d, expected 8192",
            Name, (int)Variation.NormalizedCoords[0]);
      CHECK(Variation.AxisCount == (kbts_u32)AxisCount,
            "%s: the design point names %u axes, expected %d",
            Name, Variation.AxisCount, AxisCount);
    }
  }

  kbts_FreeFont(&Font);
  free(Blob);
  free(FontData);
}

int main(void)
{
  CheckHalfWeight("three axes", 3, 1, 1, 1);
  // Twenty axes is past the limit of 16 a kbts_font_variation used to hold.
  CheckHalfWeight("twenty axes", 20, 1, 1, 1);
  CheckHalfWeight("the axis limit", KBTS_MAX_VARIATION_AXES, 1, 1, 1);
  CheckHalfWeight("one axis past the limit", KBTS_MAX_VARIATION_AXES + 1, 1, 0, 0);
  CheckHalfWeight("no fvar", 3, 0, 1, 0);

  // The font still reports the axes it has, whatever a design point can hold.
  {
    unsigned int FontSize = 0;
    BuildFvar(KBTS_MAX_VARIATION_AXES + 1);
    unsigned char *FontData = BuildFont(1, &FontSize);
    void *Blob = 0;
    kbts_font Font = KBTS__ZERO;
    LoadFont(&Font, FontData, FontSize, &Blob);

    if(kbts_FontIsValid(&Font))
    {
      CHECK(kbts_FontIsVariable(&Font), "the font with an fvar is not variable");
      CHECK(kbts_FontVariationAxisCount(&Font) == (kbts_u32)(KBTS_MAX_VARIATION_AXES + 1),
            "kbts_FontVariationAxisCount returned %u, expected %d",
            kbts_FontVariationAxisCount(&Font), KBTS_MAX_VARIATION_AXES + 1);
    }

    kbts_FreeFont(&Font);
    free(Blob);
    free(FontData);
  }

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("OK\n");
  return 0;
}
