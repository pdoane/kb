// Shaping helpers shared by the tests that shape real text through a context.
// Include after kb_text_shape.h.
#ifndef SHAPING_H
#define SHAPING_H

#include <stdio.h>
#include <string.h>

#define MAX_SHAPED_GLYPHS 16

typedef struct shaped
{
  int Count;
  kbts_u32 Ids[MAX_SHAPED_GLYPHS];
} shaped;

// Shapes [Text] in a fresh context with [PushCount] pushes of [PushedFeature],
// taking each push's value from [PushValues] in order.
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

// Shapes [Text] with [PushedFeature] pushed on, or with nothing pushed when it
// is 0.
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

static void PrintShape(const char *Name, shaped *Shaped)
{
  printf("%s:", Name);
  for(int Index = 0; Index < Shaped->Count && Index < MAX_SHAPED_GLYPHS; ++Index)
  {
    printf(" %u", Shaped->Ids[Index]);
  }
  printf("\n");
}

#endif
