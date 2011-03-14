/* ttfautohint.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

/* This file needs FreeType 2.4.5 or newer. */


#include <config.h>
#include <stdio.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H
#include FT_TRUETYPE_TAGS_H

#include "ttfautohint.h"


/* this structure represents both the data contained in the SFNT */
/* table records and the actual SFNT table data */
typedef struct SFNT_Table_ {
  FT_ULong tag;
  FT_ULong len;
  FT_Byte* buf;
  FT_ULong offset; /* used while building output font only */
} SFNT_Table;

/* this structure is used to model a font within a TTC */
typedef struct SFNT_ {
  FT_Face face;
  SFNT_Table* table_infos;
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
TA_font_read(FILE* in,
             FONT* font)
{
  fseek(in, 0, SEEK_END);
  font->in_len = ftell(in);
  fseek(in, 0, SEEK_SET);

  /* a TTF can never be that small */
  if (font->in_len < 100)
    return FT_Err_Invalid_Argument;

  font->in_buf = (FT_Byte*)malloc(font->in_len);
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
TA_font_collect_table_info(SFNT* sfnt)
{
  FT_Error error;
  FT_ULong glyf_idx;
  FT_ULong i;


  /* check that font is TTF */
  if (!FT_IS_SFNT(sfnt->face))
    return FT_Err_Invalid_Argument;

  error = FT_Sfnt_Table_Info(sfnt->face, 0, NULL, &sfnt->num_table_infos);
  if (error)
    return error;

  sfnt->table_infos =
    (SFNT_Table*)calloc(1, sfnt->num_table_infos * sizeof (SFNT_Table));
  if (!sfnt->table_infos)
    return FT_Err_Out_Of_Memory;

  /* collect SFNT table information and search for `glyf' table */
  glyf_idx = sfnt->num_table_infos;
  for (i = 0; i < sfnt->num_table_infos; i++)
  {
    FT_ULong tag, len;


    error = FT_Sfnt_Table_Info(sfnt->face, i, &tag, &len);
    if (error && error != FT_Err_Table_Missing)
      return error;

    if (!error)
    {
      if (tag == TTAG_glyf)
        glyf_idx = i;

      /* ignore tables which we are going to create by ourselves */
      if (!(tag == TTAG_fpgm
            || tag == TTAG_prep
            || tag == TTAG_cvt))
      {
        sfnt->table_infos[i].tag = tag;
        sfnt->table_infos[i].len = len;
      }
    }
  }

  /* no (non-empty) `glyf' table; this can't be a TTF with outlines */
  if (glyf_idx == sfnt->num_table_infos)
    return FT_Err_Invalid_Argument;

  return TA_Err_Ok;
}


static FT_Error
TA_font_split_into_SFNT_tables(SFNT* sfnt,
                               FONT* font)
{
  FT_Error error;
  FT_ULong i;


  for (i = 0; i < sfnt->num_table_infos; i++)
  {
    SFNT_Table* table_info = &sfnt->table_infos[i];


    /* we ignore empty tables */
    if (table_info->len)
    {
      FT_Byte* buf;
      FT_ULong len;
      FT_ULong j;


      /* assure that table length is a multiple of 4; */
      /* we need this later for writing the tables back */
      len = (table_info->len + 3) & ~3;

      buf = (FT_Byte*)malloc(len);
      if (!buf)
        return FT_Err_Out_Of_Memory;

      /* pad table with zeros */
      buf[len - 1] = 0;
      buf[len - 2] = 0;
      buf[len - 3] = 0;

      /* load table */
      error = FT_Load_Sfnt_Table(sfnt->face, table_info->tag, 0,
                                 buf, &table_info->len);
      if (error)
        goto Err;

      table_info->len = len;

      /* check whether we already have this table */
      for (j = 0; j < font->num_tables; j++)
      {
        SFNT_Table* table = &font->tables[j];


        if (table->tag == table_info->tag
            && table->len == table_info->len
            && !memcmp(table->buf, buf, table->len))
          break;
      }

      /* add element to table array if it's missing or not the same */
      if (j == font->num_tables)
      {
        SFNT_Table* tables_new;
        SFNT_Table* table_last;


        font->num_tables++;
        tables_new =
          (SFNT_Table*)realloc(font->tables,
                               font->num_tables * sizeof (SFNT_Table));
        if (!tables_new)
        {
          error = FT_Err_Out_Of_Memory;
          goto Err;
        }
        else
          font->tables = tables_new;

        table_last = &font->tables[font->num_tables - 1];

        table_last->tag = table_info->tag;
        table_last->len = table_info->len;
        table_last->buf = buf;

        /* link buffer pointer */
        table_info->buf = table_last->buf;
      }
      else
      {
        free(buf);
        table_info->buf = font->tables[j].buf;
      }
      continue;

    Err:
      free(buf);
      return error;
    }
  }

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


#define HIGH(x) (FT_Byte)(((x) & 0xFF00) >> 8)
#define LOW(x) ((x) & 0x00FF)

#define BYTE1(x) (FT_Byte)(((x) & 0xFF000000UL) >> 24);
#define BYTE2(x) (FT_Byte)(((x) & 0x00FF0000UL) >> 16);
#define BYTE3(x) (FT_Byte)(((x) & 0x0000FF00UL) >> 8);
#define BYTE4(x) ((x) & 0x000000FFUL);


static FT_Error
TA_font_build_TTF(FONT* font)
{
  SFNT* sfnt = &font->sfnts[0];
  SFNT_Table* table_infos = sfnt->table_infos;
  FT_ULong num_table_infos = sfnt->num_table_infos;

  FT_ULong num_tables_in_header;

  FT_Byte* header_buf;
  FT_ULong header_len;

  FT_Byte* head_buf = NULL;
  FT_ULong head_checksum; /* checksum in `head' table */

  FT_Byte* table_record;
  FT_ULong table_offset;

  FT_ULong i;
  FT_Error error;


  num_tables_in_header = 0;

  for (i = 0; i < num_table_infos; i++)
  {
    /* ignore empty tables */
    if (table_infos[i].len)
      num_tables_in_header++;
  }

  if (num_tables_in_header > 0xFFFF)
    return FT_Err_Array_Too_Large;

  /* construct TTF header */

  header_len = 12 + 16 * num_tables_in_header;
  header_buf = (FT_Byte*)malloc(header_len);
  if (!header_buf)
    return FT_Err_Out_Of_Memory;

  /* SFNT version */
  header_buf[0] = 0x00;
  header_buf[1] = 0x01;
  header_buf[2] = 0x00;
  header_buf[3] = 0x00;

  /* number of tables */
  header_buf[4] = HIGH(num_tables_in_header);
  header_buf[5] = LOW(num_tables_in_header);

  /* auxiliary data */
  {
    FT_ULong search_range, entry_selector, range_shift;
    FT_ULong i, j;


    for (i = 1, j = 2; j <= num_tables_in_header; i++, j <<= 1)
      ;

    entry_selector = i - 1;
    search_range = 0x10 << entry_selector;
    range_shift = (num_tables_in_header << 4) - search_range;

    header_buf[6] = HIGH(search_range);
    header_buf[7] = LOW(search_range);
    header_buf[8] = HIGH(entry_selector);
    header_buf[9] = LOW(entry_selector);
    header_buf[10] = HIGH(range_shift);
    header_buf[11] = LOW(range_shift);
  }

  /* location of the first table record */
  table_record = &header_buf[12];
  /* the first table immediately follows the header */
  table_offset = header_len;

  head_checksum = 0;

  /* loop over all tables */
  for (i = 0; i < num_table_infos; i++)
  {
    SFNT_Table *table_info = &table_infos[i];
    FT_ULong table_checksum;


    /* ignore empty tables */
    if (!table_info->len)
      continue;

    table_info->offset = table_offset;

    if (table_info->tag == TTAG_head)
    {
      /* we always reach this IF clause since FreeType would */
      /* have aborted already if the `head' table were missing */

      head_buf = table_info->buf;

      /* reset checksum in `head' table for recalculation */
      head_buf[8] = 0x00;
      head_buf[9] = 0x00;
      head_buf[10] = 0x00;
      head_buf[11] = 0x00;
    }

    table_checksum = TA_table_compute_checksum(table_info->buf,
                                               table_info->len);
    head_checksum += table_checksum;

    table_record[0] = BYTE1(table_info->tag);
    table_record[1] = BYTE2(table_info->tag);
    table_record[2] = BYTE3(table_info->tag);
    table_record[3] = BYTE4(table_info->tag);

    table_record[4] = BYTE1(table_checksum);
    table_record[5] = BYTE2(table_checksum);
    table_record[6] = BYTE3(table_checksum);
    table_record[7] = BYTE4(table_checksum);

    table_record[8] = BYTE1(table_offset);
    table_record[9] = BYTE2(table_offset);
    table_record[10] = BYTE3(table_offset);
    table_record[11] = BYTE4(table_offset);

    table_record[12] = BYTE1(table_info->len);
    table_record[13] = BYTE2(table_info->len);
    table_record[14] = BYTE3(table_info->len);
    table_record[15] = BYTE4(table_info->len);

    table_record += 16;
    table_offset += table_info->len;
  }

  /* the font header is complete; compute `head' checksum */
  head_checksum += TA_table_compute_checksum(header_buf, header_len);
  head_checksum = 0xB1B0AFBAUL - head_checksum;

  /* store checksum in `head' table; */
  head_buf[8] = BYTE1(head_checksum);
  head_buf[9] = BYTE2(head_checksum);
  head_buf[10] = BYTE3(head_checksum);
  head_buf[11] = BYTE4(head_checksum);

  /* build font */
  font->out_len = table_offset;
  font->out_buf = (FT_Byte*)malloc(font->out_len);
  if (!font->out_buf)
  {
    error = FT_Err_Out_Of_Memory;
    goto Err;
  }

  memcpy(font->out_buf, header_buf, header_len);

  for (i = 0; i < num_table_infos; i++)
  {
    SFNT_Table *table_info = &table_infos[i];


    if (!table_info->len)
      continue;

    memcpy(font->out_buf + table_info->offset,
           table_info->buf, table_info->len);
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
TA_font_write(FILE* out,
              FONT* font)
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

  error = TA_font_read(in, font);
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

    error = TA_font_collect_table_info(sfnt);
    if (error)
      goto Err;

    error = TA_font_split_into_SFNT_tables(sfnt, font);
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

  /* build font from SFNT tables */
  if (font->num_sfnts == 1)
    error = TA_font_build_TTF(font);
  else
    error = TA_font_build_TTC(font);
  if (error)
    goto Err;

  /* write font from memory */
  error = TA_font_write(out, font);
  if (error)
    goto Err;

  error = TA_Err_Ok;

Err:
  TA_font_unload(font);

  return error;
}

/* end of ttfautohint.c */
