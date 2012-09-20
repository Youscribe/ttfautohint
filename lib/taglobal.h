/* taglobal.h */

/*
 * Copyright (C) 2011-2012 by Werner Lemberg.
 *
 * This file is part of the ttfautohint library, and may only be used,
 * modified, and distributed under the terms given in `COPYING'.  By
 * continuing to use, modify, or distribute this file you indicate that you
 * have read `COPYING' and understand and accept it fully.
 *
 * The file `COPYING' mentioned in the previous paragraph is distributed
 * with the ttfautohint library.
 */


/* originally file `afglobal.h' (2011-Mar-28) from FreeType */

/* heavily modified 2011 by Werner Lemberg <wl@gnu.org> */

#ifndef __TAGLOBAL_H__
#define __TAGLOBAL_H__

#include "ta.h"
#include "tatypes.h"


/* Default values and flags for both autofitter globals */
/* (originally found in AF_ModuleRec, we use FONT instead) */
/* and face globals (in TA_FaceGlobalsRec). */

/* index of fallback script in `ta_script_classes' */
#define TA_SCRIPT_FALLBACK 0
/* a bit mask indicating an uncovered glyph */
#define TA_SCRIPT_NONE 0x7F
/* if this flag is set, we have an ASCII digit */
#define TA_DIGIT 0x80

/* `increase-x-height' property */
#define TA_PROP_INCREASE_X_HEIGHT_MIN 6
#define TA_PROP_INCREASE_X_HEIGHT_MAX 0


/* note that glyph_scripts[] is used to map each glyph into */
/* an index into the `ta_script_classes' array. */
typedef struct TA_FaceGlobalsRec_
{
  FT_Face face;
  FT_Long glyph_count; /* same as face->num_glyphs */
  FT_Byte* glyph_scripts;

  /* per-face auto-hinter properties */
  FT_UInt increase_x_height;

  TA_ScriptMetrics metrics[TA_SCRIPT_MAX];

  FONT* font; /* to access global properties */
} TA_FaceGlobalsRec;


/* this models the global hints data for a given face, */
/* decomposed into script-specific items */

FT_Error
ta_face_globals_new(FT_Face face,
                    TA_FaceGlobals *aglobals,
                    FONT* font);

FT_Error
ta_face_globals_get_metrics(TA_FaceGlobals globals,
                            FT_UInt gindex,
                            FT_UInt options,
                            TA_ScriptMetrics *ametrics);

void
ta_face_globals_free(TA_FaceGlobals globals);

FT_Bool
ta_face_globals_is_digit(TA_FaceGlobals globals,
                         FT_UInt gindex);

#endif /* __TAGLOBAL_H__ */

/* end of taglobal.h */
