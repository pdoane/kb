// Regression test for the Hangul normalization dropping codepoint indices.
//
// kbts normalizes each Hangul syllable to the L, V and T glyphs the font
// holds, or to the syllable's own glyph where it holds none. Both paths
// built their glyphs with kbts_CodepointToGlyph, which starts every glyph
// at user id 0, so every glyph of a Hangul run reported the run's first
// codepoint. Any font exercises the syllable path, since a font with no
// Hangul takes it for every syllable.
#include <stdio.h>
#include <stdlib.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

int main(int argc, char **argv)
{
  if(argc < 2) { fprintf(stderr, "usage: %s <font.ttf>\n", argv[0]); return 2; }

  void *FileData = 0;
  kbts_un FileSize = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &FileData, &FileSize);
  CHECK(kbts_FontIsValid(&Font), "kbts_FontFromFile failed (error=%u)", Font.Error);
  if(!kbts_FontIsValid(&Font)) return Failures;

  // "한글이 한글이": seven codepoints, a space between two syllable runs.
  const char *Text = "\xED\x95\x9C\xEA\xB8\x80\xEC\x9D\xB4 \xED\x95\x9C\xEA\xB8\x80\xEC\x9D\xB4";
  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  kbts_ShapePushFont(Context, &Font);
  kbts_ShapeBegin(Context, KBTS_DIRECTION_LTR, KBTS_LANGUAGE_DONT_KNOW);
  kbts_ShapeUtf8(Context, Text, 19, KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
  kbts_ShapeEnd(Context);

  int GlyphIndex = 0;
  kbts_run Run;
  while(kbts_ShapeRun(Context, &Run))
  {
    kbts_glyph *Glyph;
    while(kbts_GlyphIteratorNext(&Run.Glyphs, &Glyph))
    {
      CHECK(Glyph->UserIdOrCodepointIndex == (kbts_u32)GlyphIndex,
            "glyph %d reports codepoint index %u", GlyphIndex, Glyph->UserIdOrCodepointIndex);
      GlyphIndex++;
    }
  }
  CHECK(GlyphIndex == 7, "expected 7 glyphs, got %d", GlyphIndex);
  kbts_DestroyShapeContext(Context);
  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures) fprintf(stderr, "%d failure(s)\n", Failures);
  else printf("test_hangul_index: OK\n");
  return Failures ? 1 : 0;
}
