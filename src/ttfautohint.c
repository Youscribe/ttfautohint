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
  FT_Library lib;
  FT_Face face;
  FT_Long num_faces;
  FT_ULong num_tables;

  FT_Byte* in_buf;
  size_t in_len;

  SFNT_Table* SFNT_Tables;
  FT_ULong glyf_idx;

  FT_Error error;

  FT_ULong i;


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
    goto Err1;
  num_faces = face->num_faces;
  FT_Done_Face(face);

  error = FT_New_Memory_Face(lib, in_buf, in_len, 0, &face);
  if (error)
    goto Err1;

  /* check that font is TTF */
  if (!FT_IS_SFNT(face))
  {
    error = FT_Err_Invalid_Argument;
    goto Err2;
  }

  error = FT_Sfnt_Table_Info(face, 0, NULL, &num_tables);
  if (error)
    goto Err2;

  /* allocate three more slots for the tables we will eventually add */
  num_tables += 3;
  SFNT_Tables = (SFNT_Table*)calloc(1, num_tables * sizeof (SFNT_Table));
  if (!SFNT_Tables)
  {
    error = FT_Err_Out_Of_Memory;
    goto Err2;
  }

  /* collect SFNT table data */
  glyf_idx = num_tables - 3;
  for (i = 0; i < num_tables - 3; i++)
  {
    FT_ULong tag, len;


    error = FT_Sfnt_Table_Info(face, i, &tag, &len);
    if (error && error != FT_Err_Table_Missing)
      goto Err3;

    if (!error)
    {
      if (tag == TTAG_glyf)
        glyf_idx = i;

      /* ignore tables which we are going to create by ourselves */
      if (!(tag == TTAG_fpgm
            || tag == TTAG_prep
            || tag == TTAG_cvt))
      {
        SFNT_Tables[i].tag = tag;
        SFNT_Tables[i].len = len;
      }
    }
  }

  /* no (non-empty) `glyf' table; this can't be a TTF with outlines */
  if (glyf_idx == num_tables - 3)
  {
    error = FT_Err_Invalid_Argument;
    goto Err3;
  }


  /*** split font into SFNT tables ***/

  {
    SFNT_Table* st = SFNT_Tables;


    for (i = 0; i < num_tables - 3; i++)
    {
      if (st->len)
      {
        st->buf = (FT_Byte*)malloc(st->len);
        if (!st->buf)
        {
          error = FT_Err_Out_Of_Memory;
          goto Err4;
        }

        error = FT_Load_Sfnt_Table(face, st->tag, 0, st->buf, &st->len);
        if (error)
          goto Err4;

        st++;
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

Err4:
  for (i = 0; i < num_tables; i++)
    free(SFNT_Tables[i].buf);
Err3:
  free(SFNT_Tables);
Err2:
  FT_Done_Face(face);
Err1:
  FT_Done_FreeType(lib);
Err:
  free(in_buf);

  return error;
}

/* end of ttfautohint.c */
