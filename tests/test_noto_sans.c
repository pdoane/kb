// Variable-font tests targeting NotoSans[wdth,wght].ttf.
// A small variable font (2 axes: wght, wdth) — the simplest end-to-end
// exercise of fvar / avar / HVAR / MVAR.
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

static double Fixed16_16ToDouble(kbts_s32 V)
{
  return (double)V / 65536.0;
}

int main(int argc, char **argv)
{
  if(argc < 2)
  {
    fprintf(stderr, "usage: %s <variable-font.ttf>\n", argv[0]);
    return 2;
  }

  void *FileData = 0;
  int FileSize = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &FileData, &FileSize);

  CHECK(kbts_FontIsValid(&Font), "kbts_FontFromFile failed (error=%u)", Font.Error);
  if(!kbts_FontIsValid(&Font))
  {
    return Failures;
  }

  CHECK(kbts_FontIsVariable(&Font), "expected variable font, got static");

  kbts__fvar *Fvar = kbts__GetFvar(&Font);
  CHECK(Fvar != 0, "no fvar table parsed");
  if(!Fvar)
  {
    return Failures;
  }

  printf("fvar v%u.%u  axes=%u  instances=%u  axisSize=%u  instanceSize=%u\n",
         Fvar->Major, Fvar->Minor, Fvar->AxisCount, Fvar->InstanceCount,
         Fvar->AxisSize, Fvar->InstanceSize);

  // Public axis introspection.
  CHECK(kbts_FontVariationAxisCount(&Font) == Fvar->AxisCount,
        "public axis count %u != fvar axis count %u",
        kbts_FontVariationAxisCount(&Font), Fvar->AxisCount);

  for(kbts_u32 AxisIndex = 0; AxisIndex < kbts_FontVariationAxisCount(&Font); ++AxisIndex)
  {
    kbts_axis_info Info;
    kbts_GetFontVariationAxis(&Font, AxisIndex, &Info);
    printf("  axis ");
    PrintTag(Info.Tag);
    printf("  min=%.2f  default=%.2f  max=%.2f  flags=0x%x  nameId=%u\n",
           Fixed16_16ToDouble(Info.MinValue),
           Fixed16_16ToDouble(Info.DefaultValue),
           Fixed16_16ToDouble(Info.MaxValue),
           Info.Flags, Info.NameId);
  }

  // Public instance introspection.
  for(kbts_u32 InstanceIndex = 0; InstanceIndex < kbts_FontVariationInstanceCount(&Font); ++InstanceIndex)
  {
    kbts_instance_info InstInfo;
    kbts_s32 Coords[8];
    kbts_GetFontVariationInstance(&Font, InstanceIndex, &InstInfo, Coords, 8);
    printf("  instance subNameId=%u psNameId=%u coords=[",
           InstInfo.SubfamilyNameId, InstInfo.PostScriptNameId);
    for(kbts_u32 AxisIndex = 0; AxisIndex < kbts_FontVariationAxisCount(&Font); ++AxisIndex)
    {
      if(AxisIndex) printf(", ");
      printf("%.2f", Fixed16_16ToDouble(Coords[AxisIndex]));
    }
    printf("]\n");
  }

  // NotoSans[wdth,wght].ttf-specific assertions.
  // (Skip silently if loaded a different font.)
  if(Fvar->AxisCount == 2)
  {
    kbts__variation_axis_record *A0 = kbts__GetVariationAxis(Fvar, 0);
    kbts__variation_axis_record *A1 = kbts__GetVariationAxis(Fvar, 1);
    if(A0 && A1 &&
       A0->Tag == KBTS_FOURCC('w','g','h','t') &&
       A1->Tag == KBTS_FOURCC('w','d','t','h'))
    {
      CHECK(A0->MinValue == (100 << 16), "wght min expected 100, got %.2f", Fixed16_16ToDouble(A0->MinValue));
      CHECK(A0->MaxValue == (900 << 16), "wght max expected 900, got %.2f", Fixed16_16ToDouble(A0->MaxValue));
      CHECK(A0->DefaultValue == (400 << 16), "wght default expected 400, got %.2f", Fixed16_16ToDouble(A0->DefaultValue));
      CHECK(A1->MaxValue == (100 << 16), "wdth max expected 100, got %.2f", Fixed16_16ToDouble(A1->MaxValue));
      CHECK(Fvar->InstanceCount == 9, "expected 9 named instances, got %u", Fvar->InstanceCount);

    }
  }

  // Direct sparse-axis API.
  printf("\nkbts_SetFontVariations direct API:\n");
  {
    // Set wght=700 directly via the sparse API.
    kbts_axis_value V[] = { { KBTS_FOURCC('w','g','h','t'), (kbts_s32)700 << 16 } };
    kbts_SetFontVariations(&Font, V, 1);
    CHECK(Font.NormalizedCoords[0] == 9994, "wght=700 should give 9994 normalized, got %d", Font.NormalizedCoords[0]);
    CHECK(Font.NormalizedCoords[1] == 0, "wdth should default to 0, got %d", Font.NormalizedCoords[1]);

    kbts_font_info2 Info; Info.Size = sizeof(Info);
    kbts_GetFontInfo2(&Font, &Info);
    CHECK(Info.Weight == KBTS_FONT_WEIGHT_BOLD, "weight after wght=700 expected BOLD, got %d", Info.Weight);

    // ValueCount=0 resets to default-instance.
    kbts_SetFontVariations(&Font, 0, 0);
    CHECK(Font.NormalizedCoords[0] == 0, "reset should zero coords");
    kbts_GetFontInfo2(&Font, &Info);
    // Without override, weight comes from OS/2 (NotoSans default = Regular = 400).
    CHECK(Info.Weight == KBTS_FONT_WEIGHT_NORMAL, "after reset, weight should fall back to OS/2 default (NORMAL), got %d", Info.Weight);

    // Sparse: only set width; weight stays at OS/2 default in reporting.
    kbts_axis_value V2[] = { { KBTS_FOURCC('w','d','t','h'), ((kbts_s32)87 << 16) | 32768 } };
    kbts_SetFontVariations(&Font, V2, 1);
    CHECK(Font.NormalizedCoords[0] == 0, "wght should default to 0 when not set, got %d", Font.NormalizedCoords[0]);
    CHECK(Font.NormalizedCoords[1] == -6007, "wdth=87.5 should give -6007 normalized (avar-mapped), got %d", Font.NormalizedCoords[1]);
    kbts_GetFontInfo2(&Font, &Info);
    CHECK(Info.Width == KBTS_FONT_WIDTH_SEMI_CONDENSED, "width should be SEMI_CONDENSED, got %d", Info.Width);
    CHECK(Info.Weight == KBTS_FONT_WEIGHT_NORMAL, "weight should remain at OS/2 default when only width is set, got %d", Info.Weight);

    // Unknown tag is silently ignored.
    kbts_axis_value V3[] = { { KBTS_FOURCC('o','p','s','z'), (kbts_s32)12 << 16 } };
    kbts_SetFontVariations(&Font, V3, 1);
    CHECK(Font.NormalizedCoords[0] == 0 && Font.NormalizedCoords[1] == 0,
          "unknown tag should leave coords at 0 (got %d, %d)",
          Font.NormalizedCoords[0], Font.NormalizedCoords[1]);
  }

  // HVAR end-to-end: shape "Hello" at three weights, verify advances differ.
  printf("\nHVAR advance check: shape 'Hello' at three weights\n");
  {
    static const struct { kbts_font_weight W; const char *Name; } Cases[] = {
      { KBTS_FONT_WEIGHT_THIN,    "Thin"    },
      { KBTS_FONT_WEIGHT_NORMAL,  "Normal"  },
      { KBTS_FONT_WEIGHT_BLACK,   "Black"   },
    };

    kbts_s32 TotalAdvance[3] = {0, 0, 0};
    kbts_s32 PerGlyphAdvance[3][16] = {{0}};
    int GlyphCounts[3] = {0, 0, 0};

    for(int CaseIndex = 0; CaseIndex < 3; ++CaseIndex)
    {
      kbts_axis_value V[] = { { KBTS_FOURCC('w','g','h','t'), (kbts_s32)((kbts_un)Cases[CaseIndex].W * 100) << 16 } };
      kbts_SetFontVariations(&Font, V, 1);

      kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
      CHECK(Context != 0, "kbts_CreateShapeContext returned null");
      kbts_ShapePushFont(Context, &Font);

      kbts_ShapeBegin(Context, KBTS_DIRECTION_LTR, KBTS_LANGUAGE_DONT_KNOW);
      kbts_ShapeUtf8(Context, "Hello", 5, KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
      kbts_ShapeEnd(Context);

      printf("  %-8s ", Cases[CaseIndex].Name);
      kbts_run Run;
      while(kbts_ShapeRun(Context, &Run))
      {
        kbts_glyph *Glyph;
        while(kbts_GlyphIteratorNext(&Run.Glyphs, &Glyph))
        {
          if(GlyphCounts[CaseIndex] < 16)
          {
            PerGlyphAdvance[CaseIndex][GlyphCounts[CaseIndex]] = Glyph->AdvanceX;
          }
          TotalAdvance[CaseIndex] += Glyph->AdvanceX;
          GlyphCounts[CaseIndex]++;
          printf("[id=%u adv=%d] ", Glyph->Id, Glyph->AdvanceX);
        }
      }
      printf(" total=%d\n", TotalAdvance[CaseIndex]);

      kbts_DestroyShapeContext(Context);
    }

    CHECK(GlyphCounts[0] == GlyphCounts[1] && GlyphCounts[1] == GlyphCounts[2],
          "Glyph counts differed across weights: %d %d %d",
          GlyphCounts[0], GlyphCounts[1], GlyphCounts[2]);

    CHECK(TotalAdvance[0] != TotalAdvance[1],
          "Thin and Normal had identical total advance %d (HVAR not applied?)",
          TotalAdvance[0]);
    CHECK(TotalAdvance[1] != TotalAdvance[2],
          "Normal and Black had identical total advance %d (HVAR not applied?)",
          TotalAdvance[1]);
    CHECK(TotalAdvance[0] < TotalAdvance[1] && TotalAdvance[1] < TotalAdvance[2],
          "Expected advances to grow Thin < Normal < Black, got %d %d %d",
          TotalAdvance[0], TotalAdvance[1], TotalAdvance[2]);
  }

  // MVAR coverage check on NotoSans: it only has xhgt and stro, so
  // hasc/hdsc/hlgp/cpht should have zero delta. xhgt should shift with weight.
  printf("\nMVAR check\n");
  {
    kbts_axis_value Black[] = { { KBTS_FOURCC('w','g','h','t'), (kbts_s32)900 << 16 } };
    kbts_SetFontVariations(&Font, Black, 1);
    kbts_s32 DHasc = kbts__ApplyMvarDelta(&Font, KBTS_FOURCC('h','a','s','c'));
    kbts_s32 DHdsc = kbts__ApplyMvarDelta(&Font, KBTS_FOURCC('h','d','s','c'));
    kbts_s32 DXhgt = kbts__ApplyMvarDelta(&Font, KBTS_FOURCC('x','h','g','t'));
    kbts_s32 DStro = kbts__ApplyMvarDelta(&Font, KBTS_FOURCC('s','t','r','o'));
    printf("  Black deltas: hasc=%d hdsc=%d xhgt=%d stro=%d\n", DHasc, DHdsc, DXhgt, DStro);
    CHECK(DHasc == 0, "expected 0 (NotoSans has no hasc), got %d", DHasc);
    CHECK(DHdsc == 0, "expected 0 (NotoSans has no hdsc), got %d", DHdsc);
    CHECK(DXhgt != 0, "expected non-zero xhgt delta at Black weight, got %d", DXhgt);

    // GetFontInfo2 ascent/descent should be unchanged across weights (no MVAR records).
    kbts_font_info2_2 InfoBlack; InfoBlack.Base.Size = sizeof(InfoBlack);
    kbts_GetFontInfo2(&Font, (kbts_font_info2 *)&InfoBlack);

    kbts_axis_value Thin[] = { { KBTS_FOURCC('w','g','h','t'), (kbts_s32)100 << 16 } };
    kbts_SetFontVariations(&Font, Thin, 1);
    kbts_font_info2_2 InfoThin; InfoThin.Base.Size = sizeof(InfoThin);
    kbts_GetFontInfo2(&Font, (kbts_font_info2 *)&InfoThin);

    CHECK(InfoBlack.Ascent == InfoThin.Ascent,
          "Ascent changed across weight without MVAR hasc record (Black=%d Thin=%d)",
          InfoBlack.Ascent, InfoThin.Ascent);
    CHECK(InfoBlack.LineGap == InfoThin.LineGap,
          "LineGap changed unexpectedly (Black=%d Thin=%d)", InfoBlack.LineGap, InfoThin.LineGap);
  }

  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures)
  {
    fprintf(stderr, "%d FAILURE(S)\n", Failures);
    return 1;
  }
  printf("OK\n");
  return 0;
}
