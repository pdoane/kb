// Run segmentation test: which script changes start a new run.
//
// Runs are shaped against one OpenType language system, which is selected by
// the script's OpenType tag. Segmentation compared the kbts_script enum
// instead, so scripts that share a tag were split: hiragana and katakana are
// both `kana`, and Japanese text alternating between them was cut into a run
// per script. Every cut costs a shaping pass and breaks the cross-script
// lookups the font applies over a kana sequence.
//
// The scripts here come from Unicode data, not from the font, so any font
// exercises this.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

static int RunCount(kbts_font *Font, const char *Text)
{
  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  kbts_ShapePushFont(Context, Font);
  kbts_ShapeBegin(Context, KBTS_DIRECTION_LTR, KBTS_LANGUAGE_DONT_KNOW);
  kbts_ShapeUtf8(Context, Text, (int)strlen(Text), KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
  kbts_ShapeEnd(Context);

  int Runs = 0;
  kbts_run Run;
  while(kbts_ShapeRun(Context, &Run)) ++Runs;

  kbts_DestroyShapeContext(Context);
  return Runs;
}

int main(int argc, char **argv)
{
  if(argc < 2) { fprintf(stderr, "usage: %s <font.ttf>\n", argv[0]); return 2; }

  void *FileData = 0; int FileSize = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &FileData, &FileSize);
  CHECK(kbts_FontIsValid(&Font), "kbts_FontFromFile failed (error=%u)", Font.Error);
  if(!kbts_FontIsValid(&Font)) return Failures;

  // U+3042 HIRAGANA LETTER A and U+30A2 KATAKANA LETTER A, both `kana`.
  const char *Hiragana = "\xE3\x81\x82";
  const char *Katakana = "\xE3\x82\xA2";

  char Mixed[16];
  strcpy(Mixed, Hiragana);
  strcat(Mixed, Katakana);
  CHECK(RunCount(&Font, Mixed) == 1, "hiragana followed by katakana shaped as %d runs, expected 1",
        RunCount(&Font, Mixed));

  char Alternating[32];
  strcpy(Alternating, Katakana);
  strcat(Alternating, Hiragana);
  strcat(Alternating, Katakana);
  CHECK(RunCount(&Font, Alternating) == 1, "katakana, hiragana, katakana shaped as %d runs, expected 1",
        RunCount(&Font, Alternating));

  // Scripts with different tags still start a run each: 'a' is `latn`, U+03B1
  // GREEK SMALL LETTER ALPHA is `grek`.
  char Latin[16];
  strcpy(Latin, "a");
  strcat(Latin, "\xCE\xB1");
  CHECK(RunCount(&Font, Latin) == 2, "latin followed by greek shaped as %d runs, expected 2",
        RunCount(&Font, Latin));

  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("OK\n");
  return 0;
}
