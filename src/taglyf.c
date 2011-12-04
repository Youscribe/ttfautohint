/* taglyf.c */

/*
 * Copyright (C) 2011 by Werner Lemberg.
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
#include "tabytecode.h"


FT_Error
TA_sfnt_build_glyf_hints(SFNT* sfnt,
                         FONT* font)
{
  FT_Face face = sfnt->face;
  FT_Long idx;
  FT_Error error;


  for (idx = 0; idx < face->num_glyphs; idx++)
  {
    error = TA_sfnt_build_glyph_instructions(sfnt, font, idx);
    if (error)
      return error;
    if (font->progress)
      font->progress(idx, face->num_glyphs,
                     sfnt - font->sfnts, font->num_sfnts,
                     font->progress_data);
  }

  return FT_Err_Ok;
}


static FT_Error
TA_glyph_parse_composite(GLYPH* glyph,
                         FT_Byte* buf,
                         FT_ULong len)
{
  FT_ULong flags_offset; /* after the loop, this is the offset */
                         /* to the last element in the flags array */
  FT_UShort flags;

  FT_Byte* p;
  FT_Byte* endp;


  p = buf;
  endp = buf + len;

  /* skip header */
  p += 10;

  /* walk over component records */
  do
  {
    if (p + 4 > endp)
      return FT_Err_Invalid_Table;

    flags_offset = p - buf;

    flags = *(p++) << 8;
    flags += *(p++);

    /* skip glyph component index */
    p += 2;

    /* skip scaling and offset arguments */
    if (flags & ARGS_ARE_WORDS)
      p += 4;
    else
      p += 2;

    if (flags & WE_HAVE_A_SCALE)
      p += 2;
    else if (flags & WE_HAVE_AN_XY_SCALE)
      p += 4;
    else if (flags & WE_HAVE_A_2X2)
      p += 8;
  } while (flags & MORE_COMPONENTS);

  glyph->flags_offset = flags_offset;

  /* adjust glyph record length */
  len = p - buf;

  glyph->len1 = len;
  /* glyph->len2 = 0; */
  glyph->buf = (FT_Byte*)malloc(len);
  if (!glyph->buf)
    return FT_Err_Out_Of_Memory;

  /* copy record without instructions (if any) */
  memcpy(glyph->buf, buf, len);
  glyph->buf[flags_offset] &= ~(WE_HAVE_INSTR >> 8);

  return TA_Err_Ok;
}


static FT_Error
TA_glyph_parse_simple(GLYPH* glyph,
                      FT_Byte* buf,
                      FT_UShort num_contours,
                      FT_ULong len)
{
  FT_ULong ins_offset;
  FT_Byte* flags_start;

  FT_UShort num_ins;
  FT_UShort num_pts;

  FT_ULong flags_size; /* size of the flags array */
  FT_ULong xy_size; /* size of x and y coordinate arrays together */

  FT_Byte* p;
  FT_Byte* endp;

  FT_UShort i;


  p = buf;
  endp = buf + len;

  ins_offset = 10 + num_contours * 2;

  p += ins_offset;

  if (p + 2 > endp)
    return FT_Err_Invalid_Table;

  /* get number of instructions */
  num_ins = *(p++) << 8;
  num_ins += *(p++);

  p += num_ins;

  if (p > endp)
    return FT_Err_Invalid_Table;

  /* get number of points from last outline point */
  num_pts = buf[ins_offset - 2] << 8;
  num_pts += buf[ins_offset - 1];
  num_pts++;

  flags_start = p;
  xy_size = 0;
  i = 0;

  while (i < num_pts)
  {
    FT_Byte flags;
    FT_Byte x_short;
    FT_Byte y_short;
    FT_Byte have_x;
    FT_Byte have_y;
    FT_Byte count;


    if (p + 1 > endp)
      return FT_Err_Invalid_Table;

    flags = *(p++);

    x_short = (flags & X_SHORT_VECTOR) ? 1 : 2;
    y_short = (flags & Y_SHORT_VECTOR) ? 1 : 2;

    have_x = ((flags & SAME_X) && !(flags & X_SHORT_VECTOR)) ? 0 : 1;
    have_y = ((flags & SAME_Y) && !(flags & Y_SHORT_VECTOR)) ? 0 : 1;

    count = 1;

    if (flags & REPEAT)
    {
      if (p + 1 > endp)
        return FT_Err_Invalid_Table;

      count += *(p++);

      if (i + count > num_pts)
        return FT_Err_Invalid_Table;
    }

    xy_size += count * x_short * have_x;
    xy_size += count * y_short * have_y;

    i += count;
  }

  if (p + xy_size > endp)
    return FT_Err_Invalid_Table;

  flags_size = p - flags_start;

  /* store the data before and after the bytecode instructions */
  /* in the same array */
  glyph->len1 = ins_offset;
  glyph->len2 = flags_size + xy_size;
  glyph->buf = (FT_Byte*)malloc(glyph->len1 + glyph->len2);
  if (!glyph->buf)
    return FT_Err_Out_Of_Memory;

  /* now copy everything but the instructions */
  memcpy(glyph->buf, buf, glyph->len1);
  memcpy(glyph->buf + glyph->len1, flags_start, glyph->len2);

  return TA_Err_Ok;
}


FT_Error
TA_sfnt_split_glyf_table(SFNT* sfnt,
                         FONT* font)
{
  SFNT_Table* glyf_table = &font->tables[sfnt->glyf_idx];
  SFNT_Table* loca_table = &font->tables[sfnt->loca_idx];
  SFNT_Table* head_table = &font->tables[sfnt->head_idx];

  glyf_Data* data;
  FT_Byte loca_format;

  FT_ULong offset;
  FT_ULong offset_next;

  FT_Byte* p;
  FT_UShort i;


  /* in case of success, all allocated arrays are */
  /* linked and eventually freed in `TA_font_unload' */

  /* nothing to do if table has already been split */
  if (glyf_table->data)
    return TA_Err_Ok;

  data = (glyf_Data*)calloc(1, sizeof (glyf_Data));
  if (!data)
    return FT_Err_Out_Of_Memory;

  glyf_table->data = data;

  loca_format = head_table->buf[LOCA_FORMAT_OFFSET];

  data->num_glyphs = loca_format ? loca_table->len / 4 - 1
                                 : loca_table->len / 2 - 1;
  data->glyphs = (GLYPH*)calloc(1, data->num_glyphs * sizeof (GLYPH));
  if (!data->glyphs)
    return FT_Err_Out_Of_Memory;

  p = loca_table->buf;

  if (loca_format)
  {
    offset_next = *(p++) << 24;
    offset_next += *(p++) << 16;
    offset_next += *(p++) << 8;
    offset_next += *(p++);
  }
  else
  {
    offset_next = *(p++) << 8;
    offset_next += *(p++);
    offset_next <<= 1;
  }

  /* loop over `loca' and `glyf' data */
  for (i = 0; i < data->num_glyphs; i++)
  {
    GLYPH* glyph = &data->glyphs[i];
    FT_ULong len;


    offset = offset_next;

    if (loca_format)
    {
      offset_next = *(p++) << 24;
      offset_next += *(p++) << 16;
      offset_next += *(p++) << 8;
      offset_next += *(p++);
    }
    else
    {
      offset_next = *(p++) << 8;
      offset_next += *(p++);
      offset_next <<= 1;
    }

    if (offset_next < offset
        || offset_next > glyf_table->len)
      return FT_Err_Invalid_Table;

    len = offset_next - offset;
    if (!len)
      continue; /* empty glyph */
    else
    {
      FT_Byte* buf;
      FT_Short num_contours;
      FT_Error error;


      /* check header size */
      if (len < 10)
        return FT_Err_Invalid_Table;

      buf = glyf_table->buf + offset;
      num_contours = (FT_Short)((buf[0] << 8) + buf[1]);

      /* We must parse the rest of the glyph record to get the exact */
      /* record length.  Since the `loca' table rounds record lengths */
      /* up to multiples of 4 (or 2 for older fonts), and we must round */
      /* up again after stripping off the instructions, it would be */
      /* possible otherwise to have more than 4 bytes of padding which */
      /* is more or less invalid. */

      if (num_contours < 0)
      {
        error = TA_glyph_parse_composite(glyph, buf, len);
        if (error)
          return error;
      }
      else
      {
        error = TA_glyph_parse_simple(glyph, buf, num_contours, len);
        if (error)
          return error;
      }
    }
  }

  return TA_Err_Ok;
}


FT_Error
TA_sfnt_build_glyf_table(SFNT* sfnt,
                         FONT* font)
{
  SFNT_Table* glyf_table = &font->tables[sfnt->glyf_idx];
  glyf_Data* data = (glyf_Data*)glyf_table->data;

  GLYPH* glyph;

  FT_ULong len;
  FT_Byte* buf_new;
  FT_Byte* p;
  FT_UShort i;


  if (glyf_table->processed)
    return TA_Err_Ok;

  /* get table size */
  len = 0;
  glyph = data->glyphs;
  for (i = 0; i < data->num_glyphs; i++, glyph++)
  {
    /* glyph records should have offsets which are multiples of 4 */
    len = (len + 3) & ~3;
    len += glyph->len1 + glyph->len2 + glyph->ins_len;
    /* add two bytes for the instructionLength field */
    if (glyph->len2 || glyph->ins_len)
      len += 2;
  }

  /* to make the short format of the `loca' table always work, */
  /* assure an even length of the `glyf' table */
  glyf_table->len = (len + 1) & ~1;

  buf_new = (FT_Byte*)realloc(glyf_table->buf, (len + 3) & ~3);
  if (!buf_new)
    return FT_Err_Out_Of_Memory;
  else
    glyf_table->buf = buf_new;

  p = glyf_table->buf;
  glyph = data->glyphs;
  for (i = 0; i < data->num_glyphs; i++, glyph++)
  {
    len = glyph->len1 + glyph->len2 + glyph->ins_len;
    if (glyph->len2 || glyph->ins_len)
      len += 2;

    if (len)
    {
      /* copy glyph data and insert new instructions */
      memcpy(p, glyph->buf, glyph->len1);

      if (glyph->len2)
      {
        /* simple glyph */
        p += glyph->len1;
        *(p++) = HIGH(glyph->ins_len);
        *(p++) = LOW(glyph->ins_len);
        memcpy(p, glyph->ins_buf, glyph->ins_len);
        p += glyph->ins_len;
        memcpy(p, glyph->buf + glyph->len1, glyph->len2);
        p += glyph->len2;
      }
      else
      {
        /* composite glyph */
        if (glyph->ins_len)
        {
          *(p + glyph->flags_offset) |= (WE_HAVE_INSTR >> 8);
          p += glyph->len1;
          *(p++) = HIGH(glyph->ins_len);
          *(p++) = LOW(glyph->ins_len);
          memcpy(p, glyph->ins_buf, glyph->ins_len);
          p += glyph->ins_len;
        }
        else
          p += glyph->len1;
      }

      /* pad with zero bytes to have an offset which is a multiple of 4; */
      /* this works even for the last glyph record since the `glyf' */
      /* table length is a multiple of 4 also */
      switch (len % 4)
      {
      case 1:
        *(p++) = 0;
      case 2:
        *(p++) = 0;
      case 3:
        *(p++) = 0;
      default:
        break;
      }
    }
  }

  glyf_table->checksum = TA_table_compute_checksum(glyf_table->buf,
                                                   glyf_table->len);
  glyf_table->processed = 1;

  return TA_Err_Ok;
}

/* end of taglyf.c */
