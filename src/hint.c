/* hint.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */


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
usage(char* program_name,
      int is_error)
{
  FILE* handle = is_error ? stderr : stdout;


  fprintf(handle, "Usage: %s [options] <in-font> <out-font>\n",
                  program_name);

  if (is_error)
    exit(EXIT_FAILURE);
  else
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


  while (1)
  {
    static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"hinting-range-max", required_argument, 0, 'r'},
      {"hinting-range-min", required_argument, 0, 'l'},
      {"ignore-permissions", no_argument, 0, 'i'},
      {"latin-fallback", no_argument, 0, 'f'},
      {"pre-hinting", no_argument, 0, 'p'},
      {"verbose", no_argument, 0, 'V'},
      {"version", no_argument, 0, 'v'},
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
      break;

    case 'h':
      usage(argv[0], 0);
      break;

    case 'i':
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
      break;

    case 'V':
      progress_func = progress;
      break;

    case 'v':
      break;

    case 'x':
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
    fprintf(stderr, "The hinting range minimum must be at least 2.\n");
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
    usage(argv[0], 1);

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
                       "progress-callback, progress-callback-data",
                       in, out,
                       hinting_range_min, hinting_range_max,
                       progress_func, &progress_data);
  if (error)
  {
    fprintf(stderr, "Error code `0x%02x' while autohinting font\n", error);
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}

/* end of hint.c */
