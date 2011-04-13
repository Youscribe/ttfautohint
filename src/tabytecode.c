/* tabytecode.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#include "ta.h"
#include "tabytecode.h"


#ifdef TA_DEBUG
int _ta_debug = 1;
int _ta_debug_disable_horz_hints;
int _ta_debug_disable_vert_hints;
int _ta_debug_disable_blue_hints;
void* _ta_debug_hints;
#endif


FT_Error
TA_font_compute_global_hints(FONT* font)
{
  FT_Error error;
  FT_Face face = font->sfnts[0].face; /* XXX loop over all subfonts */
  FT_UInt enc;
  FT_UInt idx;
  TA_LatinAxis haxis;
  TA_LatinAxis vaxis;
  TA_LatinBlue blue;

  static const FT_Encoding latin_encs[] =
  {
    FT_ENCODING_UNICODE,
    FT_ENCODING_APPLE_ROMAN,
    FT_ENCODING_ADOBE_STANDARD,
    FT_ENCODING_ADOBE_LATIN_1,

    FT_ENCODING_NONE /* end of list */
  };


  error = ta_loader_init(font->loader);
  if (error)
    return error;

  /* try to select a latin charmap */
  for (enc = 0; latin_encs[enc] != FT_ENCODING_NONE; enc++)
  {
    error = FT_Select_Charmap(face, latin_encs[enc]);
    if (!error)
      break;
  }

  /* load latin glyph `a' to trigger all initializations */
  idx = FT_Get_Char_Index(face, 'a');
  error = ta_loader_load_glyph(font->loader, face, idx, 0);
  if (error)
    return error;

  haxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[0];
  vaxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[1];

#ifdef TA_DEBUG
  TA_LOG(("--------------------------------------------------\n"));
  TA_LOG(("glyph %d:\n", idx));
  ta_glyph_hints_dump_edges(_ta_debug_hints);
  ta_glyph_hints_dump_segments(_ta_debug_hints);
  ta_glyph_hints_dump_points(_ta_debug_hints);
#endif

  ta_loader_done(font->loader);

  return TA_Err_Ok;
}

/* end of tabytecode.c */
