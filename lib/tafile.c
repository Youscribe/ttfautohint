/* tafile.c */

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


#define BUF_SIZE 1024

FT_Error
TA_font_file_read(FONT* font,
                  FILE* in_file)
{
  FT_Byte buf[BUF_SIZE];
  size_t in_len = 0;
  size_t read_bytes;


  font->in_buf = (FT_Byte*)malloc(BUF_SIZE);
  if (!font->in_buf)
    return FT_Err_Out_Of_Memory;

  while ((read_bytes = fread(buf, 1, BUF_SIZE, in_file)) > 0)
  {
    FT_Byte* in_buf_new;


    in_buf_new = (FT_Byte*)realloc(font->in_buf, in_len + read_bytes);
    if (!in_buf_new)
      return FT_Err_Out_Of_Memory;
    else
      font->in_buf = in_buf_new;

    memcpy(font->in_buf + in_len, buf, read_bytes);

    in_len += read_bytes;
  }

  if (ferror(in_file))
    return FT_Err_Invalid_Stream_Read;

  /* a valid TTF can never be that small */
  if (in_len < 100)
    return TA_Err_Invalid_Font_Type;

  font->in_len = in_len;

  return TA_Err_Ok;
}


FT_Error
TA_font_file_write(FONT* font,
                   FILE* out_file)
{
  if (fwrite(font->out_buf, 1, font->out_len, out_file) != font->out_len)
    return TA_Err_Invalid_Stream_Write;

  return TA_Err_Ok;
}

/* end of tafile.c */
