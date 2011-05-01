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


static FT_Error
TA_sfnt_compute_global_hints(SFNT* sfnt,
                             FONT* font)
{
  FT_Error error;
  FT_Face face = sfnt->face;
  FT_UInt enc;
  FT_UInt idx;

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

  return error;
}


static FT_Error
TA_table_build_cvt(FT_Byte** cvt,
                   FT_ULong* cvt_len,
                   SFNT* sfnt,
                   FONT* font)
{
  TA_LatinAxis haxis;
  TA_LatinAxis vaxis;
  TA_LatinBlue blue;

  FT_UInt i;
  FT_UInt buf_len;
  FT_Byte* buf;
  FT_Byte* buf_p;

  FT_Error error;


  error = TA_sfnt_compute_global_hints(sfnt, font);
  if (error)
    return error;

  /* XXX check validity of pointers */
  haxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[0];
  vaxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[1];

  buf_len = 2 * (haxis->width_count + vaxis->width_count
                 + 2 * vaxis->blue_count);
  buf = (FT_Byte*)malloc(buf_len);
  if (!buf)
    return FT_Err_Out_Of_Memory;

  buf_p = buf;

  /* XXX emit standard_width also? */

  for (i = 0; i < haxis->width_count; i++)
  {
    if (haxis->widths[i].org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(haxis->widths[i].org);
    *(buf_p++) = LOW(haxis->widths[i].org);
  }

  for (i = 0; i < vaxis->width_count; i++)
  {
    if (vaxis->widths[i].org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(vaxis->widths[i].org);
    *(buf_p++) = LOW(vaxis->widths[i].org);
  }

  for (i = 0; i < vaxis->blue_count; i++)
  {
    if (vaxis->blues[i].ref.org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(vaxis->blues[i].ref.org);
    *(buf_p++) = LOW(vaxis->blues[i].ref.org);
    if (vaxis->blues[i].shoot.org > 0xFFFF)
      goto Err;
    *(buf_p++) = HIGH(vaxis->blues[i].shoot.org);
    *(buf_p++) = LOW(vaxis->blues[i].shoot.org);
  }

#if 0
  TA_LOG(("--------------------------------------------------\n"));
  TA_LOG(("glyph %d:\n", idx));
  ta_glyph_hints_dump_edges(_ta_debug_hints);
  ta_glyph_hints_dump_segments(_ta_debug_hints);
  ta_glyph_hints_dump_points(_ta_debug_hints);
#endif

  *cvt = buf;
  *cvt_len = buf_len;

  return FT_Err_Ok;

Err:
  free(buf);
  return TA_Err_Hinter_Overflow;
}


FT_Error
TA_sfnt_build_cvt_table(SFNT* sfnt,
                        FONT* font)
{
  FT_Error error;

  FT_Byte* cvt_buf;
  FT_ULong cvt_len;


  error = TA_sfnt_add_table_info(sfnt);
  if (error)
    return error;

  error = TA_table_build_cvt(&cvt_buf, &cvt_len, sfnt, font);
  if (error)
    return error;

  /* in case of success, `cvt_buf' gets linked */
  /* and is eventually freed in `TA_font_unload' */
  error = TA_font_add_table(font,
                            &sfnt->table_infos[sfnt->num_table_infos - 1],
                            TTAG_cvt, cvt_len, cvt_buf);
  if (error)
  {
    free(cvt_buf);
    return error;
  }
}


#define CVT_HORZ_WIDTHS_OFFSET(font) 0
#define CVT_VERT_WIDTHS_OFFSET(font) \
          CVT_HORZ_WIDTHS_OFFSET(font) \
          + ((TA_LatinMetrics)font->loader->hints.metrics)->axis[0].width_count;
#define CVT_BLUE_REFS_OFFSET(font) \
          CVT_VERT_WIDTHS_OFFSET(font) \
          + ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].width_count;
#define CVT_BLUE_SHOOTS_OFFSET(font) \
          CVT_BLUE_REFS_OFFSET(font) \
          + ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].blue_count;


/* in the comments below, the top of the stack (`s:') */
/* is the rightmost element; the stack is shown */
/* after the instruction on the same line has been executed */

  /*
   * compute_stem_width
   *
   *   This is the equivalent to the following code from function
   *   `ta_latin_compute_stem_width':
   *
   *      dist = ABS(width)
   *
   *      if stem_is_serif
   *         && dist < 3*64:
   *        return width
   *      else if base_is_round:
   *        if dist < 80
   *          dist = 64
   *      else:
   *        dist = MIN(56, dist)
   *
   *      delta = ABS(dist - std_width)
   *
   *      if delta < 40:
   *        dist = MIN(48, std_width)
   *        goto End
   *
   *      if dist < 3*64:
   *        delta = dist
   *        dist = FLOOR(dist)
   *        delta = delta - dist
   *
   *        if delta < 10:
   *          dist = dist + delta
   *        else if delta < 32:
   *          dist = dist + 10
   *        else if delta < 54:
   *          dist = dist + 54
   *        else
   *          dist = dist + delta
   *      else
   *        dist = ROUND(dist)
   *
   *    End:
   *      if width < 0:
   *        dist = -dist
   *      return dist
   *
   *
   *
   * Function 0: compute_stem_width
   *
   * in: width
   *     stem_is_serif
   *     base_is_round
   * out: new_width
   * CVT: is_extra_light
   *      std_width
   */

unsigned char fpgm_0a[] = {

  PUSHB_1
    0,
  FDEF

  DUP
  ABS /* s: base_is_round stem_is_serif width dist */

  DUP
  PUSHB_1
    3*64,
  LT /* dist < 3*64 */

  PUSHB_1
    4,
  MINDEX /* s: base_is_round width dist (dist<3*64) stem_is_serif */

  AND /* stem_is_serif && dist < 3*64 */
  IF /* s: base_is_round width dist */
    POP
    SWAP
    POP /* s: width */

  ELSE
    ROLL /* s: width dist base_is_round */
    IF /* s: width dist */
      DUP
      PUSHB_1
        80,
      LT /* dist < 80 */
      IF /* s: width dist */
        POP
        PUSHB_1
          64, /* dist = 64 */
      EIF

    ELSE
      PUSHB_1
        56,
      MIN /* dist = min(56, dist) */
    EIF

    DUP /* s: width dist dist */
    PUSHB_1

};

/*    %c, index of std_width */

unsigned char fpgm_0b[] = {

    RCVT
    SUB
    ABS /* s: width dist delta */

    PUSHB_1
      40,
    LT /* delta < 40 */
    IF /* s: width dist */
      POP
      PUSHB_2
        48,

};

/*      %c, index of std_width */

unsigned char fpgm_0c[] = {

      RCVT
      MIN /* dist = min(48, std_width) */

    ELSE
      DUP /* s: width dist dist */
      PUSHB_1
        3*64,
      LT /* dist < 3*64 */
      IF
        DUP /* s: width delta dist */
        FLOOR /* dist = FLOOR(dist) */
        DUP /* s: width delta dist dist */
        ROLL
        ROLL /* s: width dist delta dist */
        SUB /* delta = delta - dist */

        DUP /* s: width dist delta delta */
        PUSHB_1
          10,
        LT /* delta < 10 */
        IF /* s: width dist delta */
          ADD /* dist = dist + delta */

        ELSE
          DUP
          PUSHB_1
            32,
          LT /* delta < 32 */
          IF
            POP
            PUSHB_1
              10,
            ADD /* dist = dist + 10 */

          ELSE
            DUP
            PUSHB_1
              54,
            LT /* delta < 54 */
            IF
              POP
              PUSHB_1
                54,
              ADD /* dist = dist + 54 */

            ELSE
              ADD /* dist = dist + delta */

            EIF
          EIF
        EIF

        ELSE
          PUSHB_1
            32,
          ADD
          FLOOR /* dist = round(dist) */

        EIF
      EIF

      SWAP /* s: dist width */
      PUSHB_1
        0,
      LT /* width < 0 */
      NEG /* dist = -dist */

    EIF
  EIF

  ENDF

};


static FT_Error
TA_table_build_fpgm(FT_Byte** fpgm,
                    FT_ULong* fpgm_len,
                    FONT* font)
{
  FT_UInt buf_len;
  FT_Byte* buf;
  FT_Byte* buf_p;


  buf_len = sizeof (fpgm_0a) + 1
            + sizeof (fpgm_0b) + 1
            + sizeof (fpgm_0c);
  buf = (FT_Byte*)malloc(buf_len);
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* copy font program into buffer and fill in the missing variables */
  buf_p = buf;
  memcpy(buf_p, fpgm_0a, sizeof (fpgm_0a));
  buf_p += sizeof (fpgm_0a);
  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
  memcpy(buf_p, fpgm_0b, sizeof (fpgm_0b));
  buf_p += sizeof (fpgm_0b);
  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
  memcpy(buf_p, fpgm_0c, sizeof (fpgm_0c));

  *fpgm = buf;
  *fpgm_len = buf_len;

  return FT_Err_Ok;
}


FT_Error
TA_sfnt_build_fpgm_table(SFNT* sfnt,
                         FONT* font)
{
  FT_Error error;

  FT_Byte* fpgm_buf;
  FT_ULong fpgm_len;


  error = TA_sfnt_add_table_info(sfnt);
  if (error)
    return error;

  error = TA_table_build_fpgm(&fpgm_buf, &fpgm_len, font);
  if (error)
    return error;

  /* in case of success, `fpgm_buf' gets linked */
  /* and is eventually freed in `TA_font_unload' */
  error = TA_font_add_table(font,
                            &sfnt->table_infos[sfnt->num_table_infos - 1],
                            TTAG_fpgm, fpgm_len, fpgm_buf);
  if (error)
  {
    free(fpgm_buf);
    return error;
  }
}

/* end of tabytecode.c */
