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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <numberset.h>


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
  int t;
  number_range* error_code = NULL;


  if (min < 0)
    min = 0;
  if (max < 0)
    max = INT_MAX;
  if (min > max)
  {
    t = min;
    min = max;
    max = t;
  }

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

      if (error_code)
        break;

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

        if (error_code)
          break;
      }
    }
    else
      m = n;

    if (m == -1)
      m = max;

    if (m < n)
    {
      t = n;
      n = m;
      m = t;
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
    }

    last_end = m;
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


char*
number_set_show(number_range* number_set,
                int min,
                int max)
{
  char* s;
  char* s_new;
  size_t s_len;
  size_t s_len_new;

  number_range* nr = number_set;

  char tmp[256];
  int tmp_len;
  int t;
  const char *comma;


  if (min < 0)
    min = 0;
  if (max < 0)
    max = INT_MAX;
  if (min > max)
  {
    t = min;
    min = max;
    max = t;
  }

  /* we return an empty string for an empty number set */
  /* (this is, number_set == NULL or unsuitable `min' and `max' values) */
  s = (char*)malloc(1);
  if (!s)
    return NULL;
  *s = '\0';

  s_len = 1;

  while (nr)
  {
    if (nr->start > max)
      return s;
    if (nr->end < min)
      goto Again;

    comma = (s_len == 1) ? "" : ", ";

    if (nr->start <= min
        && nr->end >= max)
      tmp_len = sprintf(tmp, "-");
    else if (nr->start <= min)
      tmp_len = sprintf(tmp, "-%i",
                             nr->end);
    else if (nr->end >= max)
      tmp_len = sprintf(tmp, "%s%i-",
                             comma, nr->start);
    else
    {
      if (nr->start == nr->end)
        tmp_len = sprintf(tmp, "%s%i",
                               comma, nr->start);
      else
        tmp_len = sprintf(tmp, "%s%i-%i",
                               comma, nr->start, nr->end);
    }

    s_len_new = s_len + tmp_len;
    s_new = (char*)realloc(s, s_len_new);
    if (!s_new)
    {
      free(s);
      return NULL;
    }
    strcpy(s_new + s_len - 1, tmp);
    s_len = s_len_new;
    s = s_new;

  Again:
    nr = nr->next;
  }

  return s;
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
