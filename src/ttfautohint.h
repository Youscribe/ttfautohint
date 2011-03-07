/* ttfautohint.h */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#include <stdio.h>


/* error type */

typedef int TA_Error;


/* errors */

#define TA_Err_Ok 0;


/*
 * Read a TrueType font, remove existing bytecode (in the SFNT tables
 * `prep', `fpgm', `cvt ', and `glyf'), and write a new TrueType font with
 * new bytecode based on the autohinting of the FreeType library.
 *
 * It expects two file handles: `in', opened for binary reading, which
 * points to the input TrueType font, and `out', opened for binary writing,
 * which points to the output TrueType font.
 *
 */

TA_Error
TTF_autohint(FILE *in,
             FILE *out);

/* end of ttfautohint.h */
