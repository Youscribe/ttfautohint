// info.cpp

// Copyright (C) 2012 by Werner Lemberg.
//
// This file is part of the ttfautohint library, and may only be used,
// modified, and distributed under the terms given in `COPYING'.  By
// continuing to use, modify, or distribute this file you indicate that you
// have read `COPYING' and understand and accept it fully.
//
// The file `COPYING' mentioned in the previous paragraph is distributed
// with the ttfautohint library.


#include <config.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "info.h"


#define TTFAUTOHINT_STRING "; ttfautohint"
#define TTFAUTOHINT_STRING_WIDE "\0;\0 \0t\0t\0f\0a\0u\0t\0o\0h\0i\0n\0t"

// build string which gets appended to the `Version' field(s)

extern "C" {

// return value 1 means allocation error, value 2 too long a string

int
build_version_string(Info_Data* idata)
{
  char* d;
  char* dw;
  char* s = NULL;
  size_t s_len;
  unsigned char* data_new;
  unsigned short data_new_len;
  char strong[4];
  int count;
  int ret = 0;

  // 128 bytes certainly hold the following options except -X
  data_new = (unsigned char*)realloc(idata->data, 128);
  if (!data_new)
  {
    ret = 1;
    goto Fail;
  }
  idata->data = data_new;

  d = (char*)idata->data;
  d += sprintf(d, TTFAUTOHINT_STRING " (v%s)", VERSION);

  d += sprintf(d, " -l %d", idata->hinting_range_min);
  d += sprintf(d, " -r %d", idata->hinting_range_max);
  d += sprintf(d, " -G %d", idata->hinting_limit);
  d += sprintf(d, " -x %d", idata->increase_x_height);

  count = 0;
  strong[0] = '\0';
  strong[1] = '\0';
  strong[2] = '\0';
  strong[3] = '\0';
  if (idata->gray_strong_stem_width)
    strong[count++] = 'g';
  if (idata->gdi_cleartype_strong_stem_width)
    strong[count++] = 'G';
  if (idata->dw_cleartype_strong_stem_width)
    strong[count++] = 'D';
  d += sprintf(d, " -w \"%s\"", strong);

  if (idata->windows_compatibility)
    d += sprintf(d, " -W");
  if (idata->pre_hinting)
    d += sprintf(d, " -p");
  if (!idata->hint_with_components)
    d += sprintf(d, " -c");
  if (idata->latin_fallback)
    d += sprintf(d, " -f");
  if (idata->symbol)
    d += sprintf(d, " -s");
  if (idata->x_height_snapping_exceptions)
    d += sprintf(d, " -X \"\""); // fill in data later

  idata->data_len = d - (char*)idata->data;

  if (idata->x_height_snapping_exceptions)
  {
    s = number_set_show(idata->x_height_snapping_exceptions, 6, 0x7FFF);
    if (!s)
    {
      ret = 1;
      goto Fail;
    }

    // ensure UTF16-BE version doesn't get too long
    s_len = strlen(s);
    if (s_len > 0xFFFF / 2 - 128)
    {
      ret = 2;
      goto Fail;
    }
  }
  else
    s_len = 0;

  // we now reallocate to the real size
  // (plus one byte so that `sprintf' works)
  data_new_len = idata->data_len + s_len;
  data_new = (unsigned char*)realloc(idata->data, data_new_len + 1);
  if (!data_new)
  {
    ret = 1;
    goto Fail;
  }

  if (idata->x_height_snapping_exceptions)
  {
    // overwrite second doublequote and append it instead
    d = (char*)(data_new + idata->data_len - 1);
    sprintf(d, "%s\"", s);
  }

  idata->data = data_new;
  idata->data_len = data_new_len;

  // prepare UTF16-BE version data
  idata->data_wide_len = 2 * idata->data_len;
  data_new = (unsigned char*)realloc(idata->data_wide,
                                     idata->data_wide_len);
  if (!data_new)
  {
    ret = 1;
    goto Fail;
  }
  idata->data_wide = data_new;

  d = (char*)idata->data;
  dw = (char*)idata->data_wide;
  for (unsigned short i = 0; i < idata->data_len; i++)
  {
    *(dw++) = '\0';
    *(dw++) = *(d++);
  }

Exit:
  free(s);

  return ret;

Fail:
  free(idata->data);
  free(idata->data_wide);

  idata->data = NULL;
  idata->data_wide = NULL;
  idata->data_len = 0;
  idata->data_wide_len = 0;

  goto Exit;
}


int
info(unsigned short platform_id,
     unsigned short encoding_id,
     unsigned short /* language_id */,
     unsigned short name_id,
     unsigned short* len,
     unsigned char** str,
     void* user)
{
  Info_Data* idata = (Info_Data*)user;
  unsigned char ttfautohint_string[] = TTFAUTOHINT_STRING;
  unsigned char ttfautohint_string_wide[] = TTFAUTOHINT_STRING_WIDE;

  // we use memmem, so don't count the trailing \0 character
  size_t ttfautohint_string_len = sizeof (TTFAUTOHINT_STRING) - 1;
  size_t ttfautohint_string_wide_len = sizeof (TTFAUTOHINT_STRING_WIDE) - 1;

  unsigned char* v;
  unsigned short v_len;
  unsigned char* s;
  size_t s_len;
  size_t offset;

  // if it is a version string, append our data
  if (name_id != 5)
    return 0;

  if (platform_id == 1
      || (platform_id == 3 && !(encoding_id == 1
                                || encoding_id == 10)))
  {
    // one-byte or multi-byte encodings
    v = idata->data;
    v_len = idata->data_len;
    s = ttfautohint_string;
    s_len = ttfautohint_string_len;
    offset = 2;
  }
  else
  {
    // (two-byte) UTF-16BE for everything else
    v = idata->data_wide;
    v_len = idata->data_wide_len;
    s = ttfautohint_string_wide;
    s_len = ttfautohint_string_wide_len;
    offset = 4;
  }

  // if we already have an ttfautohint info string,
  // remove it up to a following `;' character (or end of string)
  unsigned char* s_start = (unsigned char*)memmem(*str, *len, s, s_len);
  if (s_start)
  {
    unsigned char* s_end = s_start + offset;
    unsigned char* limit = *str + *len;

    while (s_end < limit)
    {
      if (*s_end == ';')
      {
        if (offset == 2)
          break;
        else
        {
          if (*(s_end - 1) == '\0') // UTF-16BE
          {
            s_end--;
            break;
          }
        }
      }

      s_end++;
    }

    while (s_end < limit)
      *s_start++ = *s_end++;

    *len -= s_end - s_start;
  }

  // do nothing if the string would become too long
  if (*len > 0xFFFF - v_len)
    return 0;

  unsigned short len_new = *len + v_len;
  unsigned char* str_new = (unsigned char*)realloc(*str, len_new);
  if (!str_new)
    return 1;

  *str = str_new;
  memcpy(*str + *len, v, v_len);
  *len = len_new;

  return 0;
}

} // extern "C"

// end of info.cpp
