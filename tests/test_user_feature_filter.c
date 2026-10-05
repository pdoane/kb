// Feature-override reach: which glyphs a lookup a caller turned on covers.
//
// A feature baked for one of the glyph-flagged features (frac, numr, dnom, the
// joining forms) carries that feature's glyph filter, and a lookup only runs on
// glyphs the shaper flagged for it. The shaper flags numr/frac/dnom around
// U+2044 FRACTION SLASH alone, so when a caller's frac (what
// font-variant-numeric: diagonal-fractions asks for, per css-fonts-4) keeps the
// filter, a run spelled with an ASCII solidus keeps its plain digits.
//
// The shape here is "1/2" with an ASCII solidus, which the shaper never flags,
// so the fraction forms appear only if the pushed frac reaches those glyphs.
// Nothing is hardcoded to a font: the expected result is what the same font
// produces for the fraction-slash spelling, which takes the automatic path.
//
// usage: test_user_feature_filter <font.ttf>   (NotoSans[wdth,wght].ttf)
#include <stdio.h>
#include <stdlib.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"
#include "shaping.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

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

  PrintShape("1/2", &Plain);
  PrintShape("1/2 with frac pushed", &Pushed);
  PrintShape("1 U+2044 2", &Automatic);

  CHECK(!SameShape(&Automatic, &Plain),
        "the font applies no fraction forms to 1 U+2044 2, so it cannot tell this test anything");
  CHECK(!SameShape(&Pushed, &Plain),
        "frac pushed by the caller changed nothing: its lookups ran only on the "
        "glyphs the shaper flagged for fractions");
  CHECK(SameShape(&Pushed, &Automatic),
        "frac pushed over an ASCII solidus did not produce the fraction forms the same "
        "font produces for a fraction slash");

  // A pushed feature reaches the glyphs its lookups cover, and no others.
  shaped Letters = Shape(&Font, "ab", 0);
  shaped LettersPushed = Shape(&Font, "ab", KBTS_FEATURE_TAG_frac);
  CHECK(SameShape(&Letters, &LettersPushed),
        "frac pushed over letters, which its lookups do not cover, changed them");

  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("test_user_feature_filter: OK\n");
  return 0;
}
