/* numberset.c */

/*
 * Copyright (C) 2012 by Werner Lemberg.
 *
 * This file is part of the ttfautohint library, and may only be used,
 * modified, and distributed under the terms given in `COPYING'.  By
 * continuing to use, modify, or distribute this file you indicate that you
 * have read `COPYING' and understand and accept it fully.
 *
 * The file `COPYING' mentioned in the previous paragraph is distributed
 * with the ttfautohint library.
 */


#include <stdlib.h>
#include <ctype.h>
#include <limits.h>

#include "numberset.h"


const char*
number_set_parse(const char* s,
                 number_range** number_set,
                 int min,
                 int max)
{
  number_range* cur = NULL;
  number_range* new_range;
  number_range* tmp;

  const char* last_pos = s;
  int last_end = -1;
  number_range* error_code = NULL;


  if (min < 0)
    min = 0;
  if (max < 0)
    max = INT_MAX;

  for (;;)
  {
    int digit;
    int n = -1;
    int m = -1;


    while (isspace(*s))
      s++;

    if (*s == ',')
    {
      s++;
      continue;
    }
    else if (*s == '-')
    {
      last_pos = s;
      n = min;
    }
    else if (isdigit(*s))
    {
      last_pos = s;
      n = 0;
      do
      {
        digit = *s - '0';
        if (n > INT_MAX / 10
            || (n == INT_MAX / 10 && digit > 5))
        {
          error_code = NUMBERSET_OVERFLOW;
          break;
        }

        n = n * 10 + digit;
        s++;
      } while (isdigit(*s));

      while (isspace(*s))
        s++;
    }
    else if (*s == '\0')
      break; /* end of data */
    else
    {
      error_code = NUMBERSET_INVALID_CHARACTER;
      break;
    }

    if (*s == '-')
    {
      s++;

      while (isspace(*s))
        s++;

      if (isdigit(*s))
      {
        m = 0;
        do
        {
          digit = *s - '0';
          if (m > INT_MAX / 10
              || (m == INT_MAX / 10 && digit > 5))
          {
            error_code = NUMBERSET_OVERFLOW;
            break;
          }

          m = m * 10 + digit;
          s++;
        } while (isdigit(*s));
      }
    }
    else
      m = n;

    if (m == -1)
      m = max;

    if (m < n)
    {
      int tmp;


      tmp = n;
      n = m;
      m = tmp;
    }

    if (n < min || m > max)
    {
      error_code = NUMBERSET_INVALID_RANGE;
      break;
    }

    if (last_end >= n)
    {
      error_code = NUMBERSET_OVERLAPPING_RANGES;
      break;
    }

    if (cur
        && last_end + 1 == n)
    {
      /* merge adjacent ranges */
      cur->end = m;
    }
    else
    {
      if (number_set)
      {
        new_range = (number_range*)malloc(sizeof (number_range));
        if (!new_range)
        {
          error_code = NUMBERSET_ALLOCATION_ERROR;
          break;
        }

        /* prepend new range to list */
        new_range->start = n;
        new_range->end = m;
        new_range->next = cur;
        cur = new_range;
      }

      last_end = m;
    }
  } /* end of loop */

  if (error_code)
  {
    /* deallocate data */
    while (cur)
    {
      tmp = cur;
      cur = cur->next;
      free(tmp);
    }

    s = last_pos;
    if (number_set)
      *number_set = error_code;
  }
  else
  {
    /* success; now reverse list to have elements in ascending order */
    number_range* list = NULL;


    while (cur)
    {
      tmp = cur;
      cur = cur->next;
      tmp->next = list;
      list = tmp;
    }

    if (number_set)
      *number_set = list;
  }

  return s;
}


void
number_set_free(number_range* number_set)
{
  number_range* nr = number_set;
  number_range* tmp;


  while (nr)
  {
    tmp = nr;
    nr = nr->next;
    free(tmp);
  }
}


int
number_set_is_element(number_range* number_set,
                      int number)
{
  number_range* nr = number_set;


  while (nr)
  {
    if (number < nr->start)
      return 0;
    if (nr->start <= number
        && number <= nr->end)
      return 1;
    nr = nr->next;
  }

  return 0;
}

/* end of numberset.c */
