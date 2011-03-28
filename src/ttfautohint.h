/* ttfautohint.h */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#ifndef __TTFAUTOHINT_H__
#define __TTFAUTOHINT_H__

#include <stdio.h>


/* Error type. */

typedef int TA_Error;


/* Error values in addition to the FT_Err_XXX constants from FreeType. */

#define TA_Err_Ok 0x00
#define TA_Err_Invalid_Stream_Write 0x5F


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

#endif /* __TTFAUTOHINT_H__ */

/* end of ttfautohint.h */
