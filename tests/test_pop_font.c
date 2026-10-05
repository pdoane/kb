// Regression test for kbts_ShapePopFont.
//
// kbts_ShapePopFont read the top font's block from the address of the font
// stack sentinel's Prev field instead of from the block Prev points to, so it
// ended the lifetime of whatever lay there. Popping the first font of a block
// also unlinked that bogus block from the stack.
//
// Pushes and pops a font around a shape, repeatedly and across a block
// boundary, and checks every shape matches the first.
//
// usage: test_pop_font <font.ttf>
#include <stdio.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

static int ShapeGlyphCount(kbts_shape_context *Context, const char *Text)
{
  kbts_ShapeBegin(Context, KBTS_DIRECTION_DONT_KNOW, KBTS_LANGUAGE_DONT_KNOW);
  kbts_ShapeUtf8(Context, Text, (int)strlen(Text), KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
  kbts_ShapeEnd(Context);

  int Result = 0;
  kbts_run Run;
  while(kbts_ShapeRun(Context, &Run))
  {
    kbts_glyph *Glyph;
    while(kbts_GlyphIteratorNext(&Run.Glyphs, &Glyph)) ++Result;
  }
  return Result;
}

int main(int argc, char **argv)
{
  if(argc < 2) { fprintf(stderr, "usage: %s <font.ttf>\n", argv[0]); return 2; }

  void *Data = 0;
  int Size = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &Data, &Size);
  CHECK(kbts_FontIsValid(&Font), "font failed to load");
  if(Failures) return 1;

  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);

  // One font pushed and popped around every shape.
  int Expected = -1;
  for(int Iteration = 0; Iteration < 4; ++Iteration)
  {
    kbts_ShapePushFont(Context, &Font);
    int Count = ShapeGlyphCount(Context, "office");
    if(Expected < 0) Expected = Count;
    CHECK(Count == Expected, "iteration %d shaped %d glyphs, expected %d", Iteration, Count, Expected);
    CHECK(kbts_ShapePopFont(Context) == &Font, "iteration %d popped the wrong font", Iteration);
  }

  // A stack one block deep plus one, popped back to a single font.
  int Depth = KBTS_CONTEXT_FONTS_PER_BLOCK + 1;
  for(int Index = 0; Index < Depth; ++Index) kbts_ShapePushFont(Context, &Font);
  for(int Index = 1; Index < Depth; ++Index)
  {
    CHECK(kbts_ShapePopFont(Context) == &Font, "pop %d returned the wrong font", Index);
  }
  CHECK(ShapeGlyphCount(Context, "office") == Expected, "shape after popping a block changed");

  CHECK(kbts_ShapeError(Context) == KBTS_SHAPE_ERROR_NONE, "shaping set error %u", kbts_ShapeError(Context));
  kbts_DestroyShapeContext(Context);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("test_pop_font: OK\n");
  return 0;
}
