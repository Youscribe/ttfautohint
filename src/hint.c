/* hint.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */


/* This test program is a wrapper for `TTF_autohint'. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "ttfautohint.h"


int
main(int argc,
     char *argv[])
{
  FILE *in;
  FILE *out;
  TA_Error error;


  if (argc != 3)
  {
    fprintf(stderr, "Usage: %s <in-font> <out-font>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  in = fopen(argv[1], "rb");
  if (!in)
  {
    fprintf(stderr, "The following error occurred while opening font `%s':\n"
                    "\n"
                    "  %s\n",
                    argv[1], strerror(errno));
    exit(EXIT_FAILURE);
  }

  out = fopen(argv[2], "wb");
  if (!out)
  {
    fprintf(stderr, "The following error occurred while opening font `%s':\n"
                    "\n"
                    "  %s\n",
                    argv[2], strerror(errno));
    exit(EXIT_FAILURE);
  }

  error = TTF_autohint((void*)in, (void*)out, NULL);
  if (error)
  {
    fprintf(stderr, "Error code `0x%02x' while autohinting font\n", error);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}

/* end of hint.c */
