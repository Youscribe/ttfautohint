/* tacvt.c */

/*
 * Copyright (C) 2011-2012 by Werner Lemberg.
 *
 * This file is part of the ttfautohint library, and may only be used,
 * modified, and distributed under the terms given in `COPYING'.  By
 * continuing to use, modify, or distribute this file you indicate that you
 * have read `COPYING' and understand and accept it fully.
 *
 * The file `COPYING' mentioned in the previous paragraph is distributed
 * with the ttfautohint library.
 */


#include "ta.h"


static FT_Error
TA_sfnt_compute_global_hints(SFNT* sfnt,
                             FONT* font)
{
  FT_Error error;
  FT_Face face = sfnt->face;
  FT_UInt idx;
  FT_Int32 load_flags;


  error = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
  if (error)
  {
    if (font->symbol)
    {
      error = FT_Select_Charmap(face, FT_ENCODING_MS_SYMBOL);
      if (error)
        return TA_Err_Missing_Symbol_CMap;
    }
    else
      return TA_Err_Missing_Unicode_CMap;
  }

  if (font->symbol)
    idx = 0;
  else
  {
    /* load glyph `o' to trigger all initializations */
    /* XXX make this configurable for non-latin scripts */
    /* XXX make this configurable to use a different letter */
    idx = FT_Get_Char_Index(face, 'o');
    if (!idx)
      return TA_Err_Missing_Glyph;
  }

  load_flags = 1 << 29; /* vertical hinting only */
  error = ta_loader_load_glyph(font, face, idx, load_flags);

  return error;
}


static FT_Error
TA_table_build_cvt(FT_Byte** cvt,
                   FT_ULong* cvt_len,
                   SFNT* sfnt,
                   FONT* font)
{
  TA_LatinAxis haxis;
  TA_LatinAxis vaxis;

  FT_UInt hwidth_count;
  FT_UInt vwidth_count;
  FT_UInt blue_count;

  FT_UInt i;
  FT_UInt buf_len;
  FT_UInt len;
  FT_Byte* buf;
  FT_Byte* buf_p;

  FT_Error error;


  error = TA_sfnt_compute_global_hints(sfnt, font);
  if (error)
    return error;

  if (font->loader->hints.metrics->clazz->script == TA_SCRIPT_DUMMY)
  {
    haxis = NULL;
    vaxis = NULL;

    hwidth_count = 0;
    vwidth_count = 0;
    blue_count = 0;
  }
  else
  {
    haxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[0];
    vaxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[1];

    hwidth_count = haxis->width_count;
    vwidth_count = vaxis->width_count;
    blue_count = vaxis->blue_count;
  }

  buf_len = 2 * (cvtl_max_runtime /* runtime values */
                 + 2 /* vertical and horizontal standard width */
                 + hwidth_count
                 + vwidth_count
                 + 2 * blue_count);

  /* buffer length must be a multiple of four */
  len = (buf_len + 3) & ~3;
  buf = (FT_Byte*)malloc(len);
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* pad end of buffer with zeros */
  buf[len - 1] = 0x00;
  buf[len - 2] = 0x00;
  buf[len - 3] = 0x00;

  buf_p = buf;

  /* some CVT values are initialized (and modified) at runtime; */
  /* see the `cvtl_xxx' macros in tabytecode.h */
  for (i = 0; i < cvtl_max_runtime; i++)
  {
    *(buf_p++) = 0;
    *(buf_p++) = 0;
  }

  if (hwidth_count > 0)
  {
    *(buf_p++) = HIGH(haxis->widths[0].org);
    *(buf_p++) = LOW(haxis->widths[0].org);
  }
  else
  {
    *(buf_p++) = 0;
    *(buf_p++) = 50;
  }
  if (vwidth_count > 0)
  {
    *(buf_p++) = HIGH(vaxis->widths[0].org);
    *(buf_p++) = LOW(vaxis->widths[0].org);
  }
  else
  {
    *(buf_p++) = 0;
    *(buf_p++) = 50;
  }

  for (i = 0; i < hwidth_count; i++)
  {
    if (haxis->widths[i].org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(haxis->widths[i].org);
    *(buf_p++) = LOW(haxis->widths[i].org);
  }

  for (i = 0; i < vwidth_count; i++)
  {
    if (vaxis->widths[i].org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(vaxis->widths[i].org);
    *(buf_p++) = LOW(vaxis->widths[i].org);
  }

  for (i = 0; i < blue_count; i++)
  {
    if (vaxis->blues[i].ref.org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(vaxis->blues[i].ref.org);
    *(buf_p++) = LOW(vaxis->blues[i].ref.org);
  }

  for (i = 0; i < blue_count; i++)
  {
    if (vaxis->blues[i].shoot.org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(vaxis->blues[i].shoot.org);
    *(buf_p++) = LOW(vaxis->blues[i].shoot.org);
  }

  *cvt = buf;
  *cvt_len = buf_len;

  return FT_Err_Ok;

Err:
  free(buf);
  return TA_Err_Hinter_Overflow;
}


FT_Error
TA_sfnt_build_cvt_table(SFNT* sfnt,
                        FONT* font)
{
  FT_Error error = FT_Err_Ok;

  SFNT_Table* glyf_table = &font->tables[sfnt->glyf_idx];
  glyf_Data* data = (glyf_Data*)glyf_table->data;

  FT_Byte* cvt_buf;
  FT_ULong cvt_len;


  error = TA_sfnt_add_table_info(sfnt);
  if (error)
    goto Exit;

  /* `glyf', `cvt', `fpgm', and `prep' are always used in parallel */
  if (glyf_table->processed)
  {
    sfnt->table_infos[sfnt->num_table_infos - 1] = data->cvt_idx;
    goto Exit;
  }

  error = TA_table_build_cvt(&cvt_buf, &cvt_len, sfnt, font);
  if (error)
    goto Exit;

  /* in case of success, `cvt_buf' gets linked */
  /* and is eventually freed in `TA_font_unload' */
  error = TA_font_add_table(font,
                            &sfnt->table_infos[sfnt->num_table_infos - 1],
                            TTAG_cvt, cvt_len, cvt_buf);
  if (error)
    free(cvt_buf);
  else
    data->cvt_idx = sfnt->table_infos[sfnt->num_table_infos - 1];

Exit:
  return error;
}

/* end of tacvt.c */
