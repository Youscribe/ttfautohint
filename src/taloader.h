/* taloader.h */

/* originally file `afloader.h' (2011-Mar-28) from FreeType */

/* heavily modified 2011 by Werner Lemberg <wl@gnu.org> */

#ifndef __TALOADER_H__
#define __TALOADER_H__

#include "tahints.h"
#include "taglobal.h"
#include "tagloadr.h"


typedef struct TA_LoaderRec_
{
  FT_Face face; /* current face */
  TA_FaceGlobals globals; /* current face globals */
  TA_GlyphLoader gloader; /* glyph loader */
  TA_GlyphHintsRec hints;
  TA_ScriptMetrics metrics;
  FT_Bool transformed;
  FT_Matrix trans_matrix;
  FT_Vector trans_delta;
  FT_Vector pp1;
  FT_Vector pp2;
  /* we don't handle vertical phantom points */
} TA_LoaderRec, *TA_Loader;


FT_Error
ta_loader_init(TA_Loader loader);


FT_Error
ta_loader_reset(TA_Loader loader,
                FT_Face face);

void
ta_loader_done(TA_Loader loader);


FT_Error
ta_loader_load_glyph(TA_Loader loader,
                     FT_Face face,
                     FT_UInt gindex,
                     FT_UInt32 load_flags);

#endif /* __TALOADER_H__ */

/* end of taloader.h */
