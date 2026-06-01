// Regression test for the Myanmar cluster-scan hang.
//
// The Myanmar non-cluster skip loop in kbts__BeginCluster did not advance the
// glyph, unlike its Indic/USE/Khmer siblings, so any Myanmar run whose
// cluster scan landed on an OTHER-class glyph (spaces, Latin, digits) spun
// forever. The syllabic classes come from Unicode data, not the font, so any
// font exercises this.
//
// A regression hangs rather than failing, so the whole test runs under an
// alarm: SIGALRM kills the process and make reports the failure.
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

static int ShapeCountGlyphs(kbts_font *Font, const char *Text, int Length)
{
  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  kbts_ShapePushFont(Context, Font);
  kbts_ShapeBegin(Context, KBTS_DIRECTION_LTR, KBTS_LANGUAGE_DONT_KNOW);
  kbts_ShapeUtf8(Context, Text, Length, KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
  kbts_ShapeEnd(Context);

  int GlyphCount = 0;
  kbts_run Run;
  while(kbts_ShapeRun(Context, &Run))
  {
    kbts_glyph *Glyph;
    while(kbts_GlyphIteratorNext(&Run.Glyphs, &Glyph)) GlyphCount++;
  }
  kbts_DestroyShapeContext(Context);
  return GlyphCount;
}

int main(int argc, char **argv)
{
  if(argc < 2) { fprintf(stderr, "usage: %s <font.ttf>\n", argv[0]); return 2; }

  alarm(10);

  void *FileData = 0; int FileSize = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &FileData, &FileSize);
  CHECK(kbts_FontIsValid(&Font), "kbts_FontFromFile failed (error=%u)", Font.Error);
  if(!kbts_FontIsValid(&Font)) return Failures;

  // U+1019 MYANMAR LETTER MA. The space between the letters is syllabic class
  // OTHER, so the second cluster scan starts on it.
  CHECK(ShapeCountGlyphs(&Font, "\xE1\x80\x99 \xE1\x80\x99", 7) == 3,
        "expected 3 glyphs for MA space MA");

  // Non-cluster glyphs at the start of the run and a run that is all
  // non-cluster glyphs.
  CHECK(ShapeCountGlyphs(&Font, "12 \xE1\x80\x99", 6) == 4,
        "expected 4 glyphs for digits space MA");
  CHECK(ShapeCountGlyphs(&Font, "\xE1\x80\x99 ab 1", 8) == 6,
        "expected 6 glyphs for MA space ab space 1");

  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("OK\n");
  return 0;
}
