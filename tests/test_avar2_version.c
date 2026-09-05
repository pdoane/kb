// An avar table of version 2 against kbts_PlaceBlob.
//
// OpenType 1.9 added avar version 2, which appends two Offset32 fields to the
// version 1 table. kbts rejects the whole font when the avar major version is
// anything but 1, so every variable font shipping an avar 2 table loses its
// GSUB and GPOS: the WPT font rvrnTestAvar2[opsz,wdth,wght].ttf shapes with no
// feature variations, so rvrn never swaps the glyph the design point asks for.
// The two tables below differ only in that version field.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KB_TEXT_SHAPE_IMPLEMENTATION
#include "../kb_text_shape.h"

static int Failures;
#define CHECK(cond, ...) do { if(!(cond)) { \
    fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); ++Failures; } } while(0)

static unsigned char *Font;
static unsigned int FontSize;
static unsigned char *Avar;
static unsigned int AvarLength;

static unsigned int ReadU16(const unsigned char *At)
{
  return ((unsigned int)At[0] << 8) | At[1];
}

static unsigned int ReadU32(const unsigned char *At)
{
  return ((unsigned int)At[0] << 24) | ((unsigned int)At[1] << 16) | ((unsigned int)At[2] << 8) | At[3];
}

static void WriteU16(unsigned char *At, unsigned int Value)
{
  At[0] = (unsigned char)(Value >> 8);
  At[1] = (unsigned char)(Value >> 0);
}

static int ReadFont(const char *Path)
{
  FILE *File = fopen(Path, "rb");
  if(!File) return 0;
  fseek(File, 0, SEEK_END);
  long Size = ftell(File);
  fseek(File, 0, SEEK_SET);
  Font = (unsigned char *)malloc((size_t)Size);
  FontSize = (unsigned int)Size;
  int Read = Font && fread(Font, 1, (size_t)Size, File) == (size_t)Size;
  fclose(File);
  if(!Read) return 0;
  unsigned int TableCount = ReadU16(Font + 4);
  for(unsigned int Index = 0; Index < TableCount; ++Index)
  {
    const unsigned char *Record = Font + 12 + 16 * Index;
    if(memcmp(Record, "avar", 4) == 0)
    {
      Avar = Font + ReadU32(Record + 8);
      AvarLength = ReadU32(Record + 12);
      return 1;
    }
  }
  return 0;
}

// Rewrites the font's avar table as the shortest well formed table of
// `Version`: one empty segment map per axis, and for version 2 the two null
// offsets that leave the axis index map and the item variation store out. The
// table record still names the longer length the file was built with, which
// leaves trailing bytes no reader looks at.
static void WriteAvar(unsigned int Version, unsigned int AxisCount)
{
  unsigned char *At = Avar;
  WriteU16(At + 0, Version);
  WriteU16(At + 2, 0);
  WriteU16(At + 4, 0);
  WriteU16(At + 6, AxisCount);
  At += 8;
  for(unsigned int Axis = 0; Axis < AxisCount; ++Axis)
  {
    WriteU16(At, 0);
    At += 2;
  }
  if(Version == 2)
  {
    memset(At, 0, 8);
  }
}

static kbts_load_font_error LoadFont(void)
{
  kbts_font Loaded;
  kbts_load_font_state State;
  memset(&Loaded, 0, sizeof Loaded);
  memset(&State, 0, sizeof State);
  int ScratchSize = 0;
  int OutputSize = 0;
  kbts_load_font_error Error = kbts_LoadFont(&Loaded, &State, Font, (int)FontSize, 0, &ScratchSize, &OutputSize);
  if(Error != KBTS_LOAD_FONT_ERROR_NEED_TO_CREATE_BLOB) return Error;
  void *Scratch = malloc((size_t)ScratchSize);
  void *Output = malloc((size_t)OutputSize);
  Error = kbts_PlaceBlob(&Loaded, &State, Scratch, Output);
  free(Scratch);
  if(Error == KBTS_LOAD_FONT_ERROR_NONE) kbts_FreeFont(&Loaded);
  else free(Output);
  return Error;
}

int main(int argc, char **argv)
{
  if(argc < 2)
  {
    fprintf(stderr, "usage: %s <variable-font-with-avar.ttf>\n", argv[0]);
    return 2;
  }
  if(!ReadFont(argv[1]))
  {
    fprintf(stderr, "%s holds no avar table\n", argv[1]);
    return 2;
  }
  unsigned int AxisCount = ReadU16(Avar + 6);
  CHECK(AvarLength >= 16 + 2 * AxisCount, "avar table too short to rewrite");

  WriteAvar(1, AxisCount);
  CHECK(LoadFont() == KBTS_LOAD_FONT_ERROR_NONE, "an avar 1 table left the font unusable");

  WriteAvar(2, AxisCount);
  CHECK(LoadFont() == KBTS_LOAD_FONT_ERROR_NONE, "an avar 2 table left the font unusable");

  free(Font);
  printf(Failures ? "FAILED\n" : "ok\n");
  return Failures != 0;
}
