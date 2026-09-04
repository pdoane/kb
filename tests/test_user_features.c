// Feature-override reach test: which glyphs a lookup a caller turned on covers.
//
// A feature baked for one of the glyph-flagged features (frac, numr, dnom, the
// joining forms) carries that feature's glyph filter, and a lookup only runs on
// glyphs the shaper flagged for it. The shaper flags numr/frac/dnom around
// U+2044 FRACTION SLASH alone, so a caller that pushed frac -- what
// font-variant-numeric: diagonal-fractions asks for, per css-fonts-4 -- got the
// filter too, and the run kept its plain digits.
//
// Stack order: the context's feature stack applies the latest push of a tag,
// which the header documents at kbts_ShapePushFeature. The unique-override scan
// walks the stack from the top down so the latest push lands first, and then
// the hoisted copy was taken from the raw stack instead of from that scan, so
// the earliest push of a tag won.
//
// A caller's request covers every glyph its lookups match. The shape here is
// "1/2" with an ASCII solidus, which the shaper never flags, so the fraction
// forms appear only if the pushed frac reaches those glyphs. Nothing is
// hardcoded to a font: the expected result is what the same font produces for
// the fraction-slash spelling, which takes the automatic path.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Shapes [Text] with [PushCount] pushes of [PushedFeature], taking each push's
// value from [PushValues] in order.
static shaped ShapeWithStack(kbts_font *Font, const char *Text, kbts_u32 PushedFeature,
                             const int *PushValues, int PushCount)
{
  shaped Result;
  Result.Count = 0;

  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  kbts_ShapePushFont(Context, Font);
  kbts_ShapeBegin(Context, KBTS_DIRECTION_LTR, KBTS_LANGUAGE_DONT_KNOW);
  for(int PushIndex = 0; PushIndex < PushCount; ++PushIndex)
  {
    kbts_ShapePushFeature(Context, PushedFeature, PushValues[PushIndex]);
  }
  kbts_ShapeUtf8(Context, Text, (int)strlen(Text), KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
  kbts_ShapeEnd(Context);

  kbts_run Run;
  while(kbts_ShapeRun(Context, &Run))
  {
    kbts_glyph *Glyph;
    while(kbts_GlyphIteratorNext(&Run.Glyphs, &Glyph))
    {
      if(Result.Count < MAX_SHAPED_GLYPHS) Result.Ids[Result.Count] = Glyph->Id;
      Result.Count++;
    }
  }

  kbts_DestroyShapeContext(Context);
  return Result;
}

static shaped Shape(kbts_font *Font, const char *Text, kbts_u32 PushedFeature)
{
  int On = 1;
  return ShapeWithStack(Font, Text, PushedFeature, &On, PushedFeature ? 1 : 0);
}

static int SameShape(shaped *A, shaped *B)
{
  if(A->Count != B->Count) return 0;
  for(int Index = 0; Index < A->Count && Index < MAX_SHAPED_GLYPHS; ++Index)
  {
    if(A->Ids[Index] != B->Ids[Index]) return 0;
  }
  return 1;
}

static void Print(const char *Name, shaped *Shaped)
{
  printf("%s:", Name);
  for(int Index = 0; Index < Shaped->Count && Index < MAX_SHAPED_GLYPHS; ++Index)
  {
    printf(" %u", Shaped->Ids[Index]);
  }
  printf("\n");
}

int main(int argc, char **argv)
{
  if(argc < 2) { fprintf(stderr, "usage: %s <font.ttf>\n", argv[0]); return 2; }

  void *FileData = 0; int FileSize = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &FileData, &FileSize);
  CHECK(kbts_FontIsValid(&Font), "kbts_FontFromFile failed (error=%u)", Font.Error);
  if(!kbts_FontIsValid(&Font)) return Failures;

  shaped Plain = Shape(&Font, "1/2", 0);
  shaped Pushed = Shape(&Font, "1/2", KBTS_FEATURE_TAG_frac);
  shaped Automatic = Shape(&Font, "1\xE2\x81\x84" "2", 0);

  Print("1/2", &Plain);
  Print("1/2 with frac pushed", &Pushed);
  Print("1 U+2044 2", &Automatic);

  CHECK(!SameShape(&Automatic, &Plain),
        "the font applies no fraction forms to 1 U+2044 2, so it cannot tell this test anything");
  CHECK(!SameShape(&Pushed, &Plain),
        "frac pushed by the caller changed nothing: its lookups still ran only on the "
        "glyphs the shaper flagged for fractions");
  CHECK(SameShape(&Pushed, &Automatic),
        "frac pushed over an ASCII solidus did not produce the fraction forms the same "
        "font produces for a fraction slash");

  // A pushed feature reaches the glyphs its lookups cover, and no others.
  shaped Letters = Shape(&Font, "ab", 0);
  shaped LettersPushed = Shape(&Font, "ab", KBTS_FEATURE_TAG_frac);
  CHECK(SameShape(&Letters, &LettersPushed),
        "frac pushed over letters, which its lookups do not cover, changed them");

  // The feature stack applies the latest push of a tag. Two digits alone carry
  // the frac lookups' unconditional substitution, so the two stack orders shape
  // differently and each says which push won.
  shaped Digits = Shape(&Font, "12", 0);
  shaped DigitsFrac = Shape(&Font, "12", KBTS_FEATURE_TAG_frac);
  CHECK(!SameShape(&Digits, &DigitsFrac),
        "frac pushed over two digits changed nothing, so the stack order cannot be read here");

  static const int OnThenOff[2] = { 1, 0 };
  static const int OffThenOn[2] = { 0, 1 };
  shaped LastOff = ShapeWithStack(&Font, "12", KBTS_FEATURE_TAG_frac, OnThenOff, 2);
  shaped LastOn = ShapeWithStack(&Font, "12", KBTS_FEATURE_TAG_frac, OffThenOn, 2);

  Print("12", &Digits);
  Print("12 with frac on", &DigitsFrac);
  Print("12 with frac on then off", &LastOff);
  Print("12 with frac off then on", &LastOn);

  CHECK(SameShape(&LastOff, &Digits),
        "frac pushed on and then off shaped as if it were on: the stack applied the first "
        "push of the tag, not the latest");
  CHECK(SameShape(&LastOn, &DigitsFrac),
        "frac pushed off and then on shaped as if it were off: the stack applied the first "
        "push of the tag, not the latest");

  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("OK\n");
  return 0;
}
