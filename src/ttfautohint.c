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

  SFNT* sfnts;
  FT_Long num_sfnts;

  SFNT_Table* tables;
  FT_ULong num_tables;
} FONT;


static FT_Error
TA_font_load_into_memory(FILE* in,
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
      FT_ULong j;


      buf = (FT_Byte*)malloc(table_info->len);
      if (!buf)
        return FT_Err_Out_Of_Memory;

      /* load table */
      error = FT_Load_Sfnt_Table(sfnt->face, table_info->tag, 0,
                                 buf, &table_info->len);
      if (error)
        goto Err;

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

  error = TA_font_load_into_memory(in, font);
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
  /* write font from memory */

  error = TA_Err_Ok;

Err:
  TA_font_unload(font);

  return error;
}

/* end of ttfautohint.c */
