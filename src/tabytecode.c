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

  FT_UInt i;
  FT_UInt buf_len;
  FT_UInt len;
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

  /* buffer length must be a multiple of four */
  len = (buf_len + 3) & ~3;
  buf = (FT_Byte*)malloc(len);
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* pad end of buffer with zeros */
  buf[len - 1] = 0x00;
  buf[len - 2] = 0x00;
  buf[len - 3] = 0x00;

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


/* the horizontal stem widths */
#define CVT_HORZ_WIDTHS_OFFSET(font) 0
#define CVT_HORZ_WIDTHS_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[0].width_count

/* the vertical stem widths */
#define CVT_VERT_WIDTHS_OFFSET(font) \
          CVT_HORZ_WIDTHS_OFFSET(font) \
          + ((TA_LatinMetrics)font->loader->hints.metrics)->axis[0].width_count
#define CVT_VERT_WIDTHS_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].width_count

/* the blue zone values for flat edges */
#define CVT_BLUE_REFS_OFFSET(font) \
          CVT_VERT_WIDTHS_OFFSET(font) \
          + ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].width_count
#define CVT_BLUE_REFS_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].blue_count

/* the blue zone values for round edges */
#define CVT_BLUE_SHOOTS_OFFSET(font) \
          CVT_BLUE_REFS_OFFSET(font) \
          + ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].blue_count
#define CVT_BLUE_SHOOTS_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].blue_count


/* symbolic names for storage area locations */

#define sal_counter 0
#define sal_limit 1
#define sal_scale 2
#define sal_0x10000 3


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
 * CVT: is_extra_light   XXX
 *      std_width
 */

#define compute_stem_width 0

unsigned char fpgm_0a[] = {

  PUSHB_1,
    compute_stem_width,
  FDEF,

  DUP,
  ABS, /* s: base_is_round stem_is_serif width dist */

  DUP,
  PUSHB_1,
    3*64,
  LT, /* dist < 3*64 */

  PUSHB_1,
    4,
  MINDEX, /* s: base_is_round width dist (dist<3*64) stem_is_serif */

  AND, /* stem_is_serif && dist < 3*64 */
  IF, /* s: base_is_round width dist */
    POP,
    SWAP,
    POP, /* s: width */

  ELSE,
    ROLL, /* s: width dist base_is_round */
    IF, /* s: width dist */
      DUP,
      PUSHB_1,
        80,
      LT, /* dist < 80 */
      IF, /* s: width dist */
        POP,
        PUSHB_1,
          64, /* dist = 64 */
      EIF,

    ELSE,
      PUSHB_1,
        56,
      MIN, /* dist = min(56, dist) */
    EIF,

    DUP, /* s: width dist dist */
    PUSHB_1,

};

/*    %c, index of std_width */

unsigned char fpgm_0b[] = {

    RCVT,
    SUB,
    ABS, /* s: width dist delta */

    PUSHB_1,
      40,
    LT, /* delta < 40 */
    IF, /* s: width dist */
      POP,
      PUSHB_2,
        48,

};

/*      %c, index of std_width */

unsigned char fpgm_0c[] = {

      RCVT,
      MIN, /* dist = min(48, std_width) */

    ELSE,
      DUP, /* s: width dist dist */
      PUSHB_1,
        3*64,
      LT, /* dist < 3*64 */
      IF,
        DUP, /* s: width delta dist */
        FLOOR, /* dist = FLOOR(dist) */
        DUP, /* s: width delta dist dist */
        ROLL,
        ROLL, /* s: width dist delta dist */
        SUB, /* delta = delta - dist */

        DUP, /* s: width dist delta delta */
        PUSHB_1,
          10,
        LT, /* delta < 10 */
        IF, /* s: width dist delta */
          ADD, /* dist = dist + delta */

        ELSE,
          DUP,
          PUSHB_1,
            32,
          LT, /* delta < 32 */
          IF,
            POP,
            PUSHB_1,
              10,
            ADD, /* dist = dist + 10 */

          ELSE,
            DUP,
            PUSHB_1,
              54,
            LT, /* delta < 54 */
            IF,
              POP,
              PUSHB_1,
                54,
              ADD, /* dist = dist + 54 */

            ELSE,
              ADD, /* dist = dist + delta */

            EIF,
          EIF,
        EIF,

        ELSE,
          PUSHB_1,
            32,
          ADD,
          FLOOR, /* dist = round(dist) */

        EIF,
      EIF,

      SWAP, /* s: dist width */
      PUSHB_1,
        0,
      LT, /* width < 0 */
      NEG, /* dist = -dist */

    EIF,
  EIF,

  ENDF,

};


/*
 * loop
 *
 *   Take a range and a function number and apply the function to all
 *   elements of the range.  The called function must not change the
 *   stack.
 *
 * Function 1: loop
 *
 * in: func_num
 *     end
 *     start
 *
 * uses: sal_counter (counter initialized with `start')
 *       sal_limit (`end')
 */

#define loop 1

unsigned char fpgm_1[] = {

  PUSHB_1,
    loop,
  FDEF,

  ROLL, /* s: func_num start end */
  PUSHB_1,
    sal_limit,
  SWAP,
  WS,

  PUSHB_1,
    sal_counter,
  SWAP,
  WS,

/* start_loop: */
  PUSHB_1,
    sal_counter,
  RS,
  PUSHB_1,
    sal_limit,
  RS,
  LTEQ, /* start <= end */
  IF, /* s: func_num */
    DUP,
    CALL,
    PUSHB_2,
      1,
      sal_counter,
    RS,
    ADD, /* start = start + 1 */
    PUSHB_1,
      sal_counter,
    SWAP,
    WS,

    PUSHB_1,
      22,
    NEG,
    JMPR, /* goto start_loop */
  ELSE,
    POP,
  EIF,

  ENDF,

};


/*
 * rescale
 *
 *   All entries in the CVT table get scaled automatically using the
 *   vertical resolution.  However, some widths must be scaled with the
 *   horizontal resolution, and others get adjusted later on.
 *
 * Function 2: rescale
 *
 * uses: sal_counter (CVT index)
 *       sal_scale (scale in 16.16 format)
 */

#define rescale 2

unsigned char fpgm_2[] = {

  PUSHB_1,
    rescale,
  FDEF,

  PUSHB_1,
    sal_counter,
  RS,
  DUP,
  RCVT,
  PUSHB_1,
    sal_scale,
  RS,
  MUL, /* CVT * scale * 2^10 */
  PUSHB_1,
    sal_0x10000,
  RS,
  DIV, /* CVT * scale */

  SWAP,
  WCVTP,

  ENDF,

};


/* we often need 0x10000 which can't be pushed directly onto the stack, */
/* thus we provide it in the storage area */

unsigned char fpgm_A[] = {

  PUSHB_1,
    sal_0x10000,
  PUSHW_2,
    0x08, /* 0x800 */
    0x00,
    0x08, /* 0x800 */
    0x00,
  MUL, /* 0x10000 */
  WS,

};


static FT_Error
TA_table_build_fpgm(FT_Byte** fpgm,
                    FT_ULong* fpgm_len,
                    FONT* font)
{
  FT_UInt buf_len;
  FT_UInt len;
  FT_Byte* buf;
  FT_Byte* buf_p;


  buf_len = sizeof (fpgm_0a)
            + 1
            + sizeof (fpgm_0b)
            + 1
            + sizeof (fpgm_0c)
            + sizeof (fpgm_1)
            + sizeof (fpgm_2)
            + sizeof (fpgm_A);
  /* buffer length must be a multiple of four */
  len = (buf_len + 3) & ~3;
  buf = (FT_Byte*)malloc(len);
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* pad end of buffer with zeros */
  buf[len - 1] = 0x00;
  buf[len - 2] = 0x00;
  buf[len - 3] = 0x00;

  /* copy font program into buffer and fill in the missing variables */
  buf_p = buf;

  memcpy(buf_p, fpgm_0a, sizeof (fpgm_0a));
  buf_p += sizeof (fpgm_0a);

  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);

  memcpy(buf_p, fpgm_0b, sizeof (fpgm_0b));
  buf_p += sizeof (fpgm_0b);

  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);

  memcpy(buf_p, fpgm_0c, sizeof (fpgm_0c));
  buf_p += sizeof (fpgm_0c);

  memcpy(buf_p, fpgm_1, sizeof (fpgm_1));
  buf_p += sizeof (fpgm_1);

  memcpy(buf_p, fpgm_2, sizeof (fpgm_2));
  buf_p += sizeof (fpgm_2);

  memcpy(buf_p, fpgm_A, sizeof (fpgm_A));

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


/* the `prep' instructions */

unsigned char prep_a[] = {

  /* scale horizontal CVT entries */
  /* if horizontal and vertical resolutions differ */

  SVTCA_x,
  MPPEM,
  SVTCA_y,
  MPPEM,
  NEQ, /* horz_ppem != vert_ppem */
  IF,
    SVTCA_x,
    MPPEM,
    PUSHB_1,
      sal_0x10000,
    RS,
    MUL, /* horz_ppem in 22.10 format */

    SVTCA_y,
    MPPEM,
    DIV, /* (horz_ppem / vert_ppem) in 16.16 format */

    PUSHB_1,
      sal_scale,
    SWAP,
    WS,

    /* loop over horizontal CVT entries */
    PUSHB_4,

};

/*    %c, first horizontal index */
/*    %c, last horizontal index */

unsigned char prep_b[] = {

      rescale,
      loop,
    CALL,
  EIF,

};

unsigned char prep_c[] = {

  /* optimize the alignment of the top of small letters to the pixel grid */

  PUSHB_1,

};

/*  %c, index of alignment blue zone */

unsigned char prep_d[] = {

  RCVT,
  DUP,
  DUP,
  PUSHB_1,
    40,
  ADD,
  FLOOR, /* fitted = FLOOR(scaled + 40) */
  DUP, /* s: scaled scaled fitted fitted */
  ROLL,
  NEQ,
  IF, /* s: scaled fitted */
    SWAP,
    PUSHB_1,
      sal_0x10000,
    RS,
    MUL, /* scaled in 16.16 format */
    SWAP,
    DIV, /* (scaled / fitted) in 16.16 format */

    PUSHB_1,
      sal_scale,
    SWAP,
    WS,

    /* loop over vertical CVT entries */
    PUSHB_4,

};

/*    %c, first vertical index */
/*    %c, last vertical index */

unsigned char prep_e[] = {

      rescale,
      loop,
    CALL,

    /* loop over blue refs */
    PUSHB_4,

};

/*    %c, first blue ref index */
/*    %c, last blue ref index */

unsigned char prep_f[] = {

      rescale,
      loop,
    CALL,

    /* loop over blue shoots */
    PUSHB_4,

};

/*    %c, first blue shoot index */
/*    %c, last blue shoot index */

unsigned char prep_g[] = {

      rescale,
      loop,
    CALL,

  EIF,

};


static FT_Error
TA_table_build_prep(FT_Byte** prep,
                    FT_ULong* prep_len,
                    FONT* font)
{
  TA_LatinAxis vaxis;
  TA_LatinBlue blue_adjustment;
  FT_UInt i;

  FT_UInt buf_len;
  FT_UInt len;
  FT_Byte* buf;
  FT_Byte* buf_p;


  vaxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[1];
  blue_adjustment = NULL;

  for (i = 0; i < vaxis->blue_count; i++)
  {
    if (vaxis->blues[i].flags & TA_LATIN_BLUE_ADJUSTMENT)
    {
      blue_adjustment = &vaxis->blues[i];
      break;
    }
  }

  buf_len = sizeof (prep_a)
            + 2
            + sizeof (prep_b);

  if (blue_adjustment)
  {
    buf_len += sizeof (prep_c)
               + 1
               + sizeof (prep_d)
               + 2
               + sizeof (prep_e)
               + 2
               + sizeof (prep_f)
               + 2
               + sizeof (prep_g);
  }

  /* buffer length must be a multiple of four */
  len = (buf_len + 3) & ~3;
  buf = (FT_Byte*)malloc(len);
  if (!buf)
    return FT_Err_Out_Of_Memory;

  /* pad end of buffer with zeros */
  buf[len - 1] = 0x00;
  buf[len - 2] = 0x00;
  buf[len - 3] = 0x00;

  /* copy cvt program into buffer and fill in the missing variables */
  buf_p = buf;

  memcpy(buf_p, prep_a, sizeof (prep_a));
  buf_p += sizeof (prep_a);

  *(buf_p++) = (unsigned char)CVT_HORZ_WIDTHS_OFFSET(font);
  *(buf_p++) = (unsigned char)(CVT_HORZ_WIDTHS_OFFSET(font)
                               + CVT_HORZ_WIDTHS_SIZE(font) - 1);

  memcpy(buf_p, prep_b, sizeof (prep_b));

  if (blue_adjustment)
  {
    buf_p += sizeof (prep_b);

    memcpy(buf_p, prep_c, sizeof (prep_c));
    buf_p += sizeof (prep_c);

    *(buf_p++) = blue_adjustment - vaxis->blues;

    memcpy(buf_p, prep_d, sizeof (prep_d));
    buf_p += sizeof (prep_d);

    *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_VERT_WIDTHS_OFFSET(font)
                                 + CVT_VERT_WIDTHS_SIZE(font) - 1);

    memcpy(buf_p, prep_e, sizeof (prep_e));
    buf_p += sizeof (prep_e);

    *(buf_p++) = (unsigned char)CVT_BLUE_REFS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_REFS_OFFSET(font)
                                 + CVT_BLUE_REFS_SIZE(font) - 1);

    memcpy(buf_p, prep_f, sizeof (prep_f));
    buf_p += sizeof (prep_f);

    *(buf_p++) = (unsigned char)CVT_BLUE_SHOOTS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_SHOOTS_OFFSET(font)
                                 + CVT_BLUE_SHOOTS_SIZE(font) - 1);

    memcpy(buf_p, prep_g, sizeof (prep_g));
  }

  *prep = buf;
  *prep_len = buf_len;

  return FT_Err_Ok;
}


FT_Error
TA_sfnt_build_prep_table(SFNT* sfnt,
                         FONT* font)
{
  FT_Error error;

  FT_Byte* prep_buf;
  FT_ULong prep_len;


  error = TA_sfnt_add_table_info(sfnt);
  if (error)
    return error;

  error = TA_table_build_prep(&prep_buf, &prep_len, font);
  if (error)
    return error;

  /* in case of success, `prep_buf' gets linked */
  /* and is eventually freed in `TA_font_unload' */
  error = TA_font_add_table(font,
                            &sfnt->table_infos[sfnt->num_table_infos - 1],
                            TTAG_prep, prep_len, prep_buf);
  if (error)
  {
    free(prep_buf);
    return error;
  }
}

/* end of tabytecode.c */
