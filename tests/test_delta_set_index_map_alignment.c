// Regression test for unaligned delta set index map reads.
//
// A delta set index map can start at any byte offset in its table, and Noto
// Sans places its HVAR advance map at an odd one. The byteswap and the HVAR
// advance lookup read the map's entry count through a kbts_u16 or kbts_u32
// pointer, which is undefined behavior at an odd address. The reads go
// through kbts__ReadU16Unaligned and kbts__ReadU32Unaligned instead.
//
// The fault is only visible under `make ubsan`, which aborts on it. Without a
// sanitizer, the test checks the HVAR path runs at all: Noto Sans at wght=900
// shapes wider than at its default.
//
// usage: test_delta_set_index_map_alignment <NotoSans[wdth,wght].ttf>
#include <stdio.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

static int ShapedWidth(kbts_font *Font, kbts_variation *Variations, int VariationCount, const char *Text)
{
  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  kbts_ShapePushFont2(Context, Font, Variations, VariationCount, 0);
  kbts_ShapeBegin(Context, KBTS_DIRECTION_DONT_KNOW, KBTS_LANGUAGE_DONT_KNOW);
  kbts_ShapeUtf8(Context, Text, (int)strlen(Text), KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
  kbts_ShapeEnd(Context);

  int Result = 0;
  kbts_run Run;
  while(kbts_ShapeRun(Context, &Run))
  {
    kbts_glyph *Glyph;
    while(kbts_GlyphIteratorNext(&Run.Glyphs, &Glyph)) Result += Glyph->AdvanceX;
  }

  CHECK(kbts_ShapeError(Context) == KBTS_SHAPE_ERROR_NONE, "shaping set error %u", kbts_ShapeError(Context));
  kbts_DestroyShapeContext(Context);
  return Result;
}

int main(int argc, char **argv)
{
  if(argc < 2) { fprintf(stderr, "usage: %s <NotoSans[wdth,wght].ttf>\n", argv[0]); return 2; }

  void *Data = 0;
  int Size = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &Data, &Size);
  CHECK(kbts_FontIsValid(&Font), "font failed to load");
  if(Failures) return 1;

  const char *Text = "Hamburgefonstiv";
  kbts_variation Black = {KBTS_FOURCC('w', 'g', 'h', 't'), 900.0f};
  int DefaultWidth = ShapedWidth(&Font, 0, 0, Text);
  int BlackWidth = ShapedWidth(&Font, &Black, 1, Text);
  CHECK(BlackWidth > DefaultWidth, "wght=900 shaped %d units wide, the default %d", BlackWidth, DefaultWidth);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("test_delta_set_index_map_alignment: OK\n");
  return 0;
}
