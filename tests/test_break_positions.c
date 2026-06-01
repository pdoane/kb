// Regression test for buffered break positions under non-uniform increments.
//
// kbts_BreakAddCodepoint takes a caller-defined PositionIncrement per
// codepoint (1 for codepoint indices, or e.g. the UTF-8/UTF-16 length for
// code-unit positions). Word breaks buffer their position across ignored
// codepoints (ZWJ/Format), and line breaks do the same across absorbed
// codepoints (CM/ZWJ after a base, repeated SP). The buggy code stepped these
// buffered positions by the *current* codepoint's increment, but they lag one
// or two codepoints behind, so they have to be stepped by the increments of
// the codepoints they lag by. With uniform increments the two are equal and
// the bug is invisible; with code-unit increments the reported break
// positions drift.
//
// The test breaks each string twice: once with increment 1 per codepoint
// (positions are codepoint indices), and once with a non-uniform increment
// scheme. Mapping the reference positions through the increments' prefix sums
// must reproduce the second run's positions and flags exactly.
#include <stdio.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

#define MAX_CODEPOINTS 64
#define MAX_POSITION 1024

typedef struct test_string
{
  const char *Name;
  kbts_u32 Codepoints[MAX_CODEPOINTS];
} test_string;

// 0x200D ZWJ (word-ignored, line-absorbed), 0x2060 word joiner (Format),
// 0x00AD soft hyphen, 0x0301 combining acute (line-absorbed CM).
static const test_string TestStrings[] =
{
  {"zwj between letters",     {'a', 0x200D, 'b', ' ', 'c', 0}},
  {"consecutive zwj",         {'a', 0x200D, 0x200D, 'b', ' ', 'c', 0}},
  {"combining marks",         {'e', 0x0301, 't', 'u', 'd', 'e', ' ', 'r', 'e', 0x0301, 's', 'u', 'm', 'e', 0x0301, 0}},
  {"space runs",              {'a', ' ', ' ', 'b', ' ', ' ', ' ', 'c', 0}},
  {"word joiner",             {'n', 'o', 'w', 0x2060, 'o', 'k', ' ', 'x', 0}},
  {"soft hyphen",             {'h', 'y', 0x00AD, 'p', 'h', 'e', 'n', ' ', 'x', 0}},
  {"mixed",                   {'w', 'e', 0x0301, 'l', 0x200D, 'l', ' ', ' ', 'g', 'o', 0x2060, ' ', 'n', 0x0301, 0x0301, 'o', 0}},
};

static kbts_un StringLength(const kbts_u32 *Codepoints)
{
  kbts_un Result = 0;
  while(Codepoints[Result]) ++Result;
  return Result;
}

static kbts_un Utf8Length(kbts_u32 Codepoint)
{
  return (Codepoint < 0x80) ? 1 : (Codepoint < 0x800) ? 2 : (Codepoint < 0x10000) ? 3 : 4;
}

static kbts_un Utf16Length(kbts_u32 Codepoint)
{
  return (Codepoint < 0x10000) ? 1 : 2;
}

// Break the string with the given per-codepoint increments and OR each
// break's flags into FlagsByPosition. Returns the total advance.
static kbts_un CollectBreaks(const kbts_u32 *Codepoints, kbts_un Count, const kbts_un *Increments, kbts_u32 *FlagsByPosition)
{
  memset(FlagsByPosition, 0, sizeof(kbts_u32) * MAX_POSITION);

  kbts_break_state State;
  kbts_BreakBegin(&State, KBTS_DIRECTION_LTR, KBTS_JAPANESE_LINE_BREAK_STYLE_NORMAL, 0);

  kbts_un TotalAdvance = 0;
  KBTS__FOR(Index, 0, Count)
  {
    kbts_BreakAddCodepoint(&State, (int)Codepoints[Index], (int)Increments[Index], Index + 1 == Count);
    TotalAdvance += Increments[Index];

    kbts_break Break;
    while(kbts_Break(&State, &Break))
    {
      CHECK((Break.Position >= 0) && (Break.Position < MAX_POSITION), "break position %d out of range", Break.Position);
      if((Break.Position >= 0) && (Break.Position < MAX_POSITION))
      {
        FlagsByPosition[Break.Position] |= Break.Flags;
      }
    }
  }

  return TotalAdvance;
}

static void TestIncrementScheme(const test_string *String, const char *SchemeName, const kbts_un *Increments)
{
  kbts_un Count = StringLength(String->Codepoints);

  kbts_un Uniform[MAX_CODEPOINTS];
  KBTS__FOR(Index, 0, Count) Uniform[Index] = 1;

  kbts_u32 ReferenceFlags[MAX_POSITION];
  kbts_u32 TestFlags[MAX_POSITION];
  CollectBreaks(String->Codepoints, Count, Uniform, ReferenceFlags);
  kbts_un TotalAdvance = CollectBreaks(String->Codepoints, Count, Increments, TestFlags);

  // Prefix sums map codepoint index I to its position in the test run's units.
  kbts_un PositionOfIndex[MAX_CODEPOINTS + 1];
  kbts_un Position = 0;
  KBTS__FOR(Index, 0, Count)
  {
    PositionOfIndex[Index] = Position;
    Position += Increments[Index];
  }
  PositionOfIndex[Count] = Position;

  // Every reference break must appear at the mapped position with the same flags.
  KBTS__FOR(Index, 0, Count + 1)
  {
    kbts_u32 Expected = ReferenceFlags[Index];
    kbts_u32 Got = TestFlags[PositionOfIndex[Index]];
    CHECK(Expected == Got, "%s/%s: codepoint %u (position %u): flags 0x%X, expected 0x%X",
          String->Name, SchemeName, (kbts_u32)Index, (kbts_u32)PositionOfIndex[Index], Got, Expected);
  }

  // And no breaks may appear anywhere else (mid-codepoint or out of bounds).
  KBTS__FOR(CheckPosition, 0, MAX_POSITION)
  {
    int Mapped = 0;
    KBTS__FOR(Index, 0, Count + 1)
    {
      if(PositionOfIndex[Index] == CheckPosition) { Mapped = 1; break; }
    }
    if(!Mapped)
    {
      CHECK(TestFlags[CheckPosition] == 0, "%s/%s: unexpected flags 0x%X at unmapped position %u (total advance %u)",
            String->Name, SchemeName, TestFlags[CheckPosition], (kbts_u32)CheckPosition, (kbts_u32)TotalAdvance);
    }
  }
}

int main(void)
{
  KBTS__FOR(StringIndex, 0, KBTS__ARRAY_LENGTH(TestStrings))
  {
    const test_string *String = &TestStrings[StringIndex];
    kbts_un Count = StringLength(String->Codepoints);
    kbts_un Increments[MAX_CODEPOINTS];

    KBTS__FOR(Index, 0, Count) Increments[Index] = Utf8Length(String->Codepoints[Index]);
    TestIncrementScheme(String, "utf8", Increments);

    KBTS__FOR(Index, 0, Count) Increments[Index] = Utf16Length(String->Codepoints[Index]);
    TestIncrementScheme(String, "utf16", Increments);

    // Wildly non-uniform increments to stress the lag correction.
    KBTS__FOR(Index, 0, Count) Increments[Index] = 1 + (Index % 5) * 7;
    TestIncrementScheme(String, "synthetic", Increments);
  }

  if(!Failures) printf("OK\n");
  return Failures;
}
