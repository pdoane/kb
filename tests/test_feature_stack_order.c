// Feature stack order: the context applies the latest push of a tag, as the
// header documents at kbts_ShapePushFeature.
//
// The unique-override scan walks the stack from the top down so the latest push
// lands first. When the hoisted copy is taken from the raw stack instead of
// from that scan, the earliest push of a tag wins.
//
// The feature pushed is the first of a few unfiltered features that changes
// how the font shapes the text, so the two stack orders shape differently and
// each says which push won.
//
// usage: test_feature_stack_order <font.ttf>   (NotoSans[wdth,wght].ttf)
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

  const char *Text = "12ab";
  static const kbts_u32 Candidates[] =
  {
    KBTS_FEATURE_TAG_sups, KBTS_FEATURE_TAG_subs, KBTS_FEATURE_TAG_onum,
    KBTS_FEATURE_TAG_smcp, KBTS_FEATURE_TAG_c2sc, KBTS_FEATURE_TAG_ordn,
  };

  shaped Plain = Shape(&Font, Text, 0);
  kbts_u32 Feature = 0;
  shaped On = Plain;
  for(int Index = 0; Index < (int)(sizeof(Candidates) / sizeof(*Candidates)); ++Index)
  {
    On = Shape(&Font, Text, Candidates[Index]);
    if(!SameShape(&On, &Plain))
    {
      Feature = Candidates[Index];
      break;
    }
  }
  CHECK(Feature != 0, "none of the candidate features changes how the font shapes \"%s\"", Text);

  if(Feature)
  {
    printf("pushing %c%c%c%c\n", (char)Feature, (char)(Feature >> 8), (char)(Feature >> 16), (char)(Feature >> 24));

    static const int OnThenOff[2] = { 1, 0 };
    static const int OffThenOn[2] = { 0, 1 };
    shaped LastOff = ShapeWithStack(&Font, Text, Feature, OnThenOff, 2);
    shaped LastOn = ShapeWithStack(&Font, Text, Feature, OffThenOn, 2);

    PrintShape("plain", &Plain);
    PrintShape("on", &On);
    PrintShape("on then off", &LastOff);
    PrintShape("off then on", &LastOn);

    CHECK(SameShape(&LastOff, &Plain),
          "the feature pushed on and then off shaped as if it were on: the stack applied the "
          "first push of the tag, not the latest");
    CHECK(SameShape(&LastOn, &On),
          "the feature pushed off and then on shaped as if it were off: the stack applied the "
          "first push of the tag, not the latest");
  }

  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("test_feature_stack_order: OK\n");
  return 0;
}
