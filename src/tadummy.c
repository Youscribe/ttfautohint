/* tadummy.c */

/* originally file `afdummy.c' (2011-Mar-28) from FreeType */

/* heavily modified 2011 by Werner Lemberg <wl@gnu.org> */

#include "tadummy.h"
#include "tahints.h"


static FT_Error
ta_dummy_hints_init(TA_GlyphHints hints,
                    TA_ScriptMetrics metrics)
{
  ta_glyph_hints_rescale(hints, metrics);
  return FT_Err_Ok;
}


static FT_Error
ta_dummy_hints_apply(TA_GlyphHints hints,
                     FT_Outline* outline)
{
  FT_UNUSED(hints);
  FT_UNUSED(outline);

  return FT_Err_Ok;
}


const TA_ScriptClassRec ta_dummy_script_class =
{
  TA_SCRIPT_NONE,
  NULL,

  sizeof (TA_ScriptMetricsRec),

  (TA_Script_InitMetricsFunc)NULL,
  (TA_Script_ScaleMetricsFunc)NULL,
  (TA_Script_DoneMetricsFunc)NULL,

  (TA_Script_InitHintsFunc)ta_dummy_hints_init,
  (TA_Script_ApplyHintsFunc)ta_dummy_hints_apply
};

/* end of tadummy.c */