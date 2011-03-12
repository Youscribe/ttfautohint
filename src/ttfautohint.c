/* ttfautohint.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

/* This file needs FreeType 2.4.5 or newer. */


#include <config.h>
#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TAGS_H

#include "ttfautohint.h"


typedef struct SFNT_Table_ {
  FT_ULong tag;
  FT_ULong len;
  FT_Byte* buf;
} SFNT_Table;


TA_Error
TTF_autohint(FILE *in,
             FILE *out)
{
  FT_Library lib = NULL;
  FT_Face face = NULL;
  FT_Long num_faces = 0;
  FT_ULong num_table_infos = 0;

  FT_Byte* in_buf;
  size_t in_len;

  SFNT_Table* SFNT_Table_Infos = NULL;
  FT_ULong glyf_idx;

  FT_Error error;

  FT_ULong j;


  /*** load font into memory ***/

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

  error = FT_New_Memory_Face(lib, in_buf, in_len, -1, &face);
  if (error)
    goto Err;
  num_faces = face->num_faces;
  FT_Done_Face(face);

  error = FT_New_Memory_Face(lib, in_buf, in_len, 0, &face);
  if (error)
    goto Err;

  /* check that font is TTF */
  if (!FT_IS_SFNT(face))
  {
    error = FT_Err_Invalid_Argument;
    goto Err;
  }

  error = FT_Sfnt_Table_Info(face, 0, NULL, &num_table_infos);
  if (error)
    goto Err;

  SFNT_Table_Infos =
    (SFNT_Table*)calloc(1, num_table_infos * sizeof (SFNT_Table));
  if (!SFNT_Table_Infos)
  {
    error = FT_Err_Out_Of_Memory;
    goto Err;
  }

  /* collect SFNT table data */
  glyf_idx = num_table_infos;
  for (j = 0; j < num_table_infos; j++)
  {
    FT_ULong tag, len;


    error = FT_Sfnt_Table_Info(face, j, &tag, &len);
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
        SFNT_Table_Infos[j].tag = tag;
        SFNT_Table_Infos[j].len = len;
      }
    }
  }

  /* no (non-empty) `glyf' table; this can't be a TTF with outlines */
  if (glyf_idx == num_table_infos)
  {
    error = FT_Err_Invalid_Argument;
    goto Err;
  }


  /*** split font into SFNT tables ***/

  {
    SFNT_Table* stp = SFNT_Table_Infos;


    for (j = 0; j < num_table_infos; j++)
    {
      if (stp->len)
      {
        stp->buf = (FT_Byte*)malloc(stp->len);
        if (!stp->buf)
        {
          error = FT_Err_Out_Of_Memory;
          goto Err;
        }

        error = FT_Load_Sfnt_Table(face, stp->tag, 0, stp->buf, &stp->len);
        if (error)
          goto Err;

        stp++;
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
  /* in case of error it is expected that the unallocated pointers */
  /* are NULL (and counters are zero) */
  if (SFNT_Table_Infos)
  {
    for (j = 0; j < num_table_infos; j++)
      free(SFNT_Table_Infos[j].buf);
    free(SFNT_Table_Infos);
  }
  FT_Done_Face(face);
  FT_Done_FreeType(lib);
  free(in_buf);

  return error;
}

/* end of ttfautohint.c */
