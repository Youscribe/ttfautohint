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
  FT_UShort component;
  FT_UShort* components_new;

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

    /* add component to list */
    component = *(p++) << 8;
    component += *(p++);

    glyph->num_components++;
    components_new = (FT_UShort*)realloc(glyph->components,
                                         glyph->num_components
                                         * sizeof (FT_UShort));
    if (!components_new)
    {
      glyph->num_components--;
      return FT_Err_Out_Of_Memory;
    }
    else
      glyph->components = components_new;

    glyph->components[glyph->num_components - 1] = component;

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


static FT_Error
TA_iterate_composite_glyph(glyf_Data* data,
                           FT_UShort* components,
                           FT_UShort num_components,
                           FT_UShort** endpoints,
                           FT_UShort* num_endpoints)
{
  FT_UShort i;


  for (i = 0; i < num_components; i++)
  {
    GLYPH* glyph;
    FT_UShort component = components[i];
    FT_Error error;


    if (component >= data->num_glyphs)
      return FT_Err_Invalid_Table;

    glyph = &data->glyphs[component];

    if (glyph->num_components)
    {
      error = TA_iterate_composite_glyph(data,
                                         glyph->components,
                                         glyph->num_components,
                                         endpoints,
                                         num_endpoints);
      if (error)
        return error;
    }
    else
    {
      FT_UShort num_contours;
      FT_UShort endpoint;
      FT_UShort* endpoints_new;


      /* collect end points of simple glyphs */

      num_contours = glyph->buf[0] << 8;
      num_contours += glyph->buf[1];
      endpoint = glyph->buf[10 + (num_contours - 1) * 2] << 8;
      endpoint += glyph->buf[10 + (num_contours - 1) * 2 + 1];

      (*num_endpoints)++;
      endpoints_new = (FT_UShort*)realloc(*endpoints,
                                          *num_endpoints
                                          * sizeof (FT_UShort));
      if (!endpoints_new)
      {
        (*num_endpoints)--;
        return FT_Err_Out_Of_Memory;
      }
      else
        *endpoints = endpoints_new;

      (*endpoints)[*num_endpoints - 1] = endpoint;
      if (*num_endpoints > 1)
        (*endpoints)[*num_endpoints - 1] += (*endpoints)[*num_endpoints - 2] + 1;
    }
  }

  return TA_Err_Ok;
}


static FT_Error
TA_compute_composite_endpoints(glyf_Data* data)
{
  FT_UShort i;


  for (i = 0; i < data->num_glyphs; i++)
  {
    GLYPH* glyph = &data->glyphs[i];


    if (glyph->num_components)
    {
      FT_Error error;


      error = TA_iterate_composite_glyph(data,
                                         glyph->components,
                                         glyph->num_components,
                                         &glyph->endpoints,
                                         &glyph->num_endpoints);
      if (error)
        return error;
    }
  }

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

  FT_Error error;


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
        error = TA_glyph_parse_composite(glyph, buf, len);
      else
        error = TA_glyph_parse_simple(glyph, buf, num_contours, len);
      if (error)
        return error;
    }
  }

  error = TA_compute_composite_endpoints(data);
  if (error)
    return error;

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


static FT_Error
TA_create_glyph_data(FT_Outline* outline,
                     GLYPH* glyph)
{
  FT_Error error = TA_Err_Ok;

  FT_Pos xmin, ymin;
  FT_Pos xmax, ymax;

  FT_Byte header[10];
  FT_Byte* flags = NULL;
  FT_Byte* flagsp;
  FT_Byte oldf, f;
  FT_Byte* x = NULL;
  FT_Byte* xp;
  FT_Byte* y = NULL;
  FT_Byte* yp;

  FT_Pos lastx, lasty;

  FT_Short i;
  FT_Byte* p;


  if (!outline->n_contours)
    return TA_Err_Ok; /* empty glyph */

  /* in case of success, all non-local allocated arrays are */
  /* linked and eventually freed in `TA_font_unload' */

  glyph->buf = NULL;

  /* we use `calloc' since we rely on the array */
  /* being initialized to zero; */
  /* additionally, we need one more byte for a test after the loop */
  flags = (FT_Byte*)calloc(1, outline->n_points + 1);
  if (!flags)
  {
    error = FT_Err_Out_Of_Memory;
    goto Exit;
  }

  /* we have either one-byte or two-byte elements */
  x = (FT_Byte*)malloc(2 * outline->n_points);
  if (!x)
  {
    error = FT_Err_Out_Of_Memory;
    goto Exit;
  }

  y = (FT_Byte*)malloc(2 * outline->n_points);
  if (!y)
  {
    error = FT_Err_Out_Of_Memory;
    goto Exit;
  }

  flagsp = flags;
  xp = x;
  yp = y;
  xmin = xmax = (outline->points[0].x + 32) >> 6;
  ymin = ymax = (outline->points[0].y + 32) >> 6;
  lastx = 0;
  lasty = 0;
  oldf = 0x80; /* start with an impossible value */

  /* convert the FreeType representation of the glyph's outline */
  /* into the representation format of the `glyf' table */
  for (i = 0; i < outline->n_points; i++)
  {
    FT_Pos xcur = (outline->points[i].x + 32) >> 6;
    FT_Pos ycur = (outline->points[i].y + 32) >> 6;

    FT_Pos xdelta = xcur - lastx;
    FT_Pos ydelta = ycur - lasty;


    /* we are only interested in bit 0 of the `tags' array */
    f = outline->tags[i] & ON_CURVE;

    /* x value */

    if (xdelta == 0)
      f |= SAME_X;
    else
    {
      if (xdelta < 256 && xdelta > -256)
      {
        f |= X_SHORT_VECTOR;

        if (xdelta < 0)
          xdelta = -xdelta;
        else
          f |= SAME_X;

        *(xp++) = (FT_Byte)xdelta;
      }
      else
      {
        *(xp++) = HIGH(xdelta);
        *(xp++) = LOW(xdelta);
      }
    }

    /* y value */

    if (ydelta == 0)
      f |= SAME_Y;
    else
    {
      if (ydelta < 256 && ydelta > -256)
      {
        f |= Y_SHORT_VECTOR;

        if (ydelta < 0)
          ydelta = -ydelta;
        else
          f |= SAME_Y;

        *(yp++) = (FT_Byte)ydelta;
      }
      else
      {
        *(yp++) = HIGH(ydelta);
        *(yp++) = LOW(ydelta);
      }
    }

    if (f == oldf)
    {
      /* set repeat flag */
      *(flagsp - 1) |= REPEAT;

      if (*flagsp == 255)
      {
        /* we can only handle 256 repetitions at once, */
        /* so use a new counter */
        flagsp++;
        *(flagsp++) = f;
      }
      else
        *flagsp += 1; /* increase repetition counter */
    }
    else
    {
      if (*flagsp)
        flagsp++; /* skip repetition counter */
      *(flagsp++) = f;
      oldf = f;
    }

    if (xcur > xmax)
      xmax = xcur;
    if (ycur > ymax)
      ymax = ycur;
    if (xcur < xmin)
      xmin = xcur;
    if (ycur < ymin)
      ymin = ycur;

    lastx = xcur;
    lasty = ycur;
  }

  /* if the last byte was a repetition counter, */
  /* we must increase by one to get the correct array size */
  if (*flagsp)
    flagsp++;

  header[0] = HIGH(outline->n_contours);
  header[1] = LOW(outline->n_contours);
  header[2] = HIGH(xmin);
  header[3] = LOW(xmin);
  header[4] = HIGH(ymin);
  header[5] = LOW(ymin);
  header[6] = HIGH(xmax);
  header[7] = LOW(xmax);
  header[8] = HIGH(ymax);
  header[9] = LOW(ymax);

  /* concatenate all arrays and fill needed GLYPH structure elements */

  glyph->len1 = 10 + 2 * outline->n_contours;
  glyph->len2 = (flagsp - flags) + (xp - x) + (yp - y);

  glyph->buf = (FT_Byte*)malloc(glyph->len1 + glyph->len2);
  if (!glyph->buf)
  {
    error = FT_Err_Out_Of_Memory;
    goto Exit;
  }

  p = glyph->buf;
  memcpy(p, header, 10);
  p += 10;

  glyph->ins_len = 0;
  glyph->ins_buf = NULL;

  for (i = 0; i < outline->n_contours; i++)
  {
    *(p++) = HIGH(outline->contours[i]);
    *(p++) = LOW(outline->contours[i]);
  }

  memcpy(p, flags, flagsp - flags);
  p += flagsp - flags;
  memcpy(p, x, xp - x);
  p += xp - x;
  memcpy(p, y, yp - y);

Exit:
  free(flags);
  free(x);
  free(y);

  return error;
}


/* We hint each glyph at EM size and construct a new `glyf' table. */
/* Some fonts need this; in particular, */
/* there are CJK fonts which use hints to scale and position subglyphs. */
/* As a consequence, there are no longer composite glyphs. */

FT_Error
TA_sfnt_create_glyf_data(SFNT* sfnt,
                         FONT* font)
{
  SFNT_Table* glyf_table = &font->tables[sfnt->glyf_idx];
  FT_Face face = sfnt->face;
  FT_Error error;

  glyf_Data* data;

  FT_UShort i;


  /* in case of success, all allocated arrays are */
  /* linked and eventually freed in `TA_font_unload' */

  /* nothing to do if table has already been created */
  if (glyf_table->data)
    return TA_Err_Ok;

  data = (glyf_Data*)calloc(1, sizeof (glyf_Data));
  if (!data)
    return FT_Err_Out_Of_Memory;

  glyf_table->data = data;

  data->num_glyphs = face->num_glyphs;
  data->glyphs = (GLYPH*)calloc(1, data->num_glyphs * sizeof (GLYPH));
  if (!data->glyphs)
    return FT_Err_Out_Of_Memory;

  /* XXX: Make size configurable */
  /* we use the EM size */
  /* so that the resulting coordinates can be used without transformation */
  error = FT_Set_Char_Size(face, face->units_per_EM * 64, 0, 72, 0);
  if (error)
    return error;

  /* loop over all glyphs in font face */
  for (i = 0; i < data->num_glyphs; i++)
  {
    GLYPH* glyph = &data->glyphs[i];


    error = FT_Load_Glyph(face, i, FT_LOAD_NO_BITMAP | FT_LOAD_NO_AUTOHINT);
    if (error)
      return error;

    error = TA_create_glyph_data(&face->glyph->outline, glyph);
    if (error)
      return error;
  }

  return TA_Err_Ok;
}

/* end of taglyf.c */
