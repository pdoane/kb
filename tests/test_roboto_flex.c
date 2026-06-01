// Variable-font tests targeting Roboto Flex.
// Roboto Flex has 13 axes and 7 FeatureVariations records — all rvrn swaps
// triggered by opsz / wght / wdth conditions — so it's a much richer test
// of the variable-font code paths than NotoSans.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;

#define CHECK(cond, ...) do { \
    if(!(cond)) { \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
      fprintf(stderr, __VA_ARGS__); \
      fprintf(stderr, "\n"); \
      ++Failures; \
    } \
  } while(0)

static void PrintTag(kbts_u32 Tag)
{
  putchar((int)(Tag >> 0) & 0xff);
  putchar((int)(Tag >> 8) & 0xff);
  putchar((int)(Tag >> 16) & 0xff);
  putchar((int)(Tag >> 24) & 0xff);
}

// Shape `Text` and write each glyph's ID + AdvanceX into Glyphs/Advances arrays.
// Returns the glyph count.
static int ShapeAndCapture(kbts_font *Font, const char *Text, int Length,
                            kbts_u16 *Glyphs, kbts_s32 *Advances, int Capacity)
{
  int Count = 0;
  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  kbts_ShapePushFont(Context, Font);
  kbts_ShapeBegin(Context, KBTS_DIRECTION_LTR, KBTS_LANGUAGE_DONT_KNOW);
  kbts_ShapeUtf8(Context, Text, Length, KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
  kbts_ShapeEnd(Context);

  kbts_run Run;
  while(kbts_ShapeRun(Context, &Run))
  {
    kbts_glyph *G;
    while(kbts_GlyphIteratorNext(&Run.Glyphs, &G))
    {
      if(Count < Capacity)
      {
        Glyphs[Count] = (kbts_u16)G->Id;
        Advances[Count] = G->AdvanceX;
      }
      ++Count;
    }
  }
  kbts_DestroyShapeContext(Context);
  return Count;
}

int main(int argc, char **argv)
{
  if(argc < 2)
  {
    fprintf(stderr, "usage: %s <RobotoFlex...ttf>\n", argv[0]);
    return 2;
  }

  void *FileData = 0;
  int FileSize = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &FileData, &FileSize);
  CHECK(kbts_FontIsValid(&Font), "kbts_FontFromFile failed (error=%u)", Font.Error);
  if(!kbts_FontIsValid(&Font)) return Failures;

  CHECK(kbts_FontIsVariable(&Font), "expected variable font");

  // 1. Axis enumeration: should find all 13 axes.
  kbts_u32 AxisCount = kbts_FontVariationAxisCount(&Font);
  printf("Roboto Flex: %u axes\n", AxisCount);
  CHECK(AxisCount == 13, "expected 13 axes, got %u", AxisCount);

  for(kbts_u32 I = 0; I < AxisCount; ++I)
  {
    kbts_axis_info Info;
    kbts_GetFontVariationAxis(&Font, I, &Info);
    printf("  axis ");
    PrintTag(Info.Tag);
    printf("  min=%.2f  default=%.2f  max=%.2f\n",
           (double)Info.MinValue / 65536.0,
           (double)Info.DefaultValue / 65536.0,
           (double)Info.MaxValue / 65536.0);
  }

  CHECK(kbts_FontVariationInstanceCount(&Font) == 20, "expected 20 instances, got %u",
        kbts_FontVariationInstanceCount(&Font));

  // 2. HVAR scaling sanity-check on an axis-rich font.
  printf("\nHVAR check: shape 'Hello' at three weights\n");
  kbts_u16 GlyphsThin[16], GlyphsBlack[16];
  kbts_s32 AdvThin[16], AdvBlack[16];

  kbts_axis_value Thin[]  = { { KBTS_FOURCC('w','g','h','t'), (kbts_s32)100 << 16 } };
  kbts_axis_value Black[] = { { KBTS_FOURCC('w','g','h','t'), (kbts_s32)900 << 16 } };

  kbts_SetFontVariations(&Font, Thin, 1);
  int ThinCount = ShapeAndCapture(&Font, "Hello", 5, GlyphsThin, AdvThin, 16);

  kbts_SetFontVariations(&Font, Black, 1);
  int BlackCount = ShapeAndCapture(&Font, "Hello", 5, GlyphsBlack, AdvBlack, 16);

  CHECK(ThinCount == BlackCount, "Glyph count differed: thin=%d black=%d", ThinCount, BlackCount);

  kbts_s32 ThinTotal = 0, BlackTotal = 0;
  for(int i = 0; i < ThinCount; ++i)
  {
    ThinTotal += AdvThin[i];
    BlackTotal += AdvBlack[i];
  }
  printf("  Thin  total advance = %d\n", ThinTotal);
  printf("  Black total advance = %d\n", BlackTotal);
  CHECK(ThinTotal < BlackTotal, "advances should grow with weight, got thin=%d black=%d",
        ThinTotal, BlackTotal);

  // 3. FeatureVariations: setting wght=900 triggers an rvrn substitution.
  //    Roboto Flex's alternate rvrn (lookup 0) substitutes the dollar sign
  //    (glyph 7 -> 588) and other currency symbols. Shape "$" at default
  //    vs heavy weight and verify the glyph id differs.
  printf("\nFeatureVariations check: '$' at default vs heavy weight\n");
  kbts_u16 GlyphsDefault[16], GlyphsAlt[16];
  kbts_s32 AdvDefault[16], AdvAlt[16];

  kbts_SetFontVariations(&Font, 0, 0);  // reset to default instance
  int DefCount = ShapeAndCapture(&Font, "$", 1, GlyphsDefault, AdvDefault, 16);

  // Hit Record 0: opsz in [0, 0.169] AND wght in [0.333, 1.0]. Default opsz=14;
  // normalized 0 == default. wght=900 -> normalized ~0.833. Both conditions match.
  kbts_axis_value Heavy[] = {
    { KBTS_FOURCC('o','p','s','z'), (kbts_s32)14 << 16 },
    { KBTS_FOURCC('w','g','h','t'), (kbts_s32)900 << 16 },
  };
  kbts_SetFontVariations(&Font, Heavy, 2);
  int AltCount = ShapeAndCapture(&Font, "$", 1, GlyphsAlt, AdvAlt, 16);

  CHECK(DefCount == AltCount, "glyph count differed: default=%d alt=%d", DefCount, AltCount);
  CHECK(DefCount == 1, "expected one glyph for $, got %d", DefCount);

  printf("  default $: glyph %u\n", GlyphsDefault[0]);
  printf("  heavy   $: glyph %u\n", GlyphsAlt[0]);
  CHECK(GlyphsDefault[0] != GlyphsAlt[0],
        "rvrn substitution did not fire: default and heavy both produced glyph %u",
        GlyphsDefault[0]);
  CHECK(GlyphsDefault[0] == 7 && GlyphsAlt[0] == 588,
        "expected $ to substitute 7 -> 588 at heavy weight, got %u -> %u",
        GlyphsDefault[0], GlyphsAlt[0]);

  // 4. MVAR check: Roboto Flex's MVAR has cpht/hcrs/xhgt. The cpht record
  //    is gated on YTUC (parametric uppercase axis), not wght — so we set
  //    YTUC to its max to verify cap height shifts.
  printf("\nMVAR check: capital height at YTUC default vs max\n");
  kbts_font_info2_2 InfoDefault, InfoBigCaps;
  InfoDefault.Base.Size = sizeof(InfoDefault);
  InfoBigCaps.Base.Size = sizeof(InfoBigCaps);

  kbts_SetFontVariations(&Font, 0, 0);
  kbts_GetFontInfo2(&Font, (kbts_font_info2 *)&InfoDefault);

  kbts_axis_value YTUC[] = { { KBTS_FOURCC('Y','T','U','C'), (kbts_s32)760 << 16 } };
  kbts_SetFontVariations(&Font, YTUC, 1);
  kbts_GetFontInfo2(&Font, (kbts_font_info2 *)&InfoBigCaps);

  printf("  default:        cap=%d\n", InfoDefault.CapitalHeight);
  printf("  YTUC=max(760):  cap=%d\n", InfoBigCaps.CapitalHeight);

  CHECK(InfoBigCaps.CapitalHeight > InfoDefault.CapitalHeight,
        "expected cap height to grow with YTUC (default=%d, YTUC max=%d)",
        InfoDefault.CapitalHeight, InfoBigCaps.CapitalHeight);
  // Region 16 contributes delta=100 at YTUC peak, so the difference is exactly +100.
  CHECK(InfoBigCaps.CapitalHeight - InfoDefault.CapitalHeight == 100,
        "expected cap delta of 100, got %d",
        InfoBigCaps.CapitalHeight - InfoDefault.CapitalHeight);

  // 5. GDEF IVS for GPOS Devices: kerning at non-default weights.
  //    To isolate the GPOS-Device contribution from HVAR, shape "A " (A then
  //    space) and "AV" at the same weight: the per-A advance difference
  //    between the two strings is the kerning adjustment. If that adjustment
  //    changes with weight, GDEF IVS is being applied to the GPOS Device.
  printf("\nGDEF IVS / GPOS Device check: AV kerning at default vs Black\n");
  kbts_u16 GA_def[8], GAv_def[8], GA_blk[8], GAv_blk[8];
  kbts_s32 AA_def[8], AAv_def[8], AA_blk[8], AAv_blk[8];

  kbts_SetFontVariations(&Font, 0, 0);
  ShapeAndCapture(&Font, "A ", 2, GA_def,  AA_def,  8);
  ShapeAndCapture(&Font, "AV", 2, GAv_def, AAv_def, 8);
  kbts_SetFontVariations(&Font, Black, 1);
  ShapeAndCapture(&Font, "A ", 2, GA_blk,  AA_blk,  8);
  ShapeAndCapture(&Font, "AV", 2, GAv_blk, AAv_blk, 8);

  kbts_s32 KernDef = AAv_def[0] - AA_def[0];
  kbts_s32 KernBlk = AAv_blk[0] - AA_blk[0];
  printf("  default: A-alone advance=%d, A-before-V advance=%d, kern=%d\n",
         AA_def[0], AAv_def[0], KernDef);
  printf("  black:   A-alone advance=%d, A-before-V advance=%d, kern=%d\n",
         AA_blk[0], AAv_blk[0], KernBlk);

  CHECK(KernDef < 0 && KernBlk < 0,
        "expected negative AV kerning at both weights, got default=%d black=%d",
        KernDef, KernBlk);
  CHECK(KernDef != KernBlk,
        "expected AV kerning to differ between weights via GDEF IVS, got %d == %d",
        KernDef, KernBlk);

  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures)
  {
    fprintf(stderr, "%d FAILURE(S)\n", Failures);
    return 1;
  }
  printf("\nOK\n");
  return 0;
}
