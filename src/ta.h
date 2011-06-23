/* ta.h */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#ifndef __TA_H__
#define __TA_H__

#include <config.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H

#include "taloader.h"
#include "tadummy.h"
#include "talatin.h"
#include "ttfautohint.h"


/* these macros convert 16bit and 32bit numbers into single bytes */
/* using the byte order needed within SFNT files */

#define HIGH(x) (FT_Byte)(((x) & 0xFF00) >> 8)
#define LOW(x) ((x) & 0x00FF)

#define BYTE1(x) (FT_Byte)(((x) & 0xFF000000UL) >> 24);
#define BYTE2(x) (FT_Byte)(((x) & 0x00FF0000UL) >> 16);
#define BYTE3(x) (FT_Byte)(((x) & 0x0000FF00UL) >> 8);
#define BYTE4(x) ((x) & 0x000000FFUL);


/* the length of a dummy `DSIG' table */
#define DSIG_LEN 8

/* the length of our `gasp' table */
#define GASP_LEN 8

/* an empty slot in the table info array */
#define MISSING (FT_ULong)~0

/* the offset to the loca table format in the `head' table */
#define LOCA_FORMAT_OFFSET 51

/* various offsets within the `maxp' table */
#define MAXP_MAX_ZONES_OFFSET 14
#define MAXP_MAX_TWILIGHT_POINTS_OFFSET 16
#define MAXP_MAX_STORAGE_OFFSET 18
#define MAXP_MAX_FUNCTION_DEFS_OFFSET 20
#define MAXP_MAX_INSTRUCTION_DEFS_OFFSET 22
#define MAXP_MAX_STACK_ELEMENTS_OFFSET 24
#define MAXP_MAX_INSTRUCTIONS_OFFSET 26

#define MAXP_LEN 32


/* flags in composite glyph records */
#define ARGS_ARE_WORDS 0x0001
#define ARGS_ARE_XY_VALUES 0x0002
#define WE_HAVE_A_SCALE 0x0008
#define MORE_COMPONENTS 0x0020
#define WE_HAVE_AN_XY_SCALE 0x0040
#define WE_HAVE_A_2X2 0x0080
#define WE_HAVE_INSTR 0x0100

/* flags in simple glyph records */
#define X_SHORT_VECTOR 0x02
#define Y_SHORT_VECTOR 0x04
#define REPEAT 0x08
#define SAME_X 0x10
#define SAME_Y 0x20


/* a single glyph */
typedef struct GLYPH_
{
  FT_ULong len1; /* number of bytes before instructions location */
  FT_ULong len2; /* number of bytes after instructions location; */
                 /* if zero, this indicates a composite glyph */
  FT_Byte* buf; /* extracted glyph data */
  FT_ULong flags_offset; /* offset to last flag in a composite glyph */

  FT_ULong ins_len; /* number of new instructions */
  FT_Byte* ins_buf; /* new instruction data */
} GLYPH;

/* a representation of the data in the `glyf' table */
typedef struct glyf_Data_
{
  FT_UShort num_glyphs;
  GLYPH* glyphs;
} glyf_Data;

/* an SFNT table */
typedef struct SFNT_Table_ {
  FT_ULong tag;
  FT_ULong len;
  FT_Byte* buf; /* the table data */
  FT_ULong offset; /* from beginning of file */
  FT_ULong checksum;
  void* data; /* used e.g. for `glyf' table data */
  FT_Bool processed;
} SFNT_Table;

/* we use indices into the SFNT table array to */
/* represent table info records of the TTF header */
typedef FT_ULong SFNT_Table_Info;

/* this structure is used to model a TTF or a subfont within a TTC */
typedef struct SFNT_ {
  FT_Face face;

  SFNT_Table_Info* table_infos;
  FT_ULong num_table_infos;

  /* various SFNT table indices */
  FT_ULong glyf_idx;
  FT_ULong loca_idx;
  FT_ULong head_idx;
  FT_ULong maxp_idx;

  /* values necessary to update the `maxp' table */
  FT_UShort max_storage;
  FT_UShort max_stack_elements;
  FT_UShort max_twilight_points;
  FT_UShort max_instructions;
} SFNT;

/* our font object */
typedef struct FONT_ {
  FT_Library lib;

  FT_Byte* in_buf;
  size_t in_len;

  FT_Byte* out_buf;
  size_t out_len;

  SFNT* sfnts;
  FT_Long num_sfnts;

  SFNT_Table* tables;
  FT_ULong num_tables;

  TA_LoaderRec loader[1]; /* the interface to the autohinter */

  /* configuration options */
  TA_Progress_Func progress;
  void *progress_data;
  FT_UInt hinting_range_min;
  FT_UInt hinting_range_max;
  FT_Bool pre_hinting;
  FT_Bool no_x_height_snapping;
  FT_Byte* x_height_snapping_exceptions;
  FT_Bool ignore_permissions;
  FT_Bool latin_fallback;
} FONT;

#include "tatables.h"
#include "tabytecode.h"

#endif /* __TA_H__ */

/* end of ta.h */
