/* ttfautohint.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

/* This file needs FreeType 2.4.5 or newer. */


#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H

#include "ttfautohint.h"


/* these macros convert 16bit and 32bit numbers into single bytes */
/* using the byte order needed within SFNT files                  */

#define HIGH(x) (FT_Byte)(((x) & 0xFF00) >> 8)
#define LOW(x) ((x) & 0x00FF)

#define BYTE1(x) (FT_Byte)(((x) & 0xFF000000UL) >> 24);
#define BYTE2(x) (FT_Byte)(((x) & 0x00FF0000UL) >> 16);
#define BYTE3(x) (FT_Byte)(((x) & 0x0000FF00UL) >> 8);
#define BYTE4(x) ((x) & 0x000000FFUL);


/* the length of a dummy `DSIG' table */
#define DSIG_LEN 8

/* an empty slot in the table info array */
#define MISSING (FT_ULong)~0


/* an SFNT table */
typedef struct SFNT_Table_ {
  FT_ULong tag;
  FT_ULong len;
  FT_Byte* buf;    /* the table data */
  FT_ULong offset; /* from beginning of file */
  FT_ULong checksum;
} SFNT_Table;

/* we use indices into the SFNT table array to */
/* represent table info records in a TTF header */
typedef FT_ULong SFNT_Table_Info;

/* this structure is used to model a TTF or a subfont within a TTC */
typedef struct SFNT_ {
  FT_Face face;
  SFNT_Table_Info* table_infos;
  FT_ULong num_table_infos;
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
} FONT;


static FT_Error
TA_font_read(FONT* font,
             FILE* in)
{
  fseek(in, 0, SEEK_END);
  font->in_len = ftell(in);
  fseek(in, 0, SEEK_SET);

  /* a valid TTF can never be that small */
  if (font->in_len < 100)
    return FT_Err_Invalid_Argument;

  font->in_buf = (FT_Byte*)malloc(font->in_len * sizeof (FT_Byte));
  if (!font->in_buf)
    return FT_Err_Out_Of_Memory;

  if (fread(font->in_buf, 1, font->in_len, in) != font->in_len)
    return FT_Err_Invalid_Stream_Read;

  return TA_Err_Ok;
}


static FT_Error
TA_font_init(FONT* font)
{
  FT_Error error;
  FT_Face f;


  error = FT_Init_FreeType(&font->lib);
  if (error)
    return error;

  /* get number of faces (i.e. subfonts) */
  error = FT_New_Memory_Face(font->lib, font->in_buf, font->in_len, -1, &f);
  if (error)
    return error;
  font->num_sfnts = f->num_faces;
  FT_Done_Face(f);

  /* it is a TTC if we have more than a single subfont */
  font->sfnts = (SFNT*)calloc(1, font->num_sfnts * sizeof (SFNT));
  if (!font->sfnts)
    return FT_Err_Out_Of_Memory;

  return TA_Err_Ok;
}


static FT_Error
TA_sfnt_add_table_info(SFNT* sfnt)
{
  SFNT_Table_Info* table_infos_new;


  sfnt->num_table_infos++;
  table_infos_new =
    (SFNT_Table_Info*)realloc(sfnt->table_infos, sfnt->num_table_infos
                                                 * sizeof (SFNT_Table_Info));
  if (!table_infos_new)
  {
    sfnt->num_table_infos--;
    return FT_Err_Out_Of_Memory;
  }
  else
    sfnt->table_infos = table_infos_new;

  sfnt->table_infos[sfnt->num_table_infos - 1] = MISSING;

  return TA_Err_Ok;
}


static FT_ULong
TA_table_compute_checksum(FT_Byte* buf,
                          FT_ULong len)
{
  FT_Byte* end_buf = buf + len;
  FT_ULong checksum = 0;


  while (buf < end_buf)
  {
    checksum += *(buf++) << 24;
    checksum += *(buf++) << 16;
    checksum += *(buf++) << 8;
    checksum += *(buf++);
  }

  return checksum;
}


static FT_Error
TA_font_add_table(FONT* font,
                  SFNT_Table_Info* table_info,
                  FT_ULong tag,
                  FT_ULong len,
                  FT_Byte* buf)
{
  SFNT_Table* tables_new;
  SFNT_Table* table_last;


  font->num_tables++;
  tables_new = (SFNT_Table*)realloc(font->tables,
                                    font->num_tables * sizeof (SFNT_Table));
  if (!tables_new)
  {
    font->num_tables--;
    return FT_Err_Out_Of_Memory;
  }
  else
    font->tables = tables_new;

  table_last = &font->tables[font->num_tables - 1];

  table_last->tag = tag;
  table_last->len = len;
  table_last->buf = buf;
  table_last->checksum = TA_table_compute_checksum(buf, len);

  /* link table and table info */
  *table_info = font->num_tables - 1;

  return TA_Err_Ok;
}


static void
TA_sfnt_sort_table_info(SFNT* sfnt,
                        FONT* font)
{
  /* Looking into an arbitrary TTF (with a `DSIG' table), tags */
  /* starting with an uppercase letter are sorted before lowercase */
  /* letters.  In other words, the alphabetical ordering (as */
  /* mandated by signing a font) is a simple numeric comparison of */
  /* the 32bit tag values. */

  SFNT_Table* tables = font->tables;

  SFNT_Table_Info* table_infos = sfnt->table_infos;
  FT_ULong num_table_infos = sfnt->num_table_infos;

  FT_ULong i;
  FT_ULong j;


  for (i = 1; i < num_table_infos; i++)
  {
    for (j = i; j > 0; j--)
    {
      SFNT_Table_Info swap;
      FT_ULong tag1;
      FT_ULong tag2;


      tag1 = (table_infos[j] == MISSING)
               ? 0
               : tables[table_infos[j]].tag;
      tag2 = (table_infos[j - 1] == MISSING)
               ? 0
               : tables[table_infos[j - 1]].tag;

      if (tag1 > tag2)
        break;

      swap = table_infos[j];
      table_infos[j] = table_infos[j - 1];
      table_infos[j - 1] = swap;
    }
  }
}


static FT_Error
TA_sfnt_split_into_SFNT_tables(SFNT* sfnt,
                               FONT* font)
{
  FT_Error error;
  FT_ULong glyf_idx;
  FT_ULong i;


  /* basic check whether font is a TTF or TTC */
  if (!FT_IS_SFNT(sfnt->face))
    return FT_Err_Invalid_Argument;

  error = FT_Sfnt_Table_Info(sfnt->face, 0, NULL, &sfnt->num_table_infos);
  if (error)
    return error;

  sfnt->table_infos = (SFNT_Table_Info*)malloc(sfnt->num_table_infos
                                               * sizeof (SFNT_Table_Info));
  if (!sfnt->table_infos)
    return FT_Err_Out_Of_Memory;

  /* collect SFNT tables and search for `glyf' table */
  glyf_idx = MISSING;
  for (i = 0; i < sfnt->num_table_infos; i++)
  {
    SFNT_Table_Info* table_info = &sfnt->table_infos[i];
    FT_ULong tag;
    FT_ULong len;
    FT_Byte* buf;

    FT_ULong buf_len;
    FT_ULong j;


    *table_info = MISSING;

    error = FT_Sfnt_Table_Info(sfnt->face, i, &tag, &len);
    if (error)
    {
      /* this ignores both missing and zero-length tables */
      if (error == FT_Err_Table_Missing)
        continue;
      else
        return error;
    }

    if (tag == TTAG_glyf)
      glyf_idx = i;

    /* ignore tables which we are going to create by ourselves */
    else if (tag == TTAG_fpgm
             || tag == TTAG_prep
             || tag == TTAG_cvt
             || tag == TTAG_DSIG)
      continue;

    /* make the allocated buffer length a multiple of 4 */
    buf_len = (len + 3) & -3;
    buf = (FT_Byte*)malloc(buf_len * sizeof (FT_Byte));
    if (!buf)
      return FT_Err_Out_Of_Memory;

    /* pad end of buffer with zeros */
    buf[buf_len - 1] = 0x00;
    buf[buf_len - 2] = 0x00;
    buf[buf_len - 3] = 0x00;

    /* load table */
    error = FT_Load_Sfnt_Table(sfnt->face, tag, 0, buf, &len);
    if (error)
      goto Err;

    /* check whether we already have this table */
    for (j = 0; j < font->num_tables; j++)
    {
      SFNT_Table* table = &font->tables[j];


      if (table->tag == tag
          && table->len == len
          && !memcmp(table->buf, buf, len))
        break;
    }

    if (j == font->num_tables)
    {
      /* add element to table array if it is missing or different; */
      /* in case of success, `buf' gets linked and is eventually */
      /* freed in `TA_font_unload' */
      error = TA_font_add_table(font, table_info, tag, len, buf);
      if (error)
        goto Err;
    }
    else
    {
      /* reuse existing SFNT table */
      free(buf);
      *table_info = j;
    }
    continue;

  Err:
    free(buf);
    return error;
  }

  /* no (non-empty) `glyf' table; this can't be a TTF with outlines */
  if (glyf_idx == MISSING)
    return FT_Err_Invalid_Argument;

  return TA_Err_Ok;
}


/* we build a dummy `DSIG' table only */

static FT_Error
TA_table_construct_DSIG(FT_Byte** DSIG)
{
  FT_Byte* buf;


  buf = (FT_Byte*)malloc(DSIG_LEN * sizeof (FT_Byte));
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* version */
  buf[0] = 0x00;
  buf[1] = 0x00;
  buf[2] = 0x00;
  buf[3] = 0x01;

  /* zero signatures */
  buf[4] = 0x00;
  buf[5] = 0x00;

  /* permission flags */
  buf[6] = 0x00;
  buf[7] = 0x00;

  *DSIG = buf;

  return TA_Err_Ok;
}


static void
TA_font_compute_table_offsets(FONT* font,
                              FT_ULong start)
{
  FT_ULong  i;
  FT_ULong  offset = start;


  for (i = 0; i < font->num_tables; i++)
  {
    SFNT_Table* table = &font->tables[i];


    table->offset = offset;

    /* table offsets must be a multiple of 4; */
    /* this also fits the actual buffer length */
    offset += (table->len + 3) & ~3;
  }
}


static FT_Error
TA_sfnt_construct_header(SFNT* sfnt,
                         FONT* font,
                         FT_Byte** header_buf,
                         FT_ULong * header_len)
{
  SFNT_Table* tables = font->tables;

  SFNT_Table_Info* table_infos = sfnt->table_infos;
  FT_ULong num_table_infos = sfnt->num_table_infos;

  FT_Byte* buf;
  FT_ULong len;

  FT_Byte* table_record;

  FT_Byte* head_buf = NULL; /* pointer to `head' table */
  FT_ULong head_checksum; /* checksum in `head' table */

  FT_ULong num_tables_in_header;
  FT_ULong i;


  num_tables_in_header = 0;
  for (i = 0; i < num_table_infos; i++)
  {
    /* ignore empty tables */
    if (table_infos[i] != MISSING)
      num_tables_in_header++;
  }

  len = 12 + 16 * num_tables_in_header;
  buf = (FT_Byte*)malloc(len * sizeof (FT_Byte));
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* SFNT version */
  buf[0] = 0x00;
  buf[1] = 0x01;
  buf[2] = 0x00;
  buf[3] = 0x00;

  /* number of tables */
  buf[4] = HIGH(num_tables_in_header);
  buf[5] = LOW(num_tables_in_header);

  /* auxiliary data */
  {
    FT_ULong search_range, entry_selector, range_shift;
    FT_ULong i, j;


    for (i = 1, j = 2; j <= num_tables_in_header; i++, j <<= 1)
      ;

    entry_selector = i - 1;
    search_range = 0x10 << entry_selector;
    range_shift = (num_tables_in_header << 4) - search_range;

    buf[6] = HIGH(search_range);
    buf[7] = LOW(search_range);
    buf[8] = HIGH(entry_selector);
    buf[9] = LOW(entry_selector);
    buf[10] = HIGH(range_shift);
    buf[11] = LOW(range_shift);
  }

  /* location of the first table info record */
  table_record = &buf[12];

  /* the first SFNT table immediately follows the header */
  TA_font_compute_table_offsets(font, len);

  head_checksum = 0;

  /* loop over all tables */
  for (i = 0; i < num_table_infos; i++)
  {
    SFNT_Table_Info table_info = table_infos[i];
    SFNT_Table* table;


    /* ignore empty slots */
    if (table_info == MISSING)
      continue;

    table = &tables[table_info];

    if (table->tag == TTAG_head)
    {
      /* we always reach this IF clause since FreeType would */
      /* have aborted already if the `head' table were missing */

      head_buf = table->buf;

      /* reset checksum in `head' table for recalculation */
      head_buf[8] = 0x00;
      head_buf[9] = 0x00;
      head_buf[10] = 0x00;
      head_buf[11] = 0x00;
    }

    head_checksum += table->checksum;

    table_record[0] = BYTE1(table->tag);
    table_record[1] = BYTE2(table->tag);
    table_record[2] = BYTE3(table->tag);
    table_record[3] = BYTE4(table->tag);

    table_record[4] = BYTE1(table->checksum);
    table_record[5] = BYTE2(table->checksum);
    table_record[6] = BYTE3(table->checksum);
    table_record[7] = BYTE4(table->checksum);

    table_record[8] = BYTE1(table->offset);
    table_record[9] = BYTE2(table->offset);
    table_record[10] = BYTE3(table->offset);
    table_record[11] = BYTE4(table->offset);

    table_record[12] = BYTE1(table->len);
    table_record[13] = BYTE2(table->len);
    table_record[14] = BYTE3(table->len);
    table_record[15] = BYTE4(table->len);

    table_record += 16;
  }

  /* the font header is complete; compute `head' checksum */
  head_checksum += TA_table_compute_checksum(buf, len);
  head_checksum = 0xB1B0AFBAUL - head_checksum;

  /* store checksum in `head' table; */
  head_buf[8] = BYTE1(head_checksum);
  head_buf[9] = BYTE2(head_checksum);
  head_buf[10] = BYTE3(head_checksum);
  head_buf[11] = BYTE4(head_checksum);

  *header_buf = buf;
  *header_len = len;

  return TA_Err_Ok;
}


static FT_Error
TA_font_build_TTF(FONT* font)
{
  SFNT* sfnt = &font->sfnts[0];

  SFNT_Table* tables;
  FT_ULong num_tables;

  FT_Byte* DSIG_buf;

  FT_Byte* header_buf;
  FT_ULong header_len;

  FT_ULong i;
  FT_Error error;


  /* add a dummy `DSIG' table */

  error = TA_sfnt_add_table_info(sfnt);
  if (error)
    return error;

  error = TA_table_construct_DSIG(&DSIG_buf);
  if (error)
    return error;

  /* in case of success, `DSIG_buf' gets linked */
  /* and is eventually freed in `TA_font_unload' */
  error = TA_font_add_table(font,
                            &sfnt->table_infos[sfnt->num_table_infos - 1],
                            TTAG_DSIG, DSIG_LEN, DSIG_buf);
  if (error)
  {
    free(DSIG_buf);
    return error;
  }

  TA_sfnt_sort_table_info(sfnt, font);

  error = TA_sfnt_construct_header(sfnt, font, &header_buf, &header_len);
  if (error)
    return error;

  /* build font */

  tables = font->tables;
  num_tables = font->num_tables;

  font->out_len = tables[num_tables - 1].offset
                  + ((tables[num_tables - 1].len + 3) & ~3);
  font->out_buf = (FT_Byte*)malloc(font->out_len * sizeof (FT_Byte));
  if (!font->out_buf)
  {
    error = FT_Err_Out_Of_Memory;
    goto Err;
  }

  memcpy(font->out_buf, header_buf, header_len);

  for (i = 0; i < num_tables; i++)
  {
    SFNT_Table *table = &tables[i];


    /* buffer length is a multiple of 4 */
    memcpy(font->out_buf + table->offset,
           table->buf, (table->len + 3) & ~3);
  }

  error = TA_Err_Ok;

Err:
  free(header_buf);

  return error;
}


static FT_Error
TA_font_build_TTC(FONT* font)
{
  return TA_Err_Ok;
}


static FT_Error
TA_font_write(FONT* font,
              FILE* out)
{
  if (fwrite(font->out_buf, 1, font->out_len, out) != font->out_len)
    return TA_Err_Invalid_Stream_Write;

  return TA_Err_Ok;
}


static void
TA_font_unload(FONT* font)
{
  /* in case of error it is expected that unallocated pointers */
  /* are NULL (and counters are zero) */

  if (!font)
    return;

  if (font->tables)
  {
    FT_ULong i;


    for (i = 0; i < font->num_tables; i++)
      free(font->tables[i].buf);
    free(font->tables);
  }

  if (font->sfnts)
  {
    FT_Long i;


    for (i = 0; i < font->num_sfnts; i++)
    {
      FT_Done_Face(font->sfnts[i].face);
      free(font->sfnts[i].table_infos);
    }
    free(font->sfnts);
  }

  FT_Done_FreeType(font->lib);
  free(font->in_buf);
  free(font->out_buf);
  free(font);
}


TA_Error
TTF_autohint(FILE* in,
             FILE* out)
{
  FONT* font;
  FT_Error error;
  FT_Long i;


  font = (FONT*)calloc(1, sizeof (FONT));
  if (!font)
    return FT_Err_Out_Of_Memory;

  error = TA_font_read(font, in);
  if (error)
    goto Err;

  error = TA_font_init(font);
  if (error)
    goto Err;

  /* loop over subfonts */
  for (i = 0; i < font->num_sfnts; i++)
  {
    SFNT* sfnt = &font->sfnts[i];


    error = FT_New_Memory_Face(font->lib, font->in_buf, font->in_len,
                               i, &sfnt->face);
    if (error)
      goto Err;

    error = TA_sfnt_split_into_SFNT_tables(sfnt, font);
    if (error)
      goto Err;
  }

  /* compute global hints */
  /* construct `fpgm' table */
  /* construct `prep' table */
  /* construct `cvt ' table */

  /* split `glyf' table */
  /* handle all glyphs in a loop */
    /* strip bytecode */
    /* hint the glyph */
    /* construct bytecode */

  /* construct `glyf' table */

  if (font->num_sfnts == 1)
    error = TA_font_build_TTF(font);
  else
    error = TA_font_build_TTC(font);
  if (error)
    goto Err;

  error = TA_font_write(font, out);
  if (error)
    goto Err;

  error = TA_Err_Ok;

Err:
  TA_font_unload(font);

  return error;
}

/* end of ttfautohint.c */
