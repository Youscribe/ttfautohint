/* hint.c */

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


/* This test program is a wrapper for `TTF_autohint'. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

#include "ttfautohint.h"


typedef struct Progress_Data_ {
  long last_sfnt;
  int begin;
  int last_percent;
} Progress_Data;


void
progress(long curr_idx,
         long num_glyphs,
         long curr_sfnt,
         long num_sfnts,
         void *user)
{
  Progress_Data* data = (Progress_Data*)user;
  int curr_percent;
  int curr_diff;


  if (num_sfnts > 1 && curr_sfnt != data->last_sfnt)
  {
    fprintf(stderr, "subfont %ld of %ld\n", curr_sfnt + 1, num_sfnts);
    data->last_sfnt = curr_sfnt;
    data->begin = 1;
  }

  if (data->begin)
  {
    fprintf(stderr, "  %ld glyphs\n"
                    "   ", num_glyphs);
    data->begin = 0;
  }

  /* print progress approx. all 10% */
  curr_percent = curr_idx * 100 / num_glyphs;
  curr_diff = curr_percent - data->last_percent;
  if (curr_diff >= 10)
  {
    fprintf(stderr, " %d%%", curr_percent);
    data->last_percent = curr_percent - curr_percent % 10;
  }

  if (curr_idx + 1 == num_glyphs)
    fprintf(stderr, "\n");
}


static void
show_help(char* program_name,
          int is_error)
{
  FILE* handle = is_error ? stderr : stdout;


  fprintf(handle,
"Usage: %s [OPTION] IN-FILE OUT-FILE\n"
"Replace hints in TrueType font IN-FILE and write output to OUT-FILE.\n"
"The new hints are based on FreeType's autohinter.\n"
"\n"
"This program is a simple front-end to the `ttfautohint' library.\n"
"\n",
          program_name);
  fprintf(handle,
"Options:\n"
"  -f, --latin-fallback       set fallback script to latin\n"
"  -h, --help                 display this help and exit\n"
"  -i, --ignore-permissions   override font license restrictions\n"
"  -l, --hinting-range-min=N  the minimum ppem value for generating hints\n"
"  -p, --pre-hinting          apply original hints before generating hints\n");
  fprintf(handle,
"  -r, --hinting-range-max=N  the maximum ppem value for generating hints\n"
"  -v, --verbose              show progress information\n"
"  -V, --version              print version information and exit\n"
"  -x, --x-height-snapping-exceptions=STRING\n"
"                             specify a comma-separated list of x-height\n"
"                             snapping exceptions ranges and single values\n"
"\n");
  fprintf(handle,
"The program accepts both TTF and TTC files as input.\n"
"The `gasp' table of OUT-FILE enables grayscale hinting for all sizes.\n"
"Use option -i only if you have a legal permission to modify the font.\n"
"If option -f is not set, glyphs not in the latin range stay unhinted.\n"
"The used ppem value for option -p is FUnits per em, normally 2048.\n"
"\n"
"Report bugs to: freetype-devel@nongnu.org\n"
"FreeType home page: <http://www.freetype.org>\n");

  if (is_error)
    exit(EXIT_FAILURE);
  else
    exit(EXIT_SUCCESS);
}


static void
show_version(void)
{
  fprintf(stdout,

"ttfautohint version " VERSION "\n"
"Copyright (C) 2011 Werner Lemberg <wl@gnu.org>.\n"
"License: FreeType License (FTL) or GNU GPLv2.\n"
"This is free software: you are free to change and redistribute it.\n"
"There is NO WARRANTY, to the extent permitted by law.\n"

  );

  exit(EXIT_SUCCESS);
}


int
main(int argc,
     char** argv)
{
  int c;

  FILE *in;
  FILE *out;
  TA_Error error;

  Progress_Data progress_data = {-1, 1, 0};
  TA_Progress_Func progress_func = NULL;

  int hinting_range_min = 0;
  int hinting_range_max = 0;
  int have_hinting_range_min = 0;
  int have_hinting_range_max = 0;

  int ignore_permissions = 0;
  int latin_fallback = 0;


  while (1)
  {
    static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"hinting-range-max", required_argument, 0, 'r'},
      {"hinting-range-min", required_argument, 0, 'l'},
      {"ignore-permissions", no_argument, 0, 'i'},
      {"latin-fallback", no_argument, 0, 'f'},
      {"pre-hinting", no_argument, 0, 'p'},
      {"verbose", no_argument, 0, 'v'},
      {"version", no_argument, 0, 'V'},
      {"x-height-snapping-exceptions", required_argument, 0, 'x'},
      {0, 0, 0, 0}
    };

    int option_index = 0;


    c = getopt_long(argc, argv, "fhil:r:pVvx:", long_options, &option_index);
    if (c == -1)
      break;

    switch (c)
    {
    case 'f':
      latin_fallback = 1;
      break;

    case 'h':
      show_help(argv[0], 0);
      break;

    case 'i':
      ignore_permissions = 1;
      break;

    case 'l':
      hinting_range_min = atoi(optarg);
      have_hinting_range_min = 1;
      break;

    case 'r':
      hinting_range_max = atoi(optarg);
      have_hinting_range_max = 1;
      break;

    case 'p':
      fprintf(stderr, "Option `-p' not implemented yet\n");
      break;

    case 'v':
      progress_func = progress;
      break;

    case 'V':
      show_version();
      break;

    case 'x':
      fprintf(stderr, "Option `-x' not implemented yet\n");
      break;

    default:
      exit(EXIT_FAILURE);
    }
  }

  if (!have_hinting_range_min)
    hinting_range_min = 8;
  if (!have_hinting_range_max)
    hinting_range_max = 1000;

  if (hinting_range_min < 2)
  {
    fprintf(stderr, "The hinting range minimum must be at least 2\n");
    exit(EXIT_FAILURE);
  }
  if (hinting_range_max < hinting_range_min)
  {
    fprintf(stderr, "The hinting range maximum must not be smaller"
                    " than the minimum (%d)\n",
                    hinting_range_min);
    exit(EXIT_FAILURE);
  }

  if (argc - optind != 2)
    show_help(argv[0], 1);

  in = fopen(argv[optind], "rb");
  if (!in)
  {
    fprintf(stderr, "The following error occurred while opening font `%s':\n"
                    "\n"
                    "  %s\n",
                    argv[optind], strerror(errno));
    exit(EXIT_FAILURE);
  }

  out = fopen(argv[optind + 1], "wb");
  if (!out)
  {
    fprintf(stderr, "The following error occurred while opening font `%s':\n"
                    "\n"
                    "  %s\n",
                    argv[optind + 1], strerror(errno));
    exit(EXIT_FAILURE);
  }

  error = TTF_autohint("in-file, out-file,"
                       "hinting-range-min, hinting-range-max,"
                       "progress-callback, progress-callback-data,"
                       "ignore-permissions, fallback-script",
                       in, out,
                       hinting_range_min, hinting_range_max,
                       progress_func, &progress_data,
                       ignore_permissions, latin_fallback);

  if (error)
  {
    if (error == TA_Err_Invalid_FreeType_Version)
      fprintf(stderr,
              "FreeType version 2.4.5 or higher is needed.\n"
              "Perhaps using a wrong FreeType DLL?\n");
    else if (error == TA_Err_Missing_Legal_Permission)
      fprintf(stderr,
              "Bit 1 in the `fsType' field of the `OS/2' table is set:\n"
              "This font must not be modified"
                " without permission of the legal owner.\n"
              "Use command line option `-i' to continue"
                " if you have such a permission.\n");
    else if (error == TA_Err_Missing_Unicode_CMap)
      fprintf(stderr,
              "No Unicode character map.\n");
    else if (error == TA_Err_Missing_Glyph)
      fprintf(stderr,
              "No glyph for the key character"
                " to derive standard width and height.\n"
              "For the latin script, this key character is `o' (U+006F).\n");
    else
      fprintf(stderr,
              "Error code `0x%02x' while autohinting font\n", error);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}

/* end of hint.c */
