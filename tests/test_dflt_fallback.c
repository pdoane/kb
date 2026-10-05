// Language system selection: which script table a shape config reads.
//
// The lookup order is the requested script, then the default script (DFLT or
// dflt), then latn. A font with none of these has no language system for the
// request, and applies no GSUB features to it. Taking the first script in the
// list as a last fallback answers a request for one script with another
// script's language system: an Arabic GSUB applied to Greek text.
//
// The GSUBs here are synthetic, so the script list under test is written out in
// this file.
#include <stdio.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"
#include "synthetic_font.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

// The language system a correct lookup lands on, found by walking the same
// tables the config walks.
static kbts__langsys *LangSysOf(kbts_font *Font, const char *ScriptTag, kbts_language Language)
{
  kbts__gsub_gpos *Header = kbts__BlobTableDataType(Font->Blob, KBTS_BLOB_TABLE_ID_GSUB, kbts__gsub_gpos);
  kbts__script_list *ScriptList = KBTS__POINTER_OFFSET(kbts__script_list, Header, Header->ScriptListOffset);
  kbts_u32 WantedTag = KBTS_FOURCC(ScriptTag[0], ScriptTag[1], ScriptTag[2], ScriptTag[3]);

  KBTS__FOR(ScriptIndex, 0, ScriptList->Count)
  {
    kbts__script_pointer ThisScript = kbts__GetScript(ScriptList, ScriptIndex);
    if(ThisScript.Tag != WantedTag) continue;

    kbts__langsys *Result = kbts__GetDefaultLangsys(ThisScript.Script);
    KBTS__FOR(LangSysIndex, 0, ThisScript.Script->Count)
    {
      kbts__langsys_pointer LangSys = kbts__GetLangsys(ThisScript.Script, LangSysIndex);
      if(LangSys.Tag == Language) Result = LangSys.Langsys;
    }
    return Result;
  }
  return 0;
}

static kbts__langsys *ConfigLangSys(kbts_font *Font, kbts_script Script, kbts_language Language)
{
  kbts__langsys *Result = 0;
  kbts_shape_config *Config = kbts_CreateShapeConfig(Font, Script, Language, 0, 0);
  CHECK(Config != 0, "kbts_CreateShapeConfig failed");
  if(Config)
  {
    Result = Config->Langsys[KBTS_SHAPING_TABLE_GSUB];
    kbts_DestroyShapeConfig(Config);
  }
  return Result;
}

static void CheckLangSys(const char *Name, kbts_font *Font, kbts_script Script, kbts_language Language,
                         const char *ExpectedScriptTag, kbts_language ExpectedLanguage)
{
  kbts__langsys *Expected = LangSysOf(Font, ExpectedScriptTag, ExpectedLanguage);
  CHECK(Expected != 0, "%s: the test font has no %s language system", Name, ExpectedScriptTag);
  kbts__langsys *Got = ConfigLangSys(Font, Script, Language);
  CHECK(Got == Expected, "%s: config took language system %p, expected %s's %p", Name,
        (void *)Got, ExpectedScriptTag, (void *)Expected);
}

int main(void)
{
  // A font with DFLT, and a Latin script whose language systems differ.
  static const synthetic_script WithDefault[] =
  {
    { "DFLT", 0, 1, 0, 0, 0 },
    { "arab", 1, 1, 0, 0, 0 },
    { "latn", 0, 4, "TRK ", 0, 2 },
  };
  // The same font, without a DFLT script.
  static const synthetic_script WithoutDefault[] =
  {
    { "arab", 1, 1, 0, 0, 0 },
    { "latn", 0, 4, "TRK ", 0, 2 },
  };
  // A font with neither DFLT nor latn.
  static const synthetic_script WithoutFallback[] =
  {
    { "arab", 1, 1, 0, 0, 0 },
    { "cyrl", 0, 4, 0, 0, 0 },
  };

  {
    SyntheticBuildGsub(WithDefault, 3, 4);
    unsigned int FontSize = 0;
    unsigned char *FontData = SyntheticBuildFont(&FontSize);
    void *Blob = 0;
    kbts_font Font = KBTS__ZERO;
    CHECK(SyntheticLoadFont(&Font, FontData, FontSize, &Blob) == KBTS_LOAD_FONT_ERROR_NONE, "the font failed to load");

    if(kbts_FontIsValid(&Font))
    {
      CheckLangSys("requested script", &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_DONT_KNOW,
                   "latn", KBTS_LANGUAGE_DONT_KNOW);
      CheckLangSys("requested language system", &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_TURKISH,
                   "latn", KBTS_LANGUAGE_TURKISH);
      CheckLangSys("language the script does not list", &Font, KBTS_SCRIPT_LATIN, KBTS_LANGUAGE_ARABIC,
                   "latn", KBTS_LANGUAGE_DONT_KNOW);
      CheckLangSys("script the font does not list", &Font, KBTS_SCRIPT_GREEK, KBTS_LANGUAGE_DONT_KNOW,
                   "DFLT", KBTS_LANGUAGE_DONT_KNOW);
    }

    kbts_FreeFont(&Font);
    free(Blob);
    free(FontData);
  }

  {
    SyntheticBuildGsub(WithoutDefault, 2, 4);
    unsigned int FontSize = 0;
    unsigned char *FontData = SyntheticBuildFont(&FontSize);
    void *Blob = 0;
    kbts_font Font = KBTS__ZERO;
    CHECK(SyntheticLoadFont(&Font, FontData, FontSize, &Blob) == KBTS_LOAD_FONT_ERROR_NONE, "the font failed to load");

    if(kbts_FontIsValid(&Font))
    {
      // Neither the requested script nor DFLT is present, so latn answers.
      CheckLangSys("no DFLT", &Font, KBTS_SCRIPT_GREEK, KBTS_LANGUAGE_DONT_KNOW,
                   "latn", KBTS_LANGUAGE_DONT_KNOW);
    }

    kbts_FreeFont(&Font);
    free(Blob);
    free(FontData);
  }

  {
    SyntheticBuildGsub(WithoutFallback, 2, 4);
    unsigned int FontSize = 0;
    unsigned char *FontData = SyntheticBuildFont(&FontSize);
    void *Blob = 0;
    kbts_font Font = KBTS__ZERO;
    CHECK(SyntheticLoadFont(&Font, FontData, FontSize, &Blob) == KBTS_LOAD_FONT_ERROR_NONE, "the font failed to load");

    if(kbts_FontIsValid(&Font))
    {
      // Neither the requested script, DFLT nor latn is present. The Arabic
      // script table, first in the list, is not an answer.
      kbts__langsys *Got = ConfigLangSys(&Font, KBTS_SCRIPT_GREEK, KBTS_LANGUAGE_DONT_KNOW);
      CHECK(Got != LangSysOf(&Font, "arab", KBTS_LANGUAGE_DONT_KNOW),
            "a request for Greek took the Arabic language system");
      CHECK(Got == 0, "a request for Greek took language system %p, expected none", (void *)Got);
    }

    kbts_FreeFont(&Font);
    free(Blob);
    free(FontData);
  }

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("test_dflt_fallback: OK\n");
  return 0;
}
