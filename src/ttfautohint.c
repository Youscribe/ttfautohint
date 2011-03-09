/* ttfautohint.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

/* This file need FreeType 2.4.5 or newer. */


#include <config.h>
#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TAGS_H

#include "ttfautohint.h"


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

  for (i = 0; i < num_tables; i++)
  {
    FT_ULong  tag, dummy;


    error = FT_Sfnt_Table_Info(face, i, &tag, &dummy);
    if (error && error != FT_Err_Table_Missing)
      goto Err2;
    if (tag == TTAG_glyf)
      break;
  }

  /* no `glyf' table; this can't be a TTF with outlines */
  if (i == num_tables)
  {
    error = FT_Err_Invalid_Argument;
    goto Err2;
  }

  /* split font into SFNT tables */
  /* strip `fpgm' table */
  /* strip `prep' table */
  /* strip `cvt ' table */

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

Err2:
  FT_Done_Face(face);

Err1:
  FT_Done_FreeType(lib);

Err:
  free(in_buf);
  return error;
}

/* end of ttfautohint.c */
