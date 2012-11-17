/* numberset.h */

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


#ifndef __NUMBERSET_H__
#define __NUMBERSET_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A structure defining an integer range, to be used as a linked list.  It
 * gets allocated by a successful call to `parse_number_set'.  Use
 * `number_set_free' to deallocate it.
 */

typedef struct number_range_
{
  int start;
  int end;

  struct number_range_* next;
} number_range;


/*
 * Parse a description in string `s' for a set of non-negative integers
 * within the limits given by the input parameters `min' and `max', and
 * which consists of the following ranges, separated by commas (`n' and `m'
 * are non-negative integers):
 *
 *   -n       min <= x <= n
 *   n        x = n; this is a shorthand for `n-n'
 *   n-m      n <= x <= m (or  m <= x <= n  if  m < n)
 *   m-       m <= x <= max
 *   -        min <= x <= max
 *
 * Superfluous commas are ignored, as is whitespace around numbers, dashes,
 * and commas.  The ranges must be ordered, without overlaps.  As a
 * consequence, `-n' and `m-' can occur at most once and must be then the
 * first and last range, respectively; similarly, `-' cannot be paired with
 * any other range.
 *
 * In the following examples, `min' is 4 and `max' is 12:
 *
 *   -                ->    4, 5, 6, 7, 8, 9, 10, 11, 12
 *   -3, 5-           ->    invalid first range
 *   4, 6-8, 10-      ->    4, 6, 7, 8, 10, 11, 12
 *   4-8, 6-10        ->    invalid overlapping ranges
 *
 * In case of success (this is, the number set description in `s' is valid)
 * the return value is a pointer to the final zero byte in string `s'.  In
 * case of an error, the return value is a pointer to the beginning position
 * of the offending range in string `s'.
 *
 * If the user provides a non-NULL `number_set' value, `number_set_parse'
 * stores a linked list of ordered number ranges in `*number_set', allocated
 * with `malloc'.  If there is no range at all (for example, an empty string
 * or whitespace and commas only) no data gets allocated, and `*number_set'
 * is set to NULL.  In case of error, `*number_set' returns an error code;
 * you should use the following macros to compare with.
 *
 *   NUMBERSET_INVALID_CHARACTER   invalid character in description string
 *   NUMBERSET_OVERFLOW            numerical overflow
 *   NUMBERSET_INVALID_RANGE       invalid range, exceeding `min' or `max'
 *   NUMBERSET_OVERLAPPING_RANGES  overlapping ranges
 *   NUMBERSET_ALLOCATION_ERROR    allocation error
 *
 * Note that a negative value for `min' is replaced with zero, and a
 * negative value for `max' with the largest representable integer, INT_MAX.
 */

#define NUMBERSET_INVALID_CHARACTER (number_range*)-1
#define NUMBERSET_OVERFLOW (number_range*)-2
#define NUMBERSET_INVALID_RANGE (number_range*)-3
#define NUMBERSET_OVERLAPPING_RANGES (number_range*)-4
#define NUMBERSET_ALLOCATION_ERROR (number_range*)-5

const char*
number_set_parse(const char* s,
                 number_range** number_set,
                 int min,
                 int max);


/*
 * Free the allocated data in `number_set'.
 */

void
number_set_free(number_range* number_set);


/*
 * Return a string representation of `number_set', viewed through a
 * `window', so to say, spanned up by the parameters `min' and `max'.  After
 * use, the string should be deallocated with a call to `free'.  In case of
 * an allocation error, the return value is NULL.
 *
 * Note that a negative value for `min' is replaced with zero, and a
 * negative value for `max' with the largest representable integer, INT_MAX.
 */

char*
number_set_show(number_range* number_set,
                int min,
                int max);


/*
 * Return value 1 if `number' is element of `number_set', zero otherwise.
 */

int
number_set_is_element(number_range* number_set,
                      int number);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __NUMBERSET_H__ */

/* end of numberset.h */
