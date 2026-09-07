// Test for the shape-config cache index.
//
// A shape context caches one config per font, script, language and variation,
// and every run of shaping asks the cache for its config. The cache was a list
// of blocks scanned end to end, with a 32-axis variation compare per entry, so
// a run's lookup cost grew with every distinct key the context had ever
// shaped. The cache is now indexed by key hash.
//
// The index has to keep answering the same config for a key across thousands
// of keys, which grows it several times, and drop every entry of a popped
// font. Its cost is not measured here: a timing bound depends on the machine
// and its load, so the lookup count is what the change is judged by.
#include <stdio.h>
#include <stdlib.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

#define LANGUAGE_COUNT 24
#define LOOKUP_COUNT 100000

static kbts_language LanguageAt(int Index)
{
  return (kbts_language)KBTS_FOURCC('L', 'A', (char)('A' + Index / 26), (char)('A' + Index % 26));
}

int main(int argc, char **argv)
{
  if(argc < 2) { fprintf(stderr, "usage: %s <font.ttf>\n", argv[0]); return 2; }

  void *FileData = 0;
  int FileSize = 0;
  kbts_font Font = kbts_FontFromFile(argv[1], 0, 0, 0, &FileData, &FileSize);
  CHECK(kbts_FontIsValid(&Font), "kbts_FontFromFile failed (error=%u)", Font.Error);
  if(!kbts_FontIsValid(&Font)) return Failures;

  kbts_shape_context *Context = kbts_CreateShapeContext(0, 0);
  CHECK(Context != 0, "kbts_CreateShapeContext failed");
  kbts_ShapePushFont(Context, &Font);

  // Every script under every language: thousands of distinct keys, each
  // answered by the same config the second time it is asked for.
  int KeyCount = (int)KBTS_SCRIPT_COUNT * LANGUAGE_COUNT;
  kbts_shape_config **Configs = (kbts_shape_config **)malloc(sizeof(*Configs) * (size_t)KeyCount);
  for(int Key = 0; Key < KeyCount; ++Key)
  {
    kbts_script Script = (kbts_script)(Key % (int)KBTS_SCRIPT_COUNT);
    kbts_language Language = LanguageAt(Key / (int)KBTS_SCRIPT_COUNT);
    Configs[Key] = kbts__FindOrCreateShapeConfig(Context, &Font, Script, Language);
    CHECK(Configs[Key] != 0, "key %d has no config", Key);
  }
  CHECK(Context->Error == KBTS_SHAPE_ERROR_NONE, "creating %d configs set error %u", KeyCount, Context->Error);

  for(int Lookup = 0; Lookup < LOOKUP_COUNT; ++Lookup)
  {
    int Key = (int)(((long long)Lookup * 7919) % KeyCount);
    kbts_script Script = (kbts_script)(Key % (int)KBTS_SCRIPT_COUNT);
    kbts_language Language = LanguageAt(Key / (int)KBTS_SCRIPT_COUNT);
    kbts_shape_config *Found = kbts__FindOrCreateShapeConfig(Context, &Font, Script, Language);
    if(Found != Configs[Key])
    {
      CHECK(0, "key %d answered %p after %p", Key, (void *)Found, (void *)Configs[Key]);
      break;
    }
  }
  CHECK(Context->Error == KBTS_SHAPE_ERROR_NONE, "%d lookups set error %u", LOOKUP_COUNT, Context->Error);

  // A popped font's configs leave the cache; the same keys build new ones.
  kbts_ShapePopFont(Context);
  kbts_ShapePushFont(Context, &Font);
  int Rebuilt = 0;
  for(int Key = 0; Key < KeyCount; Key += 97)
  {
    kbts_script Script = (kbts_script)(Key % (int)KBTS_SCRIPT_COUNT);
    kbts_language Language = LanguageAt(Key / (int)KBTS_SCRIPT_COUNT);
    kbts_shape_config *Found = kbts__FindOrCreateShapeConfig(Context, &Font, Script, Language);
    CHECK(Found != 0, "key %d has no config after the font was popped and pushed", Key);
    CHECK(Found == kbts__FindOrCreateShapeConfig(Context, &Font, Script, Language),
          "key %d answers two configs after the font was popped and pushed", Key);
    ++Rebuilt;
  }
  CHECK(Rebuilt > 0, "no key was asked for again");

  free(Configs);
  kbts_DestroyShapeContext(Context);
  kbts_FreeFont(&Font);
  free(FileData);

  if(Failures) fprintf(stderr, "%d failure(s)\n", Failures);
  else printf("test_config_index: OK\n");
  return Failures ? 1 : 0;
}
