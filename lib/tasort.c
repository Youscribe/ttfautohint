/* tasort.c */

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


/* originally file `afangles.c' (2011-Mar-28) from FreeType */

/* heavily modified 2011 by Werner Lemberg <wl@gnu.org> */

#include "tatypes.h"
#include "tasort.h"


/* two bubble sort routines */

void
ta_sort_pos(FT_UInt count,
            FT_Pos* table)
{
  FT_UInt i;
  FT_UInt j;
  FT_Pos swap;


  for (i = 1; i < count; i++)
  {
    for (j = i; j > 0; j--)
    {
      if (table[j] >= table[j - 1])
        break;

      swap = table[j];
      table[j] = table[j - 1];
      table[j - 1] = swap;
    }
  }
}


void
ta_sort_and_quantize_widths(FT_UInt* count,
                            TA_Width table,
                            FT_Pos threshold)
{
  FT_UInt i;
  FT_UInt j;
  FT_UInt cur_idx;
  FT_Pos cur_val;
  FT_Pos sum;
  TA_WidthRec swap;


  if (*count == 1)
    return;

  /* sort */
  for (i = 1; i < *count; i++)
  {
    for (j = i; j > 0; j--)
    {
      if (table[j].org >= table[j - 1].org)
        break;

      swap = table[j];
      table[j] = table[j - 1];
      table[j - 1] = swap;
    }
  }

  cur_idx = 0;
  cur_val = table[cur_idx].org;

  /* compute and use mean values for clusters not larger than `threshold'; */
  /* this is very primitive and might not yield the best result, */
  /* but normally, using reference character `o', `*count' is 2, */
  /* so the code below is fully sufficient */
  for (i = 1; i < *count; i++)
  {
    if (table[i].org - cur_val > threshold
        || i == *count - 1)
    {
      sum = 0;

      /* fix loop for end of array */
      if (table[i].org - cur_val <= threshold
          && i == *count - 1)
        i++;

      for (j = cur_idx; j < i; j++)
      {
        sum += table[j].org;
        table[j].org = 0;
      }
      table[cur_idx].org = sum / j;

      if (i < *count - 1)
      {
        cur_idx = i + 1;
        cur_val = table[cur_idx].org;
      }
    }
  }

  cur_idx = 1;

  /* compress array to remove zero values */
  for (i = 1; i < *count; i++)
  {
    if (table[i].org)
      table[cur_idx++] = table[i];
  }

  *count = cur_idx;
}

/* end of tasort.c */
