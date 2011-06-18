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
#define TA_Err_Hinter_Overflow 0xF0


/*
 * Read a TrueType font, remove existing bytecode (in the SFNT tables
 * `prep', `fpgm', `cvt ', and `glyf'), and write a new TrueType font with
 * new bytecode based on the autohinting of the FreeType library.
 *
 * It expects three arguments:
 *
 *   in        A void pointer to the input data.
 *
 *   out       A void pointer to the output data.
 *
 *   options   A string giving options to control the conversion.
 *
 * By default, `in' and `out' are cast to `FILE*', opened for binary reading
 * and writing, respectively.
 *
 * Currently, `options' is ignored.
 *
 */

TA_Error
TTF_autohint(void *in,
             void *out,
             const char* options);

#endif /* __TTFAUTOHINT_H__ */

/* end of ttfautohint.h */
