/* ttfautohint.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

/* This file needs FreeType 2.4.5 or newer. */


#include <config.h>
#include <stdio.h>
#include <string.h>

#include <ft2build.h>
#include FT_FREETYPE_H
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


TA_Error
TTF_autohint(FILE *in,
             FILE *out)
{
  FT_Library lib = NULL;

  FT_Byte* in_buf;
  size_t in_len;

  SFNT* sfnts;
  FT_Long num_sfnts = 0;

  SFNT_Table* tables = NULL;
  FT_ULong num_tables = 0;

  FT_ULong glyf_idx;

  FT_Error error;

  FT_Long i;
  FT_ULong j;


  /*************************/
  /*                       */
  /* load font into memory */
  /*                       */
  /*************************/

  fseek(in, 0, SEEK_END);
  in_len = ftell(in);
  fseek(in, 0, SEEK_SET);

  /* a TTF can never be that small */
  if (in_len < 100)
    return FT_Err_Invalid_Argument;

  in_buf = (FT_Byte*)malloc(in_len);
  if (!in_buf)
    return FT_Err_Out_Of_Memory;

  if (fread(in_buf, 1, in_len, in) != in_len)
  {
    error = FT_Err_Invalid_Stream_Read;
    goto Err;
  }

  error = FT_Init_FreeType(&lib);
  if (error)
    goto Err;

  /* get number of faces */
  {
    FT_Face f;


    error = FT_New_Memory_Face(lib, in_buf, in_len, -1, &f);
    if (error)
      goto Err;
    num_sfnts = f->num_faces;
    FT_Done_Face(f);
  }

  /* it is a TTC if we have more than a single face */
  sfnts = (SFNT*)calloc(1, num_sfnts * sizeof (SFNT));
  if (!sfnts)
  {
    error = FT_Err_Out_Of_Memory;
    goto Err;
  }

  for (i = 0; i < num_sfnts; i++)
  {
    SFNT *sfnt = &sfnts[i];


    error = FT_New_Memory_Face(lib, in_buf, in_len, i, &sfnt->face);
    if (error)
      goto Err;

    /* check that font is TTF */
    if (!FT_IS_SFNT(sfnt->face))
    {
      error = FT_Err_Invalid_Argument;
      goto Err;
    }

    error = FT_Sfnt_Table_Info(sfnt->face, 0, NULL, &sfnt->num_table_infos);
    if (error)
      goto Err;

    sfnt->table_infos =
      (SFNT_Table*)calloc(1, sfnt->num_table_infos * sizeof (SFNT_Table));
    if (!sfnt->table_infos)
    {
      error = FT_Err_Out_Of_Memory;
      goto Err;
    }

    /* collect SFNT table information and search for `glyf' table */
    glyf_idx = sfnt->num_table_infos;
    for (j = 0; j < sfnt->num_table_infos; j++)
    {
      FT_ULong tag, len;


      error = FT_Sfnt_Table_Info(sfnt->face, j, &tag, &len);
      if (error && error != FT_Err_Table_Missing)
        goto Err;

      if (!error)
      {
        if (tag == TTAG_glyf)
          glyf_idx = j;

        /* ignore tables which we are going to create by ourselves */
        if (!(tag == TTAG_fpgm
              || tag == TTAG_prep
              || tag == TTAG_cvt))
        {
          sfnt->table_infos[j].tag = tag;
          sfnt->table_infos[j].len = len;
        }
      }
    }

    /* no (non-empty) `glyf' table; this can't be a TTF with outlines */
    if (glyf_idx == sfnt->num_table_infos)
    {
      error = FT_Err_Invalid_Argument;
      goto Err;
    }


    /*******************************/
    /*                             */
    /* split font into SFNT tables */
    /*                             */
    /*******************************/

    for (j = 0; j < sfnt->num_table_infos; j++)
    {
      SFNT_Table *table_info = &sfnt->table_infos[j];


      /* we ignore empty tables */
      if (table_info->len)
      {
        SFNT_Table* tables_new;
        SFNT_Table* table_last;
        SFNT_Table* table;
        FT_Byte* buf;

        FT_Long k;


        buf = (FT_Byte*)malloc(table_info->len);
        if (!buf)
        {
          error = FT_Err_Out_Of_Memory;
          goto Err;
        }

        /* load table */
        error = FT_Load_Sfnt_Table(sfnt->face, table_info->tag, 0,
                                   buf, &table_info->len);
        if (error)
          goto Err1;

        /* check whether we already have this table */
        for (k = 0; k < num_tables; k++)
        {
          table = &tables[k];

          if (table->tag == table_info->tag
              && table->len == table_info->len
              && !memcmp(table->buf, buf, table->len))
            break;
        }

        /* add one element to table array if it's missing or not the same */
        if (k == num_tables)
        {
          num_tables++;
          tables_new =
            (SFNT_Table*)realloc(tables,
                                 num_tables * sizeof (SFNT_Table));
          if (!tables_new)
          {
            error = FT_Err_Out_Of_Memory;
            goto Err1;
          }
          else
            tables = tables_new;

          table_last = &tables[num_tables - 1];

          table_last->tag = table_info->tag;
          table_last->len = table_info->len;
          table_last->buf = buf;

          /* link buffer pointer */
          table_info->buf = table_last->buf;
        }
        else
        {
          free(buf);
          table_info->buf = tables[k].buf;
        }
        continue;

      Err1:
        free(buf);
        goto Err;
      }
    }
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
  /* in case of error it is expected that unallocated pointers */
  /* are NULL (and counters are zero) */
  if (tables)
  {
    for (j = 0; j < num_tables; j++)
      free(tables[j].buf);
    free(tables);
  }
  if (sfnts)
  {
    for (i = 0; i < num_sfnts; i++)
    {
      FT_Done_Face(sfnts[i].face);
      free(sfnts[i].table_infos);
    }
    free(sfnts);
  }
  FT_Done_FreeType(lib);
  free(in_buf);

  return error;
}

/* end of ttfautohint.c */
