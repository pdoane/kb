// Regression test for the shape config cache key.
//
// The context caches one shape config per font, variation, script and
// language. Each cache entry kept a pointer to the font's interned info, which
// lives in the ScratchArena that kbts_ShapeBegin clears. On a later
// ShapeBegin, the next font interned lands at the same address, so the old
// entry reads that font's hash as its own and the lookup returns the old
// font's config for the new font.
//
// Shapes with font A, then, in a new ShapeBegin on the same context, with
// font B on top of the stack. B's glyphs must match B shaped in a fresh
// context.
//
// usage: test_config_cache_hash <fontA.ttf> <fontB.ttf>
#include <stdio.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

#define MAX_SHAPED_GLYPHS 32

typedef struct shaped
{
  int Count;
  kbts_u32 Ids[MAX_SHAPED_GLYPHS];
  int Advances[MAX_SHAPED_GLYPHS];
} shaped;

static void Shape(kbts_shape_context *Context, const char *Text, shaped *Out)
{
  kbts_ShapeBegin(Context, KBTS_DIRECTION_DONT_KNOW, KBTS_LANGUAGE_DONT_KNOW);
  kbts_ShapeUtf8(Context, Text, (int)strlen(Text), KBTS_USER_ID_GENERATION_MODE_CODEPOINT_INDEX);
  kbts_ShapeEnd(Context);

  Out->Count = 0;
  kbts_run Run;
  while(kbts_ShapeRun(Context, &Run))
  {
    kbts_glyph *Glyph;
    while(kbts_GlyphIteratorNext(&Run.Glyphs, &Glyph))
    {
      if(Out->Count < MAX_SHAPED_GLYPHS)
      {
        Out->Ids[Out->Count] = Glyph->Id;
        Out->Advances[Out->Count] = Glyph->AdvanceX;
      }
      Out->Count++;
    }
  }
}

// The index of the first glyph that differs, or -1 when the shapes match.
static int FirstDifference(shaped *A, shaped *B)
{
  for(int Index = 0; (Index < A->Count) && (Index < B->Count) && (Index < MAX_SHAPED_GLYPHS); ++Index)
  {
    if((A->Ids[Index] != B->Ids[Index]) || (A->Advances[Index] != B->Advances[Index])) return Index;
  }
  return (A->Count == B->Count) ? -1 : 0;
}

int main(int argc, char **argv)
{
  if(argc < 3) { fprintf(stderr, "usage: %s <fontA.ttf> <fontB.ttf>\n", argv[0]); return 2; }

  void *DataA = 0, *DataB = 0;
  int SizeA = 0, SizeB = 0;
  kbts_font FontA = kbts_FontFromFile(argv[1], 0, 0, 0, &DataA, &SizeA);
  kbts_font FontB = kbts_FontFromFile(argv[2], 0, 0, 0, &DataB, &SizeB);
  CHECK(kbts_FontIsValid(&FontA) && kbts_FontIsValid(&FontB), "fonts failed to load");
  if(Failures) return 1;

  const char *Text = "office affinity";

  shaped Expected;
  {
    kbts_shape_context *Fresh = kbts_CreateShapeContext(0, 0);
    kbts_ShapePushFont(Fresh, &FontB);
    Shape(Fresh, Text, &Expected);
    kbts_DestroyShapeContext(Fresh);
  }

  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  shaped First, Second;
  kbts_ShapePushFont(Context, &FontA);
  Shape(Context, Text, &First);
  Shape(Context, Text, &First);
  kbts_ShapePushFont(Context, &FontB);
  Shape(Context, Text, &Second);

  CHECK(kbts_ShapeError(Context) == KBTS_SHAPE_ERROR_NONE, "shaping set error %u", kbts_ShapeError(Context));
  int Difference = FirstDifference(&Second, &Expected);
  CHECK(Difference < 0,
        "font B after font A differs from a fresh context at glyph %d: id %u advance %d, expected id %u advance %d",
        Difference, Second.Ids[Difference], Second.Advances[Difference], Expected.Ids[Difference], Expected.Advances[Difference]);

  kbts_DestroyShapeContext(Context);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("test_config_cache_hash: OK\n");
  return 0;
}
