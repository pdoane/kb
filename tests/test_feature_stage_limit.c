// Feature baking: how many of a language system's features a shape config
// holds.
//
// The user feature stage bakes every feature the language system lists, into
// an array of KBTS_MAX_FEATURES_PER_STAGE entries. When that array is sized by
// KBTS_MAX_SIMULTANEOUS_FEATURES (32), a language system listing more features
// loses the ones past the limit: their lookups never enter the config, so no
// kbts_feature_override can turn them on. Fonts in the wild list more than 32:
// FontWithFancyFeatures.otf from the CSS fonts test suite lists 50, and macOS's
// SFNS.ttf lists 38 under cyrl/BGR, the last six being ss06 ss07 ss09 subs sups
// tnum.
//
// The GSUB here is synthetic: one language system listing 43 features.
#include <stdio.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"
#include "synthetic_font.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

int main(void)
{
  SyntheticBuildLatinGsub(SYNTHETIC_FEATURE_COUNT);
  unsigned int FontSize = 0;
  unsigned char *FontData = SyntheticBuildFont(&FontSize);
  void *Blob = 0;
  kbts_font Font = KBTS__ZERO;
  kbts_load_font_error Error = SyntheticLoadFont(&Font, FontData, FontSize, &Blob);
  CHECK(Error == KBTS_LOAD_FONT_ERROR_NONE && kbts_FontIsValid(&Font), "the synthetic font failed to load (%u)", Error);

  if(kbts_FontIsValid(&Font))
  {
    kbts_shape_config *Config = kbts_CreateShapeConfig(&Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW, 0, 0);
    CHECK(Config != 0, "kbts_CreateShapeConfig failed");
    if(Config)
    {
      for(int FeatureIndex = 0; FeatureIndex < SYNTHETIC_FEATURE_COUNT; ++FeatureIndex)
      {
        CHECK(SyntheticOverrideEnablesLookup(Config, SyntheticFeatureTags[FeatureIndex]),
              "an override on listed feature %d of %d (%s) enabled no lookup: the config dropped it",
              FeatureIndex + 1, SYNTHETIC_FEATURE_COUNT, SyntheticFeatureTags[FeatureIndex]);
      }
      kbts_DestroyShapeConfig(Config);
    }
  }

  kbts_FreeFont(&Font);
  free(Blob);
  free(FontData);

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("test_feature_stage_limit: OK\n");
  return 0;
}
