/* taglobal.h */

/* originally file `afglobal.h' (2011-Mar-28) from FreeType */

/* heavily modified 2011 by Werner Lemberg <wl@gnu.org> */

#ifndef __TAGLOBAL_H__
#define __TAGLOBAL_H__

#include "tatypes.h"


/* this models the global hints data for a given face, */
/* decomposed into script-specific items */
typedef struct TA_FaceGlobalsRec_* TA_FaceGlobals;

FT_Error
ta_face_globals_new(FT_Face face,
                    TA_FaceGlobals *aglobals);

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
