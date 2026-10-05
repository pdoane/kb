// Regression test for GSUB/GPOS tables missing one of their lists.
//
// A GSUB or GPOS header holds offsets to its script, feature and lookup lists.
// A table with all three offsets zero loads as empty. A table with only some
// of them zero was traversed, and the byteswap followed each zero offset back
// to the table header, read the already-swapped version field as a record
// count of 256, and wrote past the end of the blob allocation. A table missing
// any one list can apply nothing, so it loads as empty too.
//
// Each table below has one zero offset; the others point at empty lists. The
// blob is placed into an exactly-sized allocation followed by a guard
// pattern, so the overrun shows up as a modified guard even without a
// sanitizer, and as a heap overflow under -fsanitize=address.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

#define GUARD_SIZE 64

// The overrunning writes are in-place byteswaps, so the guard pattern has to
// differ between adjacent bytes for a swapped pair to read back as modified.
#define GUARD_BYTE(Index) (unsigned char)((Index) * 7 + 1)

typedef struct table_record
{
  const char *Tag;
  const unsigned char *Data;
  unsigned int Length;
} table_record;

// A 1.0 header followed by two empty lists, at offsets 10 and 12. Each table
// leaves out one list by giving it a zero offset.
#define EMPTY_LISTS 0x00, 0x00, 0x00, 0x00 // two lists, each with a count of 0

static const unsigned char NoScriptList[] =
{
  0x00, 0x01, 0x00, 0x00, // Version 1.0
  0x00, 0x00,             // ScriptListOffset
  0x00, 0x0A,             // FeatureListOffset
  0x00, 0x0C,             // LookupListOffset
  EMPTY_LISTS,
};

static const unsigned char NoFeatureList[] =
{
  0x00, 0x01, 0x00, 0x00, // Version 1.0
  0x00, 0x0A,             // ScriptListOffset
  0x00, 0x00,             // FeatureListOffset
  0x00, 0x0C,             // LookupListOffset
  EMPTY_LISTS,
};

static const unsigned char NoLookupList[] =
{
  0x00, 0x01, 0x00, 0x00, // Version 1.0
  0x00, 0x0A,             // ScriptListOffset
  0x00, 0x0C,             // FeatureListOffset
  0x00, 0x00,             // LookupListOffset
  EMPTY_LISTS,
};

static void WriteU16(unsigned char *At, unsigned int Value)
{
  At[0] = (unsigned char)(Value >> 8);
  At[1] = (unsigned char)(Value >> 0);
}

static void WriteU32(unsigned char *At, unsigned int Value)
{
  At[0] = (unsigned char)(Value >> 24);
  At[1] = (unsigned char)(Value >> 16);
  At[2] = (unsigned char)(Value >> 8);
  At[3] = (unsigned char)(Value >> 0);
}

// Assembles an sfnt with a 0x00010000 version and one directory entry per
// record. Table data is 4-aligned, in the order given.
static unsigned char *BuildFont(const table_record *Records, unsigned int RecordCount, unsigned int *SizeOut)
{
  unsigned int DirectorySize = 12 + 16 * RecordCount;
  unsigned int Size = DirectorySize;
  unsigned int RecordIndex;

  for(RecordIndex = 0; RecordIndex < RecordCount; ++RecordIndex)
  {
    Size = (Size + 3) & ~3u;
    Size += Records[RecordIndex].Length;
  }

  unsigned char *Font = (unsigned char *)calloc(1, Size);
  unsigned int At = DirectorySize;

  WriteU32(Font + 0, 0x00010000);
  WriteU16(Font + 4, RecordCount);

  for(RecordIndex = 0; RecordIndex < RecordCount; ++RecordIndex)
  {
    const table_record *Record = &Records[RecordIndex];
    unsigned char *Entry = Font + 12 + 16 * RecordIndex;

    At = (At + 3) & ~3u;

    memcpy(Entry, Record->Tag, 4);
    WriteU32(Entry + 8, At);
    WriteU32(Entry + 12, Record->Length);

    memcpy(Font + At, Record->Data, Record->Length);
    At += Record->Length;
  }

  *SizeOut = Size;
  return Font;
}

static void FillGuard(unsigned char *Guard)
{
  int Index;
  for(Index = 0; Index < GUARD_SIZE; ++Index) Guard[Index] = GUARD_BYTE(Index);
}

static int GuardIsIntact(const unsigned char *Guard)
{
  int Index;
  for(Index = 0; Index < GUARD_SIZE; ++Index)
  {
    if(Guard[Index] != GUARD_BYTE(Index)) return 0;
  }
  return 1;
}

static void CheckFont(const char *Name, const table_record *Records, unsigned int RecordCount)
{
  unsigned int FontSize = 0;
  unsigned char *FontData = BuildFont(Records, RecordCount, &FontSize);

  kbts_font Font = KBTS__ZERO;
  kbts_load_font_state State = KBTS__ZERO;
  int ScratchSize = 0;
  int OutputSize = 0;

  kbts_load_font_error Error = kbts_LoadFont(&Font, &State, FontData, (int)FontSize, 0, &ScratchSize, &OutputSize);
  CHECK(Error == KBTS_LOAD_FONT_ERROR_NEED_TO_CREATE_BLOB, "%s: kbts_LoadFont returned %u", Name, Error);

  if(Error == KBTS_LOAD_FONT_ERROR_NEED_TO_CREATE_BLOB)
  {
    unsigned char *Scratch = (unsigned char *)malloc((size_t)ScratchSize + GUARD_SIZE);
    unsigned char *Output = (unsigned char *)malloc((size_t)OutputSize + GUARD_SIZE);

    FillGuard(Scratch + ScratchSize);
    FillGuard(Output + OutputSize);

    kbts_PlaceBlob(&Font, &State, Scratch, Output);

    CHECK(GuardIsIntact(Output + OutputSize), "%s: kbts_PlaceBlob wrote past the end of the blob", Name);
    CHECK(GuardIsIntact(Scratch + ScratchSize), "%s: kbts_PlaceBlob wrote past the end of the scratch memory", Name);
    CHECK(kbts_FontIsValid(&Font), "%s: font did not load (error=%u)", Name, Font.Error);

    free(Output);
    free(Scratch);
  }

  free(FontData);
}

static void CheckTable(const char *Name, const unsigned char *Table, unsigned int Length)
{
  char Label[64];
  table_record Gsub[] = {{"GSUB", Table, Length}};
  table_record Gpos[] = {{"GPOS", Table, Length}};
  table_record Both[] = {{"GSUB", Table, Length}, {"GPOS", Table, Length}};

  snprintf(Label, sizeof(Label), "GSUB, %s", Name);
  CheckFont(Label, Gsub, 1);
  snprintf(Label, sizeof(Label), "GPOS, %s", Name);
  CheckFont(Label, Gpos, 1);
  snprintf(Label, sizeof(Label), "GSUB+GPOS, %s", Name);
  CheckFont(Label, Both, 2);
}

int main(void)
{
  CheckTable("no script list", NoScriptList, sizeof(NoScriptList));
  CheckTable("no feature list", NoFeatureList, sizeof(NoFeatureList));
  CheckTable("no lookup list", NoLookupList, sizeof(NoLookupList));

  if(Failures) { fprintf(stderr, "%d FAILURE(S)\n", Failures); return 1; }
  printf("test_gsub_gpos_bounds: OK\n");
  return 0;
}
