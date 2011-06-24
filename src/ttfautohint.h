/* ttfautohint.h */

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


#ifndef __TTFAUTOHINT_H__
#define __TTFAUTOHINT_H__

#include <stdarg.h>


/* Error type. */

typedef int TA_Error;


/*
 * A callback function to get progress information.  `curr_idx' gives the
 * currently processed glyph index; if it is negative, an error has
 * occurred.  `num_glyphs' holds the total number of glyphs in the font
 * (this value can't be larger than 65535).
 *
 * `curr_sfnt' gives the current subfont within a TrueType Collection (TTC),
 * and `num_sfnts' the total number of subfonts.  Currently, the ttfautohint
 * library only hints glyphs from the `glyf' table used in subfont 0.
 *
 * `progress_data' is a void pointer to user supplied data.
 */

typedef void
(*TA_Progress_Func)(long curr_idx,
                    long num_glyphs,
                    long curr_sfnt,
                    long num_sfnts,
                    void *progress_data);


/* Error values in addition to the FT_Err_XXX constants from FreeType. */

#define TA_Err_Ok 0x00
#define TA_Err_Missing_Legal_Permission 0x0F
#define TA_Err_Invalid_Stream_Write 0x5F
#define TA_Err_Hinter_Overflow 0xF0


/*
 * Read a TrueType font, remove existing bytecode (in the SFNT tables
 * `prep', `fpgm', `cvt ', and `glyf'), and write a new TrueType font with
 * new bytecode based on the autohinting of the FreeType library.
 *
 * It expects a format string `options' and a variable number of arguments,
 * depending on the fields in `options'.  The fields are comma separated;
 * whitespace within the format string is not significant, a trailing comma
 * is ignored.  Fields are parsed from left to right; if a field occurs
 * multiple times, the last field's argument wins.  Depending on the field,
 * zero or one argument is expected.
 *
 * Note that fields marked as `not implemented yet' are subject to change.
 *
 *   in-file                      A pointer of type `FILE*' to the data
 *                                stream of the input font, opened for
 *                                binary reading.
 *
 *   in-buffer                    A pointer of type `const char*' to a
 *                                buffer which contains the input font.
 *                                Needs `in-buffer-len'.
 *
 *   in-buffer-len                A value of type `size_t', giving the
 *                                length of the input buffer.  Needs
 *                                `in-buffer'.
 *
 *   out-file                     A pointer of type `FILE*' to the data
 *                                stream of the output font, opened for
 *                                binary writing.
 *
 *   out-buffer                   A pointer of type `char**' to a buffer
 *                                which contains the output font.  Needs
 *                                `out-buffer-len'.  Deallocate the memory
 *                                with `free'.
 *
 *   out-buffer-len               A pointer of type `size_t*' to a value
 *                                giving the length of the output buffer.
 *                                Needs `out-buffer'.
 *
 *   progress-callback            A pointer of type `TA_Progress_Func',
 *                                specifying a callback function for
 *                                progress reports.  This function gets
 *                                called after a single glyph has been
 *                                processed.  If this field is not set, no
 *                                progress callback function is used.  Not
 *                                implemented yet.
 *
 *   progress-callback-data       A pointer of type `void*' to user data
 *                                which is passed to the progress callback
 *                                function.
 *
 *   hinting-range-min            An integer (which must be larger than or
 *                                equal to 2) giving the lowest ppem value
 *                                used for autohinting.  If this field is
 *                                not set, it defaults to value 8.
 *
 *   hinting-range-max            An integer (which must be larger than or
 *                                equal to the value of `hinting-range-min')
 *                                giving the highest ppem value used for
 *                                autohinting.  If this field is not set, it
 *                                defaults to value 1000.
 *
 *   pre-hinting                  An integer (1 for `on' and 0 for `off',
 *                                which is the default) to specify whether
 *                                native TrueType hinting shall be applied
 *                                to all glyphs before passing them to the
 *                                (internal) autohinter.  The used
 *                                resolution is the em-size in font units;
 *                                for most fonts this is 2048ppem.  Use this
 *                                if the hints move or scale subglyphs
 *                                independently of the output resolution.
 *                                Not implemented yet.
 *
 *   x-height-snapping-exceptions A pointer of type `const char*' to a
 *                                null-terminated string which gives a list
 *                                of comma separated ppem values or value
 *                                ranges at which no x-height snapping shall
 *                                be applied.  A value range has the form
 *                                `value1-value2', meaning `value1' <= ppem
 *                                <= `value2'.  Whitespace is not
 *                                significant; a trailing comma is ignored.
 *                                If the supplied argument is NULL, no
 *                                x-height snapping takes place at all.  By
 *                                default, there are no snapping exceptions.
 *                                Not implemented yet.
 *
 *   ignore-permissions           If the font has set bit 1 in the `fsType'
 *                                field of the `OS/2' table, the ttfautohint
 *                                library refuses to process the font since
 *                                a permission to do that is required from
 *                                the font's legal owner.  In case you have
 *                                such a permission you might set the
 *                                integer argument to value 1 to make
 *                                ttfautohint handle the font.
 *
 *   fallback-script              An integer which specifies the default
 *                                script for glyphs not in the `latin'
 *                                range.  If set to 1, the `latin' script is
 *                                used.  By default, no script is used
 *                                (value 0; this disables autohinting for
 *                                such glyphs).
 *
 * Remarks:
 *
 * o Obviously, it is necessary to have an input and an output data stream.
 *   All other options are optional.
 *
 * o `hinting-range-min' and `hinting-range-max' specify the range for which
 *   the autohinter generates optimized hinting code.  If a ppem is smaller
 *   than the value of `hinting-range-min', hinting still takes place but
 *   the configuration created for `hinting-range-min' is used.  The
 *   analogous action is taken for `hinting-range-max'.  The font's `gasp'
 *   table is set up to always use grayscale rendering with grid-fitting for
 *   standard hinting, and symmetric grid-fitting and symmetric smoothing
 *   for horizontal subpixel hinting (ClearType).
 */

TA_Error
TTF_autohint(const char* options,
             ...);

#endif /* __TTFAUTOHINT_H__ */

/* end of ttfautohint.h */
