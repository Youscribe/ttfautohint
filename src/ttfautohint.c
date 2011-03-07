/* ttfautohint.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#include <config.h>
#include <stdio.h>

#include "ttfautohint.h"

TA_Error
TTF_autohint(FILE *in,
             FILE *out)
{
  /* load font into memory */
  /* split font into SFNT tables */
  /* strip `fpgm' table */
  /* strip `prep' table */
  /* strip `cvt ' table */

  /* compute global hints */
  /* construct `fpgm' table */
  /* construct `prep' table */
  /* construct `cvt ' table */

  /* split `glyf' table */
  /* handle all glyphs in a loop */
    /* strip bytecode */
    /* hint the glyph */
    /* construct bytecode */

  /* construct `glyf' table */
  /* build font from SFNT tables */
  /* write font from memory */  

  return TA_Err_Ok;
}

/* end of ttfautohint.c */
