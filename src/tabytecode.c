/* tabytecode.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#include "ta.h"
#include "tabytecode.h"


/* a simple macro to emit bytecode instructions */
#define BCI(code) *(bufp++) = (code)

/* we increase the stack depth by this amount */
#define ADDITIONAL_STACK_ELEMENTS 20


/* #define DEBUGGING */


#ifdef TA_DEBUG
int _ta_debug = 1;
int _ta_debug_disable_horz_hints;
int _ta_debug_disable_vert_hints;
int _ta_debug_disable_blue_hints;
void* _ta_debug_hints;
#endif


typedef struct Hints_Record_ {
  FT_UInt size;
  FT_UInt num_actions;
  FT_Byte* buf;
  FT_UInt buf_len;
} Hints_Record;

typedef struct Recorder_ {
  FONT* font;
  Hints_Record hints_record;
/* see explanations in `TA_sfnt_build_glyph_segments' */
  FT_UInt* wrap_around_segments;
} Recorder;


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

  buf_len = 2 * (2
                 + haxis->width_count
                 + vaxis->width_count
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

  if (haxis->width_count > 0)
  {
    *(buf_p++) = HIGH(haxis->widths[0].org);
    *(buf_p++) = LOW(haxis->widths[0].org);
  }
  else
  {
    *(buf_p++) = 0;
    *(buf_p++) = 50;
  }
  if (vaxis->width_count > 0)
  {
    *(buf_p++) = HIGH(vaxis->widths[0].org);
    *(buf_p++) = LOW(vaxis->widths[0].org);
  }
  else
  {
    *(buf_p++) = 0;
    *(buf_p++) = 50;
  }

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
  }

  for (i = 0; i < vaxis->blue_count; i++)
  {
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

  return FT_Err_Ok;
}


/* the horizontal and vertical standard widths */
#define CVT_HORZ_STANDARD_WIDTH_OFFSET(font) 0
#define CVT_VERT_STANDARD_WIDTH_OFFSET(font) \
          CVT_HORZ_STANDARD_WIDTH_OFFSET(font) + 1

/* the horizontal stem widths */
#define CVT_HORZ_WIDTHS_OFFSET(font) \
          CVT_VERT_STANDARD_WIDTH_OFFSET(font) + 1
#define CVT_HORZ_WIDTHS_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[0].width_count

/* the vertical stem widths */
#define CVT_VERT_WIDTHS_OFFSET(font) \
          CVT_HORZ_WIDTHS_OFFSET(font) + CVT_HORZ_WIDTHS_SIZE(font)
#define CVT_VERT_WIDTHS_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].width_count

/* the number of blue zones */
#define CVT_BLUES_SIZE(font) \
          ((TA_LatinMetrics)font->loader->hints.metrics)->axis[1].blue_count

/* the blue zone values for flat and round edges */
#define CVT_BLUE_REFS_OFFSET(font) \
          CVT_VERT_WIDTHS_OFFSET(font) + CVT_VERT_WIDTHS_SIZE(font)
#define CVT_BLUE_SHOOTS_OFFSET(font) \
          CVT_BLUE_REFS_OFFSET(font) + CVT_BLUES_SIZE(font)


/* symbolic names for storage area locations */

#define sal_i 0
#define sal_j sal_i + 1
#define sal_k sal_j + 1
#define sal_temp1 sal_k + 1
#define sal_temp2 sal_temp1 + 1
#define sal_temp3 sal_temp2 + 1
#define sal_limit sal_temp3 + 1
#define sal_func sal_limit +1
#define sal_num_segments sal_func + 1
#define sal_scale sal_num_segments + 1
#define sal_0x10000 sal_scale + 1
#define sal_is_extra_light sal_0x10000 + 1
#define sal_anchor sal_is_extra_light + 1
#define sal_point_min sal_anchor + 1
#define sal_point_max sal_point_min + 1
#define sal_segment_offset sal_point_max + 1 /* must be last */


/* we need the following macro */
/* so that `func_name' doesn't get replaced with its #defined value */
/* (as defined in `tabytecode.h') */

#define FPGM(func_name) fpgm_ ## func_name


/* in the comments below, the top of the stack (`s:') */
/* is the rightmost element; the stack is shown */
/* after the instruction on the same line has been executed */

/* we use two sets of points in the twilight zone (zp0): */
/* one set to hold the unhinted segment positions, */
/* and another one to track the positions as changed by the hinting -- */
/* this is necessary since all points in zp0 */
/* have (0,0) as the original coordinates, */
/* making e.g. `MD_orig' return useless results */


/*
 * bci_round
 *
 *   Round a 26.6 number.  Contrary to the ROUND bytecode instruction, no
 *   engine specific corrections are applied.
 *
 * in: val
 * out: ROUND(val)
 */

unsigned char FPGM(bci_round) [] = {

  PUSHB_1,
    bci_round,
  FDEF,

  DUP,
  ABS,
  PUSHB_1,
    32,
  ADD,
  FLOOR,
  SWAP,
  PUSHB_1,
    0,
  LT,
  IF,
    NEG,
  EIF,

  ENDF,

};


/*
 * bci_compute_stem_width
 *
 *   This is the equivalent to the following code from function
 *   `ta_latin_compute_stem_width':
 *
 *      dist = ABS(width)
 *
 *      if (stem_is_serif
 *          && dist < 3*64)
 *         || is_extra_light:
 *        return width
 *      else if base_is_round:
 *        if dist < 80
 *          dist = 64
 *      else if dist < 56:
 *        dist = 56
 *
 *      delta = ABS(dist - std_width)
 *
 *      if delta < 40:
 *        dist = std_width
 *        if dist < 48
 *          dist = 48
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
 * in: width
 *     stem_is_serif
 *     base_is_round
 * out: new_width
 * sal: sal_is_extra_light
 * CVT: std_width
 */

unsigned char FPGM(bci_compute_stem_width_a) [] = {

  PUSHB_1,
    bci_compute_stem_width,
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

  PUSHB_1,
    sal_is_extra_light,
  RS,
  OR, /* (stem_is_serif && dist < 3*64) || is_extra_light */

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
      DUP,
      PUSHB_1,
        56,
      LT, /* dist < 56 */
      IF, /* s: width dist */
        POP,
        PUSHB_1,
          56, /* dist = 56 */
      EIF,
    EIF,

    DUP, /* s: width dist dist */
    PUSHB_1,

};

/*    %c, index of std_width */

unsigned char FPGM(bci_compute_stem_width_b) [] = {

    RCVT,
    SUB,
    ABS, /* s: width dist delta */

    PUSHB_1,
      40,
    LT, /* delta < 40 */
    IF, /* s: width dist */
      POP,
      PUSHB_1,

};

/*      %c, index of std_width */

unsigned char FPGM(bci_compute_stem_width_c) [] = {

      RCVT, /* dist = std_width */
      DUP,
      PUSHB_1,
        48,
      LT, /* dist < 48 */
      IF,
        POP,
        PUSHB_1,
          48, /* dist = 48 */
      EIF,

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
            bci_round,
          CALL, /* dist = round(dist) */

        EIF,
      EIF,

      SWAP, /* s: dist width */
      PUSHB_1,
        0,
      LT, /* width < 0 */
      IF,
        NEG, /* dist = -dist */
      EIF,
    EIF,
  EIF,

  ENDF,

};


/*
 * bci_loop
 *
 *   Take a range and a function number and apply the function to all
 *   elements of the range.
 *
 * in: func_num
 *     end
 *     start
 *
 * uses: sal_i (counter initialized with `start')
 *       sal_limit (`end')
 *       sal_func (`func_num')
 */

unsigned char FPGM(bci_loop) [] = {

  PUSHB_1,
    bci_loop,
  FDEF,

  PUSHB_1,
    sal_func,
  SWAP,
  WS, /* sal_func = func_num */
  PUSHB_1,
    sal_limit,
  SWAP,
  WS, /* sal_limit = end */
  PUSHB_1,
    sal_i,
  SWAP,
  WS, /* sal_i = start */

/* start_loop: */
  PUSHB_1,
    sal_i,
  RS,
  PUSHB_1,
    sal_limit,
  RS,
  LTEQ, /* start <= end */
  IF,
    PUSHB_1,
      sal_func,
    RS,
    CALL,
    PUSHB_3,
      sal_i,
      1,
      sal_i,
    RS,
    ADD, /* start = start + 1 */
    WS,

    PUSHB_1,
      22,
    NEG,
    JMPR, /* goto start_loop */
  EIF,

  ENDF,

};


/*
 * bci_cvt_rescale
 *
 *   Rescale CVT value by a given factor.
 *
 * uses: sal_i (CVT index)
 *       sal_scale (scale in 16.16 format)
 */

unsigned char FPGM(bci_cvt_rescale) [] = {

  PUSHB_1,
    bci_cvt_rescale,
  FDEF,

  PUSHB_1,
    sal_i,
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

  WCVTP,

  ENDF,

};


/*
 * bci_blue_round
 *
 *   Round a blue ref value and adjust its corresponding shoot value.
 *
 * uses: sal_i (CVT index)
 *
 */

unsigned char FPGM(bci_blue_round_a) [] = {

  PUSHB_1,
    bci_blue_round,
  FDEF,

  PUSHB_1,
    sal_i,
  RS,
  DUP,
  RCVT, /* s: ref_idx ref */

  DUP,
  PUSHB_1,
    bci_round,
  CALL,
  SWAP, /* s: ref_idx round(ref) ref */

  PUSHB_2,

};

/*  %c, blue_count */

unsigned char FPGM(bci_blue_round_b) [] = {

    4,
  CINDEX,
  ADD, /* s: ref_idx round(ref) ref shoot_idx */
  DUP,
  RCVT, /* s: ref_idx round(ref) ref shoot_idx shoot */

  ROLL, /* s: ref_idx round(ref) shoot_idx shoot ref */
  SWAP,
  SUB, /* s: ref_idx round(ref) shoot_idx dist */
  DUP,
  ABS, /* s: ref_idx round(ref) shoot_idx dist delta */

  DUP,
  PUSHB_1,
    32,
  LT, /* delta < 32 */
  IF,
    POP,
    PUSHB_1,
      0, /* delta = 0 */

  ELSE,
    PUSHB_1,
      48,
    LT, /* delta < 48 */
    IF,
      PUSHB_1,
        32, /* delta = 32 */

    ELSE,
      PUSHB_1,
        64, /* delta = 64 */
    EIF,
  EIF,

  SWAP, /* s: ref_idx round(ref) shoot_idx delta dist */
  PUSHB_1,
    0,
  LT, /* dist < 0 */
  IF,
    NEG, /* delta = -delta */
  EIF,

  PUSHB_1,
    3,
  CINDEX,
  ADD, /* s: ref_idx round(ref) shoot_idx (round(ref) + delta) */

  WCVTP,
  WCVTP,

  ENDF,

};


/*
 * bci_get_point_extrema
 *
 *   An auxiliary function for `bci_create_segment'.
 *
 * in: point-1
 * out: point
 *
 * sal: sal_point_min
 *      sal_point_max
 */

unsigned char FPGM(bci_get_point_extrema) [] = {

  PUSHB_1,
    bci_get_point_extrema,
  FDEF,

  PUSHB_1,
    1,
  ADD, /* s: point */
  DUP,
  DUP,

  /* check whether `point' is a new minimum */
  PUSHB_1,
    sal_point_min,
  RS, /* s: point point point point_min */
  MD_orig,
  /* if distance is negative, we have a new minimum */
  PUSHB_1,
    0,
  LT,
  IF, /* s: point point */
    DUP,
    PUSHB_1,
      sal_point_min,
    SWAP,
    WS,
  EIF,

  /* check whether `point' is a new maximum */
  PUSHB_1,
    sal_point_max,
  RS, /* s: point point point_max */
  MD_orig,
  /* if distance is positive, we have a new maximum */
  PUSHB_1,
    0,
  GT,
  IF, /* s: point */
    DUP,
    PUSHB_1,
      sal_point_max,
    SWAP,
    WS,
  EIF, /* s: point */

  ENDF,

};


/*
 * bci_create_segment
 *
 *   Store start and end point of a segment in the storage area,
 *   then construct two points in the twilight zone to represent it:
 *   an original one (which stays unmodified) and a hinted one,
 *   initialized with the original value.
 *
 *   This function is used by `bci_create_segment_points'.
 *
 * in: start
 *     end
 *       [last (if wrap-around segment)]
 *       [first (if wrap-around segment)]
 *
 * uses: bci_get_point_extrema
 *
 * sal: sal_i (start of current segment)
 *      sal_j (current original twilight point)
 *      sal_k (current hinted twilight point)
 *      sal_point_min
 *      sal_point_max
 */

unsigned char FPGM(bci_create_segment) [] = {

  PUSHB_1,
    bci_create_segment,
  FDEF,

  PUSHB_1,
    sal_i,
  RS,
  PUSHB_1,
    2,
  CINDEX,
  WS, /* sal[sal_i] = start */

  /* increase `sal_i'; together with the outer loop, this makes sal_i += 2 */
  PUSHB_3,
    sal_i,
    1,
    sal_i,
  RS,
  ADD, /* sal_i = sal_i + 1 */
  WS,

  /* initialize inner loop(s) */
  PUSHB_2,
    sal_point_min,
    2,
  CINDEX,
  WS, /* sal_point_min = start */
  PUSHB_2,
    sal_point_max,
    2,
  CINDEX,
  WS, /* sal_point_max = start */

  PUSHB_1,
    1,
  SZPS, /* set zp0, zp1, and zp2 to normal zone 1 */

  SWAP,
  DUP,
  PUSHB_1,
    3,
  CINDEX, /* s: start end end start */
  LT, /* start > end */
  IF,
    /* we have a wrap-around segment with two more arguments */
    /* to give the last and first point of the contour, respectively; */
    /* our job is to store a segment `start'-`last', */
    /* and to get extrema for the two segments */
    /* `start'-`last' and `first'-`end' */

    /* s: first last start end */
    PUSHB_1,
      sal_i,
    RS,
    PUSHB_1,
      4,
    CINDEX,
    WS, /* sal[sal_i] = last */

    ROLL,
    ROLL, /* s: first end last start */
    DUP,
    ROLL,
    SWAP, /* s: first end start last start */
    SUB, /* s: first end start loop_count */

    PUSHB_1,
      bci_get_point_extrema,
    LOOPCALL,
    /* clean up stack */
    POP,

    SWAP, /* s: end first */
    PUSHB_1,
      1,
    SUB,
    DUP,
    ROLL, /* s: (first - 1) (first - 1) end */
    SWAP,
    SUB, /* s: (first - 1) loop_count */

    PUSHB_1,
      bci_get_point_extrema,
    LOOPCALL,
    /* clean up stack */
    POP,

  ELSE, /* s: start end */
    PUSHB_1,
      sal_i,
    RS,
    PUSHB_1,
      2,
    CINDEX,
    WS, /* sal[sal_i] = end */

    PUSHB_1,
      2,
    CINDEX,
    SUB, /* s: start loop_count */

    PUSHB_1,
      bci_get_point_extrema,
    LOOPCALL,
    /* clean up stack */
    POP,
  EIF,

  /* the twilight point representing a segment */
  /* is in the middle between the minimum and maximum */
  PUSHB_1,
    sal_point_max,
  RS,
  PUSHB_1,
    sal_point_min,
  RS,
  MD_orig,
  PUSHB_1,
    2*64,
  DIV, /* s: delta */

  PUSHB_4,
    sal_j,
    0,
    0,
    sal_point_min,
  RS,
  MDAP_noround, /* set rp0 and rp1 to `sal_point_min' */
  SZP1, /* set zp1 to twilight zone 0 */
  SZP2, /* set zp2 to twilight zone 0 */

  RS,
  DUP, /* s: delta point[sal_j] point[sal_j] */
  ALIGNRP, /* align `point[sal_j]' with `sal_point_min' */
  PUSHB_1,
    2,
  CINDEX, /* s: delta point[sal_j] delta */
  SHPIX, /* shift `point[sal_j]' by `delta' */

  PUSHB_1,
    sal_k,
  RS,
  DUP, /* s: delta point[sal_k] point[sal_k] */
  ALIGNRP, /* align `point[sal_k]' with `sal_point_min' */
  SWAP,
  SHPIX, /* shift `point[sal_k]' by `delta' */

  PUSHB_6,
    sal_k,
    1,
    sal_k,
    sal_j,
    1,
    sal_j,
  RS,
  ADD, /* original_twilight_point = original_twilight_point + 1 */
  WS,
  RS,
  ADD, /* hinted_twilight_point = hinted_twilight_point + 1 */
  WS,

  ENDF,

};


/*
 * bci_create_segments
 *
 *   Set up segments by defining point ranges which defines them
 *   and computing twilight points to represent them.
 *
 * in: num_segments (N)
 *     segment_start_0
 *     segment_end_0
 *       [contour_last 0 (if wrap-around segment)]
 *       [contour_first 0 (if wrap-around segment)]
 *     segment_start_1
 *     segment_end_1
 *       [contour_last 0 (if wrap-around segment)]
 *       [contour_first 0 (if wrap-around segment)]
 *     ...
 *     segment_start_(N-1)
 *     segment_end_(N-1)
 *       [contour_last (N-1) (if wrap-around segment)]
 *       [contour_first (N-1) (if wrap-around segment)]
 *
 * uses: bci_create_segment
 *
 * sal: sal_i (start of current segment)
 *      sal_j (current original twilight point)
 *      sal_k (current hinted twilight point)
 *      sal_num_segments
 */

unsigned char FPGM(bci_create_segments) [] = {

  PUSHB_1,
    bci_create_segments,
  FDEF,

  /* all our measurements are taken along the y axis */
  SVTCA_y,

  PUSHB_1,
    sal_num_segments,
  SWAP,
  WS, /* sal_num_segments = num_segments */

  PUSHB_7,
    sal_segment_offset,
    sal_segment_offset,
    sal_num_segments,

    sal_k,
    0,
    sal_j,
    sal_num_segments,
  RS,
  WS, /* sal_j = num_segments (offset for original points) */
  WS, /* sal_k = 0 (offset for hinted points) */

  RS,
  DUP,
  ADD,
  ADD,
  PUSHB_1,
    1,
  SUB, /* s: sal_segment_offset (sal_segment_offset + 2*num_segments - 1) */

  /* `bci_create_segment_point' also increases the loop counter by 1; */
  /* this effectively means we have a loop step of 2 */
  PUSHB_2,
    bci_create_segment,
    bci_loop,
  CALL,

  ENDF,

};

unsigned char FPGM(bci_handle_segment) [] = {

  PUSHB_1,
    bci_handle_segment,
  FDEF,

  POP, /* XXX segment */

  ENDF,

};


/*
 * bci_align_segment
 *
 *   Align all points in a segment to the twilight point in rp0.
 *   zp0 and zp1 must be set to 0 (twilight) and 1 (normal), respectively.
 *
 * in: segment_index
 */

unsigned char FPGM(bci_align_segment) [] = {

  PUSHB_1,
    bci_align_segment,
  FDEF,

  /* we need the values of `sal_segment_offset + 2*segment_index' */
  /* and `sal_segment_offset + 2*segment_index + 1' */
  DUP,
  ADD,
  PUSHB_1,
    sal_segment_offset,
  ADD,
  DUP,
  RS,
  SWAP,
  PUSHB_1,
    1,
  ADD,
  RS, /* s: first last */

/* start_loop: */
  PUSHB_1,
    2,
  CINDEX, /* s: first last first */
  PUSHB_1,
    2,
  CINDEX, /* s: first last first last */
  LTEQ, /* first <= end */
  IF, /* s: first last */
    SWAP,
    DUP, /* s: last first first */
    ALIGNRP, /* align point with index `first' with rp0 */

    PUSHB_1,
      1,
    ADD, /* first = first + 1 */
    SWAP, /* s: first last */

    PUSHB_1,
      18,
    NEG,
    JMPR, /* goto start_loop */

  ELSE,
    POP,
    POP,
  EIF,

  ENDF,

};

unsigned char FPGM(bci_handle_segments) [] = {

  PUSHB_1,
    bci_handle_segments,
  FDEF,

  POP, /* XXX first segment */

  PUSHB_1,
    bci_handle_segment,
  LOOPCALL,

  ENDF,

};


/*
 * bci_align_segments
 *
 *   Align segments to the twilight point in rp0.
 *   zp0 and zp1 must be set to 0 (twilight) and 1 (normal), respectively.
 *
 * in: first_segment
 *     loop_counter (N)
 *       segment_1
 *       segment_2
 *       ...
 *       segment_N
 *
 * uses: handle_segment
 *
 */

unsigned char FPGM(bci_align_segments) [] = {

  PUSHB_1,
    bci_align_segments,
  FDEF,

  PUSHB_1,
    bci_align_segment,
  CALL,

  PUSHB_1,
    bci_align_segment,
  LOOPCALL,

  ENDF,

};


/*
 * bci_action_adjust_bound
 *
 *   Handle the ADJUST_BOUND action to align an edge of a stem if the other
 *   edge of the stem has already been moved, then moving it again if
 *   necessary to stay bound.
 *
 * in: edge2_is_serif
 *     edge_is_round
 *     edge_point (in twilight zone)
 *     edge2_point (in twilight zone)
 *     edge[-1] (in twilight zone)
 *     ... stuff for bci_align_segments (edge) ...
 */

unsigned char FPGM(bci_action_adjust_bound) [] = {

  PUSHB_1,
    bci_action_adjust_bound,
  FDEF,

  PUSHB_1,
    0,
  SZPS, /* set zp0, zp1, and zp2 to twilight zone 0 */

  PUSHB_1,
    4,
  CINDEX,
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD, /* s: edge[-1] edge2 edge is_round is_serif edge2_orig */
  PUSHB_1,
    4,
  CINDEX,
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD, /* s: edge[-1] edge2 edge is_round is_serif edge2_orig edge_orig */
  MD_cur, /* s: edge[-1] edge2 edge is_round is_serif org_len */

  PUSHB_1,
    bci_compute_stem_width,
  CALL,
  NEG, /* s: edge[-1] edge2 edge -cur_len */

  ROLL, /* s: edge[-1] edge -cur_len edge2 */
  MDAP_noround, /* set rp0 and rp1 to `edge2' */
  SWAP,
  DUP,
  DUP, /* s: edge[-1] -cur_len edge edge edge */
  ALIGNRP, /* align `edge' with `edge2' */
  ROLL,
  SHPIX, /* shift `edge' by -cur_len */

  SWAP, /* s: edge edge[-1] */
  DUP,
  MDAP_noround, /* set rp0 and rp1 to `edge[-1]' */
  GC_cur,
  PUSHB_1,
    2,
  CINDEX,
  GC_cur, /* s: edge edge[-1]_pos edge_pos */
  GT, /* edge_pos < edge[-1]_pos */
  IF,
    DUP,
    ALIGNRP, /* align `edge' to `edge[-1]' */
  EIF,

  MDAP_noround, /* set rp0 and rp1 to `edge' */

  PUSHB_2,
    bci_align_segments,
    1,
  SZP1, /* set zp1 to normal zone 1 */
  CALL,

  ENDF,

};


/*
 * bci_action_stem_bound
 *
 *   Handle the STEM action to align two edges of a stem, then moving one
 *   edge again if necessary to stay bound.
 *
 *   The code after computing `cur_len' to shift `edge' and `edge2'
 *   is equivalent to the snippet below (part of `ta_latin_hint_edges'):
 *
 *      if cur_len < 96:
 *        if cur_len < = 64:
 *          u_off = 32
 *          d_off = 32
 *        else:
 *          u_off = 38
 *          d_off = 26
 *
 *        org_center = edge_orig + org_len / 2
 *        cur_pos1 = ROUND(org_center)
 *
 *        delta1 = ABS(org_center - (cur_pos1 - u_off))
 *        delta2 = ABS(org_center - (cur_pos1 + d_off))
 *        if (delta1 < delta2):
 *          cur_pos1 = cur_pos1 - u_off
 *        else:
 *          cur_pos1 = cur_pos1 + d_off
 *
 *        edge = cur_pos1 - cur_len / 2
 *
 *      else:
 *        org_pos = anchor + (edge_orig - anchor_orig)
 *        org_center = edge_orig + org_len / 2
 *
 *        cur_pos1 = ROUND(org_pos)
 *        delta1 = ABS(cur_pos1 + cur_len / 2 - org_center)
 *        cur_pos2 = ROUND(org_pos + org_len) - cur_len
 *        delta2 = ABS(cur_pos2 + cur_len / 2 - org_center)
 *
 *        if (delta1 < delta2):
 *          edge = cur_pos1
 *        else:
 *          edge = cur_pos2
 *
 *      edge2 = edge + cur_len
 *
 * in: edge2_is_serif
 *     edge_is_round
 *     edge_point (in twilight zone)
 *     edge2_point (in twilight zone)
 *     edge[-1] (in twilight zone)
 *     ... stuff for bci_align_segments (edge) ...
 *     ... stuff for bci_align_segments (edge2)...
 *
 * sal: sal_anchor
 *      sal_temp1
 *      sal_temp2
 *      sal_temp3
 *      sal_num_segments
 */

#undef sal_u_off
#define sal_u_off sal_temp1
#undef sal_d_off
#define sal_d_off sal_temp2
#undef sal_org_len
#define sal_org_len sal_temp3
#undef sal_edge2
#define sal_edge2 sal_temp3

unsigned char FPGM(bci_action_stem_bound) [] = {

  PUSHB_1,
    bci_action_stem_bound,
  FDEF,

  PUSHB_1,
    0,
  SZPS, /* set zp0, zp1, and zp2 to twilight zone 0 */

  PUSHB_1,
    4,
  CINDEX,
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD,
  PUSHB_1,
    4,
  CINDEX,
  DUP,
  MDAP_noround, /* set rp0 and rp1 to `edge_point' (for ALIGNRP below) */
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD, /* s: edge[-1] edge2 edge is_round is_serif edge2_orig edge_orig */

  MD_cur, /* s: edge[-1] edge2 edge is_round is_serif org_len */
  DUP,
  PUSHB_1,
    sal_org_len,
  SWAP,
  WS,

  PUSHB_1,
    bci_compute_stem_width,
  CALL, /* s: edge[-1] edge2 edge cur_len */

  DUP,
  PUSHB_1,
    96,
  LT, /* cur_len < 96 */
  IF,
    DUP,
    PUSHB_1,
      64,
    LTEQ, /* cur_len <= 64 */
    IF,
      PUSHB_4,
        sal_u_off,
        32,
        sal_d_off,
        32,

    ELSE,
      PUSHB_4,
        sal_u_off,
        38,
        sal_d_off,
        26,
    EIF,
    WS,
    WS,

    SWAP, /* s: edge[-1] edge2 cur_len edge */
    DUP,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD, /* s: edge[-1] edge2 cur_len edge edge_orig */

    GC_cur,
    PUSHB_1,
      sal_org_len,
    RS,
    PUSHB_1,
      2*64,
    DIV,
    ADD, /* s: edge[-1] edge2 cur_len edge org_center */

    DUP,
    PUSHB_1,
      bci_round,
    CALL, /* s: edge[-1] edge2 cur_len edge org_center cur_pos1 */

    DUP,
    ROLL,
    ROLL,
    SUB, /* s: ... cur_len edge cur_pos1 (org_center - cur_pos1) */

    DUP,
    PUSHB_1,
      sal_u_off,
    RS,
    ADD,
    ABS, /* s: ... cur_len edge cur_pos1 (org_center - cur_pos1) delta1 */

    SWAP,
    PUSHB_1,
      sal_d_off,
    RS,
    SUB,
    ABS, /* s: edge[-1] edge2 cur_len edge cur_pos1 delta1 delta2 */

    LT, /* delta1 < delta2 */
    IF,
      PUSHB_1,
        sal_u_off,
      RS,
      SUB, /* cur_pos1 = cur_pos1 - u_off */

    ELSE,
      PUSHB_1,
        sal_d_off,
      RS,
      ADD, /* cur_pos1 = cur_pos1 + d_off */
    EIF, /* s: edge[-1] edge2 cur_len edge cur_pos1 */

    PUSHB_1,
      3,
    CINDEX,
    PUSHB_1,
      2*64,
    DIV,
    SUB, /* arg = cur_pos1 - cur_len/2 */

    SWAP, /* s: edge[-1] edge2 cur_len arg edge */
    DUP,
    DUP,
    PUSHB_1,
      4,
    MINDEX,
    SWAP, /* s: edge[-1] edge2 cur_len edge edge arg edge */
    GC_cur,
    SUB,
    SHPIX, /* edge = cur_pos1 - cur_len/2 */

  ELSE,
    SWAP, /* s: edge[-1] edge2 cur_len edge */
    DUP,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD, /* s: edge[-1] edge2 cur_len edge edge_orig */

    GC_cur,
    PUSHB_1,
      sal_org_len,
    RS,
    PUSHB_1,
      2*64,
    DIV,
    ADD, /* s: edge[-1] edge2 cur_len edge org_center */

    PUSHB_1,
      sal_anchor,
    RS,
    GC_cur, /* s: edge[-1] edge2 cur_len edge org_center anchor_pos */
    PUSHB_1,
      3,
    CINDEX,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD,
    PUSHB_1,
      sal_anchor,
    RS,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD,
    MD_cur,
    ADD, /* s: edge[-1] edge2 cur_len edge org_center org_pos */

    DUP,
    PUSHB_1,
      bci_round,
    CALL, /* cur_pos1 = ROUND(org_pos) */
    SWAP,
    PUSHB_1,
      sal_org_len,
    RS,
    ADD,
    PUSHB_1,
      bci_round,
    CALL,
    PUSHB_1,
      5,
    CINDEX,
    SUB, /* s: edge[-1] edge2 cur_len edge org_center cur_pos1 cur_pos2 */

    PUSHB_1,
      5,
    CINDEX,
    PUSHB_1,
      2*64,
    DIV,
    PUSHB_1,
      4,
    MINDEX,
    SUB, /* s: ... cur_len edge cur_pos1 cur_pos2 (cur_len/2 - org_center) */

    DUP,
    PUSHB_1,
      4,
    CINDEX,
    ADD,
    ABS, /* delta1 = ABS(cur_pos1 + cur_len / 2 - org_center) */
    SWAP,
    PUSHB_1,
      3,
    CINDEX,
    ADD,
    ABS, /* s: ... edge2 cur_len edge cur_pos1 cur_pos2 delta1 delta2 */
    LT, /* delta1 < delta2 */
    IF,
      POP, /* arg = cur_pos1 */
    ELSE,
      SWAP,
      POP, /* arg = cur_pos2 */
    EIF, /* s: edge[-1] edge2 cur_len edge arg */
    SWAP,
    DUP,
    DUP,
    PUSHB_1,
      4,
    MINDEX,
    SWAP, /* s: edge[-1] edge2 cur_len edge edge arg edge */
    GC_cur,
    SUB,
    SHPIX, /* edge = arg */
  EIF, /* s: edge[-1] edge2 cur_len edge */

  ROLL, /* s: edge[-1] cur_len edge edge2 */
  DUP,
  DUP,
  ALIGNRP, /* align `edge2' with rp0 (still `edge') */
  PUSHB_1,
    sal_edge2,
  SWAP,
  WS, /* s: edge[-1] cur_len edge edge2 */
  ROLL,
  SHPIX, /* edge2 = edge + cur_len */

  SWAP, /* s: edge edge[-1] */
  DUP,
  MDAP_noround, /* set rp0 and rp1 to `edge[-1]' */
  GC_cur,
  PUSHB_1,
    2,
  CINDEX,
  GC_cur, /* s: edge edge[-1]_pos edge_pos */
  GT, /* edge_pos < edge[-1]_pos */
  IF,
    DUP,
    ALIGNRP, /* align `edge' to `edge[-1]' */
  EIF,

  MDAP_noround, /* set rp0 and rp1 to `edge' */

  PUSHB_2,
    bci_align_segments,
    1,
  SZP1, /* set zp1 to normal zone 1 */
  CALL,

  PUSHB_1,
    sal_edge2,
  RS,
  MDAP_noround, /* set rp0 and rp1 to `edge2' */

  PUSHB_1,
    bci_align_segments,
  CALL,

  ENDF,

};


/*
 * bci_action_link
 *
 *   Handle the LINK action to link an edge to another one.
 *
 * in: stem_is_serif
 *     base_is_round
 *     base_point (in twilight zone)
 *     stem_point (in twilight zone)
 *     ... stuff for bci_align_segments (base) ...
 */

unsigned char FPGM(bci_action_link) [] = {

  PUSHB_1,
    bci_action_link,
  FDEF,

  PUSHB_1,
    0,
  SZPS, /* set zp0, zp1, and zp2 to twilight zone 0 */

  PUSHB_1,
    4,
  CINDEX,
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD,
  PUSHB_1,
    4,
  MINDEX,
  DUP,
  MDAP_noround, /* set rp0 and rp1 to `base_point' (for ALIGNRP below) */
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD, /* s: stem is_round is_serif stem_orig base_orig */

  MD_cur, /* s: stem is_round is_serif dist_orig */

  PUSHB_1,
    bci_compute_stem_width,
  CALL, /* s: stem new_dist */

  SWAP,
  DUP,
  ALIGNRP, /* align `stem_point' with `base_point' */
  DUP,
  MDAP_noround, /* set rp0 and rp1 to `stem_point' */
  SWAP,
  SHPIX, /* stem_point = base_point + new_dist */

  PUSHB_2,
    bci_align_segments,
    1,
  SZP1, /* set zp1 to normal zone 1 */
  CALL,

  ENDF,

};


/*
 * bci_action_anchor
 *
 *   Handle the ANCHOR action to align two edges
 *   and to set the edge anchor.
 *
 *   The code after computing `cur_len' to shift `edge' and `edge2'
 *   is equivalent to the snippet below (part of `ta_latin_hint_edges'):
 *
 *      if cur_len < 96:
 *        if cur_len < = 64:
 *          u_off = 32
 *          d_off = 32
 *        else:
 *          u_off = 38
 *          d_off = 26
 *
 *        org_center = edge_orig + org_len / 2
 *        cur_pos1 = ROUND(org_center)
 *
 *        error1 = ABS(org_center - (cur_pos1 - u_off))
 *        error2 = ABS(org_center - (cur_pos1 + d_off))
 *        if (error1 < error2):
 *          cur_pos1 = cur_pos1 - u_off
 *        else:
 *          cur_pos1 = cur_pos1 + d_off
 *
 *        edge = cur_pos1 - cur_len / 2
 *        edge2 = edge + cur_len
 *
 *      else:
 *        edge = ROUND(edge_orig)
 *
 * in: edge2_is_serif
 *     edge_is_round
 *     edge_point (in twilight zone)
 *     edge2_point (in twilight zone)
 *     ... stuff for bci_align_segments (edge) ...
 *
 * sal: sal_anchor
 *      sal_temp1
 *      sal_temp2
 *      sal_temp3
 */

#undef sal_u_off
#define sal_u_off sal_temp1
#undef sal_d_off
#define sal_d_off sal_temp2
#undef sal_org_len
#define sal_org_len sal_temp3

unsigned char FPGM(bci_action_anchor) [] = {

  PUSHB_1,
    bci_action_anchor,
  FDEF,

  /* store anchor point number in `sal_anchor' */
  PUSHB_2,
    sal_anchor,
    4,
  CINDEX,
  WS, /* sal_anchor = edge_point */

  PUSHB_1,
    0,
  SZPS, /* set zp0, zp1, and zp2 to twilight zone 0 */

  PUSHB_1,
    4,
  CINDEX,
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD,
  PUSHB_1,
    4,
  CINDEX,
  DUP,
  MDAP_noround, /* set rp0 and rp1 to `edge_point' (for ALIGNRP below) */
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD, /* s: edge2 edge is_round is_serif edge2_orig edge_orig */

  MD_cur, /* s: edge2 edge is_round is_serif org_len */
  DUP,
  PUSHB_1,
    sal_org_len,
  SWAP,
  WS,

  PUSHB_1,
    bci_compute_stem_width,
  CALL, /* s: edge2 edge cur_len */

  DUP,
  PUSHB_1,
    96,
  LT, /* cur_len < 96 */
  IF,
    DUP,
    PUSHB_1,
      64,
    LTEQ, /* cur_len <= 64 */
    IF,
      PUSHB_4,
        sal_u_off,
        32,
        sal_d_off,
        32,

    ELSE,
      PUSHB_4,
        sal_u_off,
        38,
        sal_d_off,
        26,
    EIF,
    WS,
    WS,

    SWAP, /* s: edge2 cur_len edge */
    DUP,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD, /* s: edge2 cur_len edge edge_orig */

    GC_cur,
    PUSHB_1,
      sal_org_len,
    RS,
    PUSHB_1,
      2*64,
    DIV,
    ADD, /* s: edge2 cur_len edge org_center */

    DUP,
    PUSHB_1,
      bci_round,
    CALL, /* s: edge2 cur_len edge org_center cur_pos1 */

    DUP,
    ROLL,
    ROLL,
    SUB, /* s: edge2 cur_len edge cur_pos1 (org_center - cur_pos1) */

    DUP,
    PUSHB_1,
      sal_u_off,
    RS,
    ADD,
    ABS, /* s: edge2 cur_len edge cur_pos1 (org_center - cur_pos1) error1 */

    SWAP,
    PUSHB_1,
      sal_d_off,
    RS,
    SUB,
    ABS, /* s: edge2 cur_len edge cur_pos1 error1 error2 */

    LT, /* error1 < error2 */
    IF,
      PUSHB_1,
        sal_u_off,
      RS,
      SUB, /* cur_pos1 = cur_pos1 - u_off */

    ELSE,
      PUSHB_1,
        sal_d_off,
      RS,
      ADD, /* cur_pos1 = cur_pos1 + d_off */
    EIF, /* s: edge2 cur_len edge cur_pos1 */

    PUSHB_1,
      3,
    CINDEX,
    PUSHB_1,
      2*64,
    DIV,
    SUB, /* s: edge2 cur_len edge (cur_pos1 - cur_len/2) */

    PUSHB_1,
      2,
    CINDEX, /* s: edge2 cur_len edge (cur_pos1 - cur_len/2) edge */
    GC_cur,
    SUB,
    SHPIX, /* edge = cur_pos1 - cur_len/2 */

    SWAP, /* s: cur_len edge2 */
    DUP,
    ALIGNRP, /* align `edge2' with rp0 (still `edge') */
    SWAP,
    SHPIX, /* edge2 = edge1 + cur_len */

  ELSE,
    POP, /* s: edge2 edge */
    DUP,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD, /* s: edge2 edge edge_orig */

    MDAP_noround, /* set rp0 and rp1 to `edge_orig' */
    DUP,
    ALIGNRP, /* align `edge' with `edge_orig' */
    MDAP_round, /* round `edge' */

    /* clean up stack */
    POP,
  EIF,

  PUSHB_2,
    bci_align_segments,
    1,
  SZP1, /* set zp1 to normal zone 1 */
  CALL,

  ENDF,

};


/*
 * bci_action_blue_anchor
 *
 *   Handle the BLUE_ANCHOR action to align an edge with a blue zone
 *   and to set the edge anchor.
 *
 * in: anchor_point (in twilight zone)
 *     blue_cvt_idx
 *     edge_point (in twilight zone)
 *     ... stuff for bci_align_segments (edge) ...
 *
 * sal: sal_anchor
 */

unsigned char FPGM(bci_action_blue_anchor) [] = {

  PUSHB_1,
    bci_action_blue_anchor,
  FDEF,

  /* store anchor point number in `sal_anchor' */
  PUSHB_1,
    sal_anchor,
  SWAP,
  WS,

  PUSHB_1,
    0,
  SZP0, /* set zp0 to twilight zone 0 */

  /* move `edge_point' to `blue_cvt_idx' position */
  MIAP_noround, /* this also sets rp0 */

  PUSHB_2,
    bci_align_segments,
    1,
  SZP1, /* set zp1 to normal zone 1 */
  CALL,

  ENDF,

};


/*
 * bci_action_adjust
 *
 *   Handle the ADJUST action to align an edge of a stem if the other edge
 *   of the stem has already been moved.
 *
 * in: edge2_is_serif
 *     edge_is_round
 *     edge_point (in twilight zone)
 *     edge2_point (in twilight zone)
 *     ... stuff for bci_align_segments (edge) ...
 */

unsigned char FPGM(bci_action_adjust) [] = {

  PUSHB_1,
    bci_action_adjust,
  FDEF,

  PUSHB_1,
    0,
  SZPS, /* set zp0, zp1, and zp2 to twilight zone 0 */

  PUSHB_1,
    4,
  CINDEX,
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD, /* s: edge2 edge is_round is_serif edge2_orig */
  PUSHB_1,
    4,
  CINDEX,
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD, /* s: edge2 edge is_round is_serif edge2_orig edge_orig */
  MD_cur, /* s: edge2 edge is_round is_serif org_len */

  PUSHB_1,
    bci_compute_stem_width,
  CALL,
  NEG, /* s: edge2 edge -cur_len */

  ROLL,
  MDAP_noround, /* set rp0 and rp1 to `edge2' */
  SWAP,
  DUP,
  DUP, /* s: -cur_len edge edge edge */
  ALIGNRP, /* align `edge' with `edge2' */
  ROLL,
  SHPIX, /* shift `edge' by -cur_len */

  MDAP_noround, /* set rp0 and rp1 to `edge' */

  PUSHB_2,
    bci_align_segments,
    1,
  SZP1, /* set zp1 to normal zone 1 */
  CALL,

  ENDF,

};


/*
 * bci_action_stem
 *
 *   Handle the STEM action to align two edges of a stem.
 *
 *   The code after computing `cur_len' to shift `edge' and `edge2'
 *   is equivalent to the snippet below (part of `ta_latin_hint_edges'):
 *
 *      if cur_len < 96:
 *        if cur_len < = 64:
 *          u_off = 32
 *          d_off = 32
 *        else:
 *          u_off = 38
 *          d_off = 26
 *
 *        org_center = edge_orig + org_len / 2
 *        cur_pos1 = ROUND(org_center)
 *
 *        delta1 = ABS(org_center - (cur_pos1 - u_off))
 *        delta2 = ABS(org_center - (cur_pos1 + d_off))
 *        if (delta1 < delta2):
 *          cur_pos1 = cur_pos1 - u_off
 *        else:
 *          cur_pos1 = cur_pos1 + d_off
 *
 *        edge = cur_pos1 - cur_len / 2
 *
 *      else:
 *        org_pos = anchor + (edge_orig - anchor_orig)
 *        org_center = edge_orig + org_len / 2
 *
 *        cur_pos1 = ROUND(org_pos)
 *        delta1 = ABS(cur_pos1 + cur_len / 2 - org_center)
 *        cur_pos2 = ROUND(org_pos + org_len) - cur_len
 *        delta2 = ABS(cur_pos2 + cur_len / 2 - org_center)
 *
 *        if (delta1 < delta2):
 *          edge = cur_pos1
 *        else:
 *          edge = cur_pos2
 *
 *      edge2 = edge + cur_len
 *
 * in: edge2_is_serif
 *     edge_is_round
 *     edge_point (in twilight zone)
 *     edge2_point (in twilight zone)
 *     ... stuff for bci_align_segments (edge) ...
 *     ... stuff for bci_align_segments (edge2)...
 *
 * sal: sal_anchor
 *      sal_temp1
 *      sal_temp2
 *      sal_temp3
 *      sal_num_segments
 */

#undef sal_u_off
#define sal_u_off sal_temp1
#undef sal_d_off
#define sal_d_off sal_temp2
#undef sal_org_len
#define sal_org_len sal_temp3
#undef sal_edge2
#define sal_edge2 sal_temp3

unsigned char FPGM(bci_action_stem) [] = {

  PUSHB_1,
    bci_action_stem,
  FDEF,

  PUSHB_1,
    0,
  SZPS, /* set zp0, zp1, and zp2 to twilight zone 0 */

  PUSHB_1,
    4,
  CINDEX,
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD,
  PUSHB_1,
    4,
  CINDEX,
  DUP,
  MDAP_noround, /* set rp0 and rp1 to `edge_point' (for ALIGNRP below) */
  PUSHB_1,
    sal_num_segments,
  RS,
  ADD, /* s: edge2 edge is_round is_serif edge2_orig edge_orig */

  MD_cur, /* s: edge2 edge is_round is_serif org_len */
  DUP,
  PUSHB_1,
    sal_org_len,
  SWAP,
  WS,

  PUSHB_1,
    bci_compute_stem_width,
  CALL, /* s: edge2 edge cur_len */

  DUP,
  PUSHB_1,
    96,
  LT, /* cur_len < 96 */
  IF,
    DUP,
    PUSHB_1,
      64,
    LTEQ, /* cur_len <= 64 */
    IF,
      PUSHB_4,
        sal_u_off,
        32,
        sal_d_off,
        32,

    ELSE,
      PUSHB_4,
        sal_u_off,
        38,
        sal_d_off,
        26,
    EIF,
    WS,
    WS,

    SWAP, /* s: edge2 cur_len edge */
    DUP,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD, /* s: edge2 cur_len edge edge_orig */

    GC_cur,
    PUSHB_1,
      sal_org_len,
    RS,
    PUSHB_1,
      2*64,
    DIV,
    ADD, /* s: edge2 cur_len edge org_center */

    DUP,
    PUSHB_1,
      bci_round,
    CALL, /* s: edge2 cur_len edge org_center cur_pos1 */

    DUP,
    ROLL,
    ROLL,
    SUB, /* s: edge2 cur_len edge cur_pos1 (org_center - cur_pos1) */

    DUP,
    PUSHB_1,
      sal_u_off,
    RS,
    ADD,
    ABS, /* s: ... cur_len edge cur_pos1 (org_center - cur_pos1) delta1 */

    SWAP,
    PUSHB_1,
      sal_d_off,
    RS,
    SUB,
    ABS, /* s: edge2 cur_len edge cur_pos1 delta1 delta2 */

    LT, /* delta1 < delta2 */
    IF,
      PUSHB_1,
        sal_u_off,
      RS,
      SUB, /* cur_pos1 = cur_pos1 - u_off */

    ELSE,
      PUSHB_1,
        sal_d_off,
      RS,
      ADD, /* cur_pos1 = cur_pos1 + d_off */
    EIF, /* s: edge2 cur_len edge cur_pos1 */

    PUSHB_1,
      3,
    CINDEX,
    PUSHB_1,
      2*64,
    DIV,
    SUB, /* arg = cur_pos1 - cur_len/2 */

    SWAP, /* s: edge2 cur_len arg edge */
    DUP,
    PUSHB_1,
      3,
    MINDEX,
    SWAP, /* s: edge2 cur_len edge arg edge */
    GC_cur,
    SUB,
    SHPIX, /* edge = cur_pos1 - cur_len/2 */

  ELSE,
    SWAP, /* s: edge2 cur_len edge */
    DUP,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD, /* s: edge2 cur_len edge edge_orig */

    GC_cur,
    PUSHB_1,
      sal_org_len,
    RS,
    PUSHB_1,
      2*64,
    DIV,
    ADD, /* s: edge2 cur_len edge org_center */

    PUSHB_1,
      sal_anchor,
    RS,
    GC_cur, /* s: edge2 cur_len edge org_center anchor_pos */
    PUSHB_1,
      3,
    CINDEX,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD,
    PUSHB_1,
      sal_anchor,
    RS,
    PUSHB_1,
      sal_num_segments,
    RS,
    ADD,
    MD_cur,
    ADD, /* s: edge2 cur_len edge org_center org_pos */

    DUP,
    PUSHB_1,
      bci_round,
    CALL, /* cur_pos1 = ROUND(org_pos) */
    SWAP,
    PUSHB_1,
      sal_org_len,
    RS,
    ADD,
    PUSHB_1,
      bci_round,
    CALL,
    PUSHB_1,
      5,
    CINDEX,
    SUB, /* s: edge2 cur_len edge org_center cur_pos1 cur_pos2 */

    PUSHB_1,
      5,
    CINDEX,
    PUSHB_1,
      2*64,
    DIV,
    PUSHB_1,
      4,
    MINDEX,
    SUB, /* s: ... cur_len edge cur_pos1 cur_pos2 (cur_len/2 - org_center) */

    DUP,
    PUSHB_1,
      4,
    CINDEX,
    ADD,
    ABS, /* delta1 = ABS(cur_pos1 + cur_len / 2 - org_center) */
    SWAP,
    PUSHB_1,
      3,
    CINDEX,
    ADD,
    ABS, /* s: edge2 cur_len edge cur_pos1 cur_pos2 delta1 delta2 */
    LT, /* delta1 < delta2 */
    IF,
      POP, /* arg = cur_pos1 */
    ELSE,
      SWAP,
      POP, /* arg = cur_pos2 */
    EIF, /* s: edge2 cur_len edge arg */
    SWAP,
    DUP,
    PUSHB_1,
      3,
    MINDEX,
    SWAP, /* s: edge2 cur_len edge arg edge */
    GC_cur,
    SUB,
    SHPIX, /* edge = arg */
  EIF, /* s: edge2 cur_len */

  SWAP, /* s: cur_len edge2 */
  DUP,
  DUP,
  ALIGNRP, /* align `edge2' with rp0 (still `edge') */
  PUSHB_1,
    sal_edge2,
  SWAP,
  WS, /* s: cur_len edge2 */
  SWAP,
  SHPIX, /* edge2 = edge + cur_len */

  PUSHB_2,
    bci_align_segments,
    1,
  SZP1, /* set zp1 to normal zone 1 */
  CALL,

  PUSHB_1,
    sal_edge2,
  RS,
  MDAP_noround, /* set rp0 and rp1 to `edge2' */

  PUSHB_1,
    bci_align_segments,
  CALL,
  ENDF,

};


/*
 * bci_action_blue
 *
 *   Handle the BLUE action to align an edge with a blue zone.
 *
 * in: blue_cvt_idx
 *     edge_point (in twilight zone)
 *     ... stuff for bci_align_segments (edge) ...
 */

unsigned char FPGM(bci_action_blue) [] = {

  PUSHB_1,
    bci_action_blue,
  FDEF,

  PUSHB_1,
    0,
  SZP0, /* set zp0 to twilight zone 0 */

  /* move `edge_point' to `blue_cvt_idx' position */
  MIAP_noround, /* this also sets rp0 */

  PUSHB_2,
    bci_align_segments,
    1,
  SZP1, /* set zp1 to normal zone 1 */
  CALL,

  ENDF,

};

unsigned char FPGM(bci_action_serif) [] = {

  PUSHB_1,
    bci_action_serif,
  FDEF,

  PUSHB_1,
    bci_handle_segments,
  CALL,

  /* XXX */

  ENDF,

};

unsigned char FPGM(bci_action_serif_anchor) [] = {

  PUSHB_1,
    bci_action_serif_anchor,
  FDEF,

  PUSHB_1,
    bci_handle_segments,
  CALL,

  /* XXX */

  ENDF,

};

unsigned char FPGM(bci_action_serif_link1) [] = {

  PUSHB_1,
    bci_action_serif_link1,
  FDEF,

  PUSHB_1,
    bci_handle_segments,
  CALL,

  /* XXX */

  ENDF,

};

unsigned char FPGM(bci_action_serif_link2) [] = {

  PUSHB_1,
    bci_action_serif_link2,
  FDEF,

  PUSHB_1,
    bci_handle_segments,
  CALL,

  /* XXX */

  ENDF,

};


/*
 * bci_handle_action
 *
 *   Execute function.
 *
 * in: function_index
 */

unsigned char FPGM(bci_handle_action) [] = {

  PUSHB_1,
    bci_handle_action,
  FDEF,

  CALL,

  ENDF,

};


/*
 * bci_hint_glyph
 *
 *   This is the top-level glyph hinting function
 *   which parses the arguments on the stack and calls subroutines.
 *
 * in: num_actions (M)
 *       action_0_func_idx
 *         ... data ...
 *       action_1_func_idx
 *         ... data ...
 *       ...
 *       action_M_func_idx
 *         ... data ...
 *
 * uses: bci_handle_action
 *       bci_action_adjust_bound
 *       bci_action_stem_bound
 *
 *       bci_action_link
 *       bci_action_anchor
 *       bci_action_blue_anchor
 *       bci_action_adjust
 *       bci_action_stem
 *
 *       bci_action_blue
 *       bci_action_serif
 *       bci_action_serif_anchor
 *       bci_action_serif_link1
 *       bci_action_serif_link2
 */

unsigned char FPGM(bci_hint_glyph) [] = {

  PUSHB_1,
    bci_hint_glyph,
  FDEF,

  PUSHB_1,
    bci_handle_action,
  LOOPCALL,

  ENDF,

};


#define COPY_FPGM(func_name) \
          memcpy(buf_p, fpgm_ ## func_name, \
                 sizeof (fpgm_ ## func_name)); \
          buf_p += sizeof (fpgm_ ## func_name) \

static FT_Error
TA_table_build_fpgm(FT_Byte** fpgm,
                    FT_ULong* fpgm_len,
                    FONT* font)
{
  FT_UInt buf_len;
  FT_UInt len;
  FT_Byte* buf;
  FT_Byte* buf_p;


  buf_len = sizeof (FPGM(bci_round))
            + sizeof (FPGM(bci_compute_stem_width_a))
            + 1
            + sizeof (FPGM(bci_compute_stem_width_b))
            + 1
            + sizeof (FPGM(bci_compute_stem_width_c))
            + sizeof (FPGM(bci_loop))
            + sizeof (FPGM(bci_cvt_rescale))
            + sizeof (FPGM(bci_blue_round_a))
            + 1
            + sizeof (FPGM(bci_blue_round_b))
            + sizeof (FPGM(bci_get_point_extrema))
            + sizeof (FPGM(bci_create_segment))
            + sizeof (FPGM(bci_create_segments))
            + sizeof (FPGM(bci_handle_segment))
            + sizeof (FPGM(bci_align_segment))
            + sizeof (FPGM(bci_handle_segments))
            + sizeof (FPGM(bci_align_segments))
            + sizeof (FPGM(bci_action_adjust_bound))
            + sizeof (FPGM(bci_action_stem_bound))
            + sizeof (FPGM(bci_action_link))
            + sizeof (FPGM(bci_action_anchor))
            + sizeof (FPGM(bci_action_blue_anchor))
            + sizeof (FPGM(bci_action_adjust))
            + sizeof (FPGM(bci_action_stem))
            + sizeof (FPGM(bci_action_blue))
            + sizeof (FPGM(bci_action_serif))
            + sizeof (FPGM(bci_action_serif_anchor))
            + sizeof (FPGM(bci_action_serif_link1))
            + sizeof (FPGM(bci_action_serif_link2))
            + sizeof (FPGM(bci_handle_action))
            + sizeof (FPGM(bci_hint_glyph));
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

  COPY_FPGM(bci_round);
  COPY_FPGM(bci_compute_stem_width_a);
  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
  COPY_FPGM(bci_compute_stem_width_b);
  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
  COPY_FPGM(bci_compute_stem_width_c);
  COPY_FPGM(bci_loop);
  COPY_FPGM(bci_cvt_rescale);
  COPY_FPGM(bci_blue_round_a);
  *(buf_p++) = (unsigned char)CVT_BLUES_SIZE(font);
  COPY_FPGM(bci_blue_round_b);
  COPY_FPGM(bci_get_point_extrema);
  COPY_FPGM(bci_create_segment);
  COPY_FPGM(bci_create_segments);
  COPY_FPGM(bci_handle_segment);
  COPY_FPGM(bci_align_segment);
  COPY_FPGM(bci_handle_segments);
  COPY_FPGM(bci_align_segments);
  COPY_FPGM(bci_action_adjust_bound);
  COPY_FPGM(bci_action_stem_bound);
  COPY_FPGM(bci_action_link);
  COPY_FPGM(bci_action_anchor);
  COPY_FPGM(bci_action_blue_anchor);
  COPY_FPGM(bci_action_adjust);
  COPY_FPGM(bci_action_stem);
  COPY_FPGM(bci_action_blue);
  COPY_FPGM(bci_action_serif);
  COPY_FPGM(bci_action_serif_anchor);
  COPY_FPGM(bci_action_serif_link1);
  COPY_FPGM(bci_action_serif_link2);
  COPY_FPGM(bci_handle_action);
  COPY_FPGM(bci_hint_glyph);

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

  return FT_Err_Ok;
}


/* the `prep' instructions */

#define PREP(snippet_name) prep_ ## snippet_name

/* we often need 0x10000 which can't be pushed directly onto the stack, */
/* thus we provide it in the storage area */

unsigned char PREP(store_0x10000) [] = {

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

unsigned char PREP(align_top_a) [] = {

  /* optimize the alignment of the top of small letters to the pixel grid */

  PUSHB_1,

};

/*  %c, index of alignment blue zone */

unsigned char PREP(align_top_b) [] = {

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
    PUSHB_1,
      sal_0x10000,
    RS,
    MUL, /* scaled in 16.16 format */
    SWAP,
    DIV, /* (fitted / scaled) in 16.16 format */

    PUSHB_1,
      sal_scale,
    SWAP,
    WS,

};

unsigned char PREP(loop_cvt_a) [] = {

    /* loop over vertical CVT entries */
    PUSHB_4,

};

/*    %c, first vertical index */
/*    %c, last vertical index */

unsigned char PREP(loop_cvt_b) [] = {

      bci_cvt_rescale,
      bci_loop,
    CALL,

    /* loop over blue refs */
    PUSHB_4,

};

/*    %c, first blue ref index */
/*    %c, last blue ref index */

unsigned char PREP(loop_cvt_c) [] = {

      bci_cvt_rescale,
      bci_loop,
    CALL,

    /* loop over blue shoots */
    PUSHB_4,

};

/*    %c, first blue shoot index */
/*    %c, last blue shoot index */

unsigned char PREP(loop_cvt_d) [] = {

      bci_cvt_rescale,
      bci_loop,
    CALL,
  EIF,

};

unsigned char PREP(compute_extra_light_a) [] = {

  /* compute (vertical) `extra_light' flag */
  PUSHB_3,
    sal_is_extra_light,
    40,

};

/*  %c, index of vertical standard_width */

unsigned char PREP(compute_extra_light_b) [] = {

  RCVT,
  GT, /* standard_width < 40 */
  WS,

};

unsigned char PREP(round_blues_a) [] = {

  /* use discrete values for blue zone widths */
  PUSHB_4,

};

/*  %c, first blue ref index */
/*  %c, last blue ref index */

unsigned char PREP(round_blues_b) [] = {

    bci_blue_round,
    bci_loop,
  CALL

};

/* XXX talatin.c: 1671 */
/* XXX talatin.c: 1708 */
/* XXX talatin.c: 2182 */


#define COPY_PREP(snippet_name) \
          memcpy(buf_p, prep_ ## snippet_name, \
                 sizeof (prep_ ## snippet_name)); \
          buf_p += sizeof (prep_ ## snippet_name);

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

  buf_len = sizeof (PREP(store_0x10000));

  if (blue_adjustment)
    buf_len += sizeof (PREP(align_top_a))
               + 1
               + sizeof (PREP(align_top_b))
               + sizeof (PREP(loop_cvt_a))
               + 2
               + sizeof (PREP(loop_cvt_b))
               + 2
               + sizeof (PREP(loop_cvt_c))
               + 2
               + sizeof (PREP(loop_cvt_d));

  buf_len += sizeof (PREP(compute_extra_light_a))
             + 1
             + sizeof (PREP(compute_extra_light_b));

  if (CVT_BLUES_SIZE(font))
    buf_len += sizeof (PREP(round_blues_a))
               + 2
               + sizeof (PREP(round_blues_b));

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

  COPY_PREP(store_0x10000);

  if (blue_adjustment)
  {
    COPY_PREP(align_top_a);
    *(buf_p++) = (unsigned char)(CVT_BLUE_SHOOTS_OFFSET(font)
                                 + blue_adjustment - vaxis->blues);
    COPY_PREP(align_top_b);

    COPY_PREP(loop_cvt_a);
    *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_VERT_WIDTHS_OFFSET(font)
                                 + CVT_VERT_WIDTHS_SIZE(font) - 1);
    COPY_PREP(loop_cvt_b);
    *(buf_p++) = (unsigned char)CVT_BLUE_REFS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_REFS_OFFSET(font)
                                 + CVT_BLUES_SIZE(font) - 1);
    COPY_PREP(loop_cvt_c);
    *(buf_p++) = (unsigned char)CVT_BLUE_SHOOTS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_SHOOTS_OFFSET(font)
                                 + CVT_BLUES_SIZE(font) - 1);
    COPY_PREP(loop_cvt_d);
  }

  COPY_PREP(compute_extra_light_a);
  *(buf_p++) = (unsigned char)CVT_VERT_STANDARD_WIDTH_OFFSET(font);
  COPY_PREP(compute_extra_light_b);

  if (CVT_BLUES_SIZE(font))
  {
    COPY_PREP(round_blues_a);
    *(buf_p++) = (unsigned char)CVT_BLUE_REFS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_REFS_OFFSET(font)
                                 + CVT_BLUES_SIZE(font) - 1);
    COPY_PREP(round_blues_b);
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

  return FT_Err_Ok;
}


/* we store the segments in the storage area; */
/* each segment record consists of the first and last point */

static FT_Byte*
TA_sfnt_build_glyph_segments(SFNT* sfnt,
                             Recorder* recorder,
                             FT_Byte* bufp)
{
  FONT* font = recorder->font;
  TA_GlyphHints hints = &font->loader->hints;
  TA_AxisHints axis = &hints->axis[TA_DIMENSION_VERT];
  TA_Point points = hints->points;
  TA_Segment segments = axis->segments;
  TA_Segment seg;
  TA_Segment seg_limit;

  FT_Outline outline = font->loader->gloader->base.outline;

  FT_UInt* args;
  FT_UInt* arg;
  FT_UInt num_args;
  FT_UInt nargs;
  FT_UInt num_segments;

  FT_UInt* wrap_around_segment;
  FT_UInt num_wrap_around_segments;

  FT_Bool need_words = 0;

  FT_Int n;
  FT_UInt i, j;
  FT_UInt num_storage;
  FT_UInt num_stack_elements;
  FT_UInt num_twilight_points;


  seg_limit = segments + axis->num_segments;
  num_segments = axis->num_segments;

  /* some segments can `wrap around' */
  /* a contour's start point like 24-25-26-0-1-2 */
  /* (there can be at most one such segment per contour); */
  /* we thus append additional records to split them into 24-26 and 0-2 */
  wrap_around_segment = recorder->wrap_around_segments;
  for (seg = segments; seg < seg_limit; seg++)
    if (seg->first > seg->last)
    {
      /* the stored data is used later for edge linking */
      *(wrap_around_segment++) = seg - segments;
    }

  num_wrap_around_segments = wrap_around_segment
                             - recorder->wrap_around_segments;
  num_segments += num_wrap_around_segments;

  /* wrap-around segments are pushed with four arguments */
  num_args = 2 * num_segments + 2 * num_wrap_around_segments + 2;

  /* collect all arguments temporarily in an array (in reverse order) */
  /* so that we can easily split into chunks of 255 args */
  /* as needed by NPUSHB and NPUSHW, respectively */
  args = (FT_UInt*)malloc(num_args * sizeof (FT_UInt));
  if (!args)
    return NULL;

  arg = args + num_args - 1;

  if (num_segments > 0xFF)
    need_words = 1;

  *(arg--) = bci_create_segments;
  *(arg--) = num_segments;

  for (seg = segments; seg < seg_limit; seg++)
  {
    FT_UInt first = seg->first - points;
    FT_UInt last = seg->last - points;


    *(arg--) = first;
    *(arg--) = last;

    /* we push the last and first contour point */
    /* as a third and fourth argument in wrap-around segments */
    if (first > last)
    {
      for (n = 0; n < outline.n_contours; n++)
      {
        FT_UInt end = (FT_UInt)outline.contours[n];


        if (first <= end)
        {
          *(arg--) = end;
          if (end > 0xFF)
            need_words = 1;

          if (n == 0)
            *(arg--) = 0;
          else
            *(arg--) = (FT_UInt)outline.contours[n - 1] + 1;
          break;
        }
      }
    }

    if (last > 0xFF)
      need_words = 1;
  }

  /* emit the second part of wrap-around segments as separate segments */
  /* so that edges can easily link to them */
  for (seg = segments; seg < seg_limit; seg++)
  {
    FT_UInt first = seg->first - points;
    FT_UInt last = seg->last - points;


    if (first > last)
    {
      for (n = 0; n < outline.n_contours; n++)
      {
        if (first <= (FT_UInt)outline.contours[n])
        {
          if (n == 0)
            *(arg--) = 0;
          else
            *(arg--) = (FT_UInt)outline.contours[n - 1] + 1;
          break;
        }
      }

      *(arg--) = last;
    }
  }
  /* with most fonts it is very rare */
  /* that any of the pushed arguments is larger than 0xFF, */
  /* thus we refrain from further optimizing this case */

  arg = args;

  if (need_words)
  {
    for (i = 0; i < num_args; i += 255)
    {
      nargs = (num_args - i > 255) ? 255 : num_args - i;

      BCI(NPUSHW);
      BCI(nargs);
      for (j = 0; j < nargs; j++)
      {
        BCI(HIGH(*arg));
        BCI(LOW(*arg));
        arg++;
      }
    }
  }
  else
  {
    for (i = 0; i < num_args; i += 255)
    {
      nargs = (num_args - i > 255) ? 255 : num_args - i;

      BCI(NPUSHB);
      BCI(nargs);
      for (j = 0; j < nargs; j++)
      {
        BCI(*arg);
        arg++;
      }
    }
  }

  BCI(CALL);

  num_storage = sal_segment_offset + num_segments * 2;
  if (num_storage > sfnt->max_storage)
    sfnt->max_storage = num_storage;

  num_twilight_points = num_segments * 2;
  if (num_twilight_points > sfnt->max_twilight_points)
    sfnt->max_twilight_points = num_twilight_points;

  num_stack_elements = ADDITIONAL_STACK_ELEMENTS + num_args;
  if (num_stack_elements > sfnt->max_stack_elements)
    sfnt->max_stack_elements = num_stack_elements;

  free(args);

  return bufp;
}


static FT_Bool
TA_hints_record_is_different(Hints_Record* hints_records,
                             FT_UInt num_hints_records,
                             FT_Byte* start,
                             FT_Byte* end)
{
  Hints_Record last_hints_record;


  if (!hints_records)
    return 1;

  /* we only need to compare with the last hints record */
  last_hints_record = hints_records[num_hints_records - 1];

  if ((FT_UInt)(end - start) != last_hints_record.buf_len)
    return 1;

  if (memcmp(start, last_hints_record.buf, last_hints_record.buf_len))
    return 1;

  return 0;
}


static FT_Error
TA_add_hints_record(Hints_Record** hints_records,
                    FT_UInt* num_hints_records,
                    FT_Byte* start,
                    Hints_Record hints_record)
{
  Hints_Record* hints_records_new;
  FT_UInt buf_len;
  /* at this point, `hints_record.buf' still points into `ins_buf' */
  FT_Byte* end = hints_record.buf;


  buf_len = (FT_UInt)(end - start);

  /* now fill the structure completely */
  hints_record.buf_len = buf_len;
  hints_record.buf = (FT_Byte*)malloc(buf_len);
  if (!hints_record.buf)
    return FT_Err_Out_Of_Memory;

  memcpy(hints_record.buf, start, buf_len);

  (*num_hints_records)++;
  hints_records_new =
    (Hints_Record*)realloc(*hints_records, *num_hints_records
                                           * sizeof (Hints_Record));
  if (!hints_records_new)
  {
    free(hints_record.buf);
    (*num_hints_records)--;
    return FT_Err_Out_Of_Memory;
  }
  else
    *hints_records = hints_records_new;

  (*hints_records)[*num_hints_records - 1] = hints_record;

  return FT_Err_Ok;
}


static FT_Byte*
TA_sfnt_emit_hints_record(SFNT* sfnt,
                          Hints_Record* hints_record,
                          FT_Byte* bufp)
{
  FT_Byte* p;
  FT_Byte* endp;
  FT_Bool need_words = 0;

  FT_UInt i, j;
  FT_UInt num_arguments;
  FT_UInt num_args;
  FT_UInt num_stack_elements;


  /* check whether any argument is larger than 0xFF */
  endp = hints_record->buf + hints_record->buf_len;
  for (p = hints_record->buf; p < endp; p += 2)
    if (*p)
      need_words = 1;

  /* with most fonts it is very rare */
  /* that any of the pushed arguments is larger than 0xFF, */
  /* thus we refrain from further optimizing this case */

  num_arguments = hints_record->buf_len / 2;
  p = endp - 2;

  if (need_words)
  {
    for (i = 0; i < num_arguments; i += 255)
    {
      num_args = (num_arguments - i > 255) ? 255 : (num_arguments - i);

      BCI(NPUSHW);
      BCI(num_args);
      for (j = 0; j < num_args; j++)
      {
        BCI(*p);
        BCI(*(p + 1));
        p -= 2;
      }
    }
  }
  else
  {
    /* we only need the lower bytes */
    p++;

    for (i = 0; i < num_arguments; i += 255)
    {
      num_args = (num_arguments - i > 255) ? 255 : (num_arguments - i);

      BCI(NPUSHB);
      BCI(num_args);
      for (j = 0; j < num_args; j++)
      {
        BCI(*p);
        p -= 2;
      }
    }
  }

  num_stack_elements = ADDITIONAL_STACK_ELEMENTS + num_arguments;
  if (num_stack_elements > sfnt->max_stack_elements)
    sfnt->max_stack_elements = sfnt->max_stack_elements;

  return bufp;
}


static FT_Byte*
TA_sfnt_emit_hints_records(SFNT* sfnt,
                           Hints_Record* hints_records,
                           FT_UInt num_hints_records,
                           FT_Byte* bufp)
{
  FT_UInt i;
  Hints_Record* hints_record;


  hints_record = hints_records;

  for (i = 0; i < num_hints_records - 1; i++)
  {
    BCI(MPPEM);
    if (hints_record->size > 0xFF)
    {
      BCI(PUSHW_1);
      BCI(HIGH((hints_record + 1)->size));
      BCI(LOW((hints_record + 1)->size));
    }
    else
    {
      BCI(PUSHB_1);
      BCI((hints_record + 1)->size);
    }
    BCI(LT);
    BCI(IF);
    bufp = TA_sfnt_emit_hints_record(sfnt, hints_record, bufp);
    BCI(ELSE);

    hints_record++;
  }

  bufp = TA_sfnt_emit_hints_record(sfnt, hints_record, bufp);

  for (i = 0; i < num_hints_records - 1; i++)
    BCI(EIF);

  BCI(PUSHB_1);
  BCI(bci_hint_glyph);
  BCI(CALL);

  return bufp;
}


static void
TA_free_hints_records(Hints_Record* hints_records,
                      FT_UInt num_hints_records)
{
  FT_UInt i;


  for (i = 0; i < num_hints_records; i++)
    free(hints_records[i].buf);

  free(hints_records);
}


static FT_Byte*
TA_hints_recorder_handle_segments(FT_Byte* bufp,
                                  TA_AxisHints axis,
                                  TA_Edge edge,
                                  FT_UInt* wraps)
{
  TA_Segment segments = axis->segments;
  TA_Segment seg;
  FT_UInt seg_idx;
  FT_UInt num_segs = 0;
  FT_UInt* wrap;


  seg_idx = edge->first - segments;

  /* we store everything as 16bit numbers */
  *(bufp++) = HIGH(seg_idx);
  *(bufp++) = LOW(seg_idx);

  /* wrap-around segments are stored as two segments */
  if (edge->first->first > edge->first->last)
    num_segs++;

  seg = edge->first->edge_next;
  while (seg != edge->first)
  {
    num_segs++;

    if (seg->first > seg->last)
      num_segs++;

    seg = seg->edge_next;
  }

  *(bufp++) = HIGH(num_segs);
  *(bufp++) = LOW(num_segs);

  if (edge->first->first > edge->first->last)
  {
    /* emit second part of wrap-around segment; */
    /* the bytecode positions such segments after `normal' ones */
    wrap = wraps;
    for (;;)
    {
      if (seg_idx == *wrap)
        break;
      wrap++;
    }

    *(bufp++) = HIGH(axis->num_segments + (wrap - wraps));
    *(bufp++) = LOW(axis->num_segments + (wrap - wraps));
  }

  seg = edge->first->edge_next;
  while (seg != edge->first)
  {
    seg_idx = seg - segments;

    *(bufp++) = HIGH(seg_idx);
    *(bufp++) = LOW(seg_idx);

    if (seg->first > seg->last)
    {
      wrap = wraps;
      for (;;)
      {
        if (seg_idx == *wrap)
          break;
        wrap++;
      }

      *(bufp++) = HIGH(axis->num_segments + (wrap - wraps));
      *(bufp++) = LOW(axis->num_segments + (wrap - wraps));
    }

    seg = seg->edge_next;
  }

  return bufp;
}


static void
TA_hints_recorder(TA_Action action,
                  TA_GlyphHints hints,
                  TA_Dimension dim,
                  void* arg1,
                  void* arg2,
                  void* arg3)
{
  TA_AxisHints axis = &hints->axis[dim];
  TA_Segment segments = axis->segments;

  Recorder* recorder = (Recorder*)hints->user;
  FONT* font = recorder->font;
  FT_UInt* wraps = recorder->wrap_around_segments;
  FT_Byte* p = recorder->hints_record.buf;


  if (dim == TA_DIMENSION_HORZ)
    return;

  /* we ignore the BOUND action since the information is handled */
  /* in `ta_adjust_bound' and `ta_stem_bound' */
  if (action == ta_bound)
    return;

  *(p++) = 0;
  *(p++) = (FT_Byte)action + ACTION_OFFSET;

  switch (action)
  {
  case ta_adjust_bound:
    {
      TA_Edge edge = (TA_Edge)arg1;
      TA_Edge edge2 = (TA_Edge)arg2;
      TA_Edge edge_minus_one = (TA_Edge)arg3;


      *(p++) = 0;
      *(p++) = edge2->flags & TA_EDGE_SERIF;
      *(p++) = 0;
      *(p++) = edge->flags & TA_EDGE_ROUND;
      *(p++) = HIGH(edge->first - segments);
      *(p++) = LOW(edge->first - segments);
      *(p++) = HIGH(edge2->first - segments);
      *(p++) = LOW(edge2->first - segments);
      *(p++) = HIGH(edge_minus_one->first - segments);
      *(p++) = LOW(edge_minus_one->first - segments);

      p = TA_hints_recorder_handle_segments(p, axis, edge, wraps);
    }
    break;

  case ta_stem_bound:
    {
      TA_Edge edge = (TA_Edge)arg1;
      TA_Edge edge2 = (TA_Edge)arg2;
      TA_Edge edge_minus_one = (TA_Edge)arg3;


      *(p++) = 0;
      *(p++) = edge2->flags & TA_EDGE_SERIF;
      *(p++) = 0;
      *(p++) = edge->flags & TA_EDGE_ROUND;
      *(p++) = HIGH(edge->first - segments);
      *(p++) = LOW(edge->first - segments);
      *(p++) = HIGH(edge2->first - segments);
      *(p++) = LOW(edge2->first - segments);
      *(p++) = HIGH(edge_minus_one->first - segments);
      *(p++) = LOW(edge_minus_one->first - segments);

      p = TA_hints_recorder_handle_segments(p, axis, edge, wraps);
      p = TA_hints_recorder_handle_segments(p, axis, edge2, wraps);
    }
    break;

  case ta_link:
    {
      TA_Edge base_edge = (TA_Edge)arg1;
      TA_Edge stem_edge = (TA_Edge)arg2;


      *(p++) = 0;
      *(p++) = stem_edge->flags & TA_EDGE_SERIF;
      *(p++) = 0;
      *(p++) = base_edge->flags & TA_EDGE_ROUND;
      *(p++) = HIGH(base_edge->first - segments);
      *(p++) = LOW(base_edge->first - segments);
      *(p++) = HIGH(stem_edge->first - segments);
      *(p++) = LOW(stem_edge->first - segments);

      p = TA_hints_recorder_handle_segments(p, axis, stem_edge, wraps);
    }
    break;

  case ta_anchor:
  case ta_adjust:
    {
      TA_Edge edge = (TA_Edge)arg1;
      TA_Edge edge2 = (TA_Edge)arg2;


      *(p++) = 0;
      *(p++) = edge2->flags & TA_EDGE_SERIF;
      *(p++) = 0;
      *(p++) = edge->flags & TA_EDGE_ROUND;
      *(p++) = HIGH(edge->first - segments);
      *(p++) = LOW(edge->first - segments);
      *(p++) = HIGH(edge2->first - segments);
      *(p++) = LOW(edge2->first - segments);

      p = TA_hints_recorder_handle_segments(p, axis, edge, wraps);
    }
    break;

  case ta_blue_anchor:
    {
      TA_Edge edge = (TA_Edge)arg1;
      TA_Edge blue = (TA_Edge)arg2;


      *(p++) = HIGH(blue->first - segments);
      *(p++) = LOW(blue->first - segments);

      if (edge->best_blue_is_shoot)
      {
        *(p++) = HIGH(CVT_BLUE_SHOOTS_OFFSET(font) + edge->best_blue_idx);
        *(p++) = LOW(CVT_BLUE_SHOOTS_OFFSET(font) + edge->best_blue_idx);
      }
      else
      {
        *(p++) = HIGH(CVT_BLUE_REFS_OFFSET(font) + edge->best_blue_idx);
        *(p++) = LOW(CVT_BLUE_REFS_OFFSET(font) + edge->best_blue_idx);
      }

      *(p++) = HIGH(edge->first - segments);
      *(p++) = LOW(edge->first - segments);

      p = TA_hints_recorder_handle_segments(p, axis, edge, wraps);
    }
    break;

  case ta_stem:
    {
      TA_Edge edge = (TA_Edge)arg1;
      TA_Edge edge2 = (TA_Edge)arg2;


      *(p++) = 0;
      *(p++) = edge2->flags & TA_EDGE_SERIF;
      *(p++) = 0;
      *(p++) = edge->flags & TA_EDGE_ROUND;
      *(p++) = HIGH(edge->first - segments);
      *(p++) = LOW(edge->first - segments);
      *(p++) = HIGH(edge2->first - segments);
      *(p++) = LOW(edge2->first - segments);

      p = TA_hints_recorder_handle_segments(p, axis, edge, wraps);
      p = TA_hints_recorder_handle_segments(p, axis, edge2, wraps);
    }
    break;

  case ta_blue:
    {
      TA_Edge edge = (TA_Edge)arg1;


      if (edge->best_blue_is_shoot)
      {
        *(p++) = HIGH(CVT_BLUE_SHOOTS_OFFSET(font) + edge->best_blue_idx);
        *(p++) = LOW(CVT_BLUE_SHOOTS_OFFSET(font) + edge->best_blue_idx);
      }
      else
      {
        *(p++) = HIGH(CVT_BLUE_REFS_OFFSET(font) + edge->best_blue_idx);
        *(p++) = LOW(CVT_BLUE_REFS_OFFSET(font) + edge->best_blue_idx);
      }

      *(p++) = HIGH(edge->first - segments);
      *(p++) = LOW(edge->first - segments);

      p = TA_hints_recorder_handle_segments(p, axis, edge, wraps);
    }
    break;

  case ta_serif:
    p = TA_hints_recorder_handle_segments(p, axis, (TA_Edge)arg1, wraps);
    break;

  case ta_serif_anchor:
    p = TA_hints_recorder_handle_segments(p, axis, (TA_Edge)arg1, wraps);
    break;

  case ta_serif_link1:
    p = TA_hints_recorder_handle_segments(p, axis, (TA_Edge)arg1, wraps);
    break;

  case ta_serif_link2:
    p = TA_hints_recorder_handle_segments(p, axis, (TA_Edge)arg1, wraps);
    break;

  /* to pacify the compiler */
  case ta_bound:
    break;
  }

  recorder->hints_record.num_actions++;
  recorder->hints_record.buf = p;
}


static FT_Error
TA_sfnt_build_glyph_instructions(SFNT* sfnt,
                                 FONT* font,
                                 FT_Long idx)
{
  FT_Face face = sfnt->face;
  FT_Error error;

  FT_Byte* ins_buf;
  FT_UInt ins_len;
  FT_Byte* bufp;

  SFNT_Table* glyf_table = &font->tables[sfnt->glyf_idx];
  glyf_Data* data = (glyf_Data*)glyf_table->data;
  GLYPH* glyph = &data->glyphs[idx];

  TA_GlyphHints hints;

  FT_UInt num_hints_records;
  Hints_Record* hints_records;

  Recorder recorder;

  FT_UInt size;


  if (idx < 0)
    return FT_Err_Invalid_Argument;

  /* computing the segments is resolution independent, */
  /* thus the pixel size in this call is arbitrary */
  error = FT_Set_Pixel_Sizes(face, 20, 20);
  if (error)
    return error;

  ta_loader_register_hints_recorder(font->loader, NULL, NULL);
  error = ta_loader_load_glyph(font->loader, face, (FT_UInt)idx, 0);
  if (error)
    return error;

  /* do nothing if we have an empty glyph */
  if (!face->glyph->outline.n_contours)
    return FT_Err_Ok;

  /* do nothing if the dummy hinter has been used */
  if (font->loader->metrics->clazz == &ta_dummy_script_class)
    return FT_Err_Ok;

  hints = &font->loader->hints;

  /* we allocate a buffer which is certainly large enough */
  /* to hold all of the created bytecode instructions; */
  /* later on it gets reallocated to its real size */
  ins_len = hints->num_points * 1000;
  ins_buf = (FT_Byte*)malloc(ins_len);
  if (!ins_buf)
    return FT_Err_Out_Of_Memory;

  /* initialize array with an invalid bytecode */
  /* so that we can easily find the array length at reallocation time */
  memset(ins_buf, INS_A0, ins_len);

  recorder.font = font;
  recorder.wrap_around_segments =
    (FT_UInt*)malloc(face->glyph->outline.n_contours * sizeof (FT_UInt));

  bufp = TA_sfnt_build_glyph_segments(sfnt, &recorder, ins_buf);

  /* now we loop over a large range of pixel sizes */
  /* to find hints records which get pushed onto the bytecode stack */
  num_hints_records = 0;
  hints_records = NULL;

#ifdef DEBUGGING
  printf("glyph %ld\n", idx);
#endif

  /* we temporarily use `ins_buf' to record the current glyph hints, */
  /* leaving two bytes at the beginning so that the number of actions */
  /* can be inserted later on */
  ta_loader_register_hints_recorder(font->loader,
                                    TA_hints_recorder,
                                    (void *)&recorder);

  for (size = 8; size <= 1000; size++)
  {
    /* rewind buffer pointer for recorder */
    recorder.hints_record.buf = bufp + 2;
    recorder.hints_record.num_actions = 0;
    recorder.hints_record.size = size;

    error = FT_Set_Pixel_Sizes(face, size, size);
    if (error)
      goto Err;

    /* calling `ta_loader_load_glyph' uses the */
    /* `TA_hints_recorder' function as a callback, */
    /* modifying `hints_record' */
    error = ta_loader_load_glyph(font->loader, face, idx, 0);
    if (error)
      goto Err;

    /* store the number of actions in `ins_buf' */
    *bufp = HIGH(recorder.hints_record.num_actions);
    *(bufp + 1) = LOW(recorder.hints_record.num_actions);

    if (TA_hints_record_is_different(hints_records,
                                     num_hints_records,
                                     bufp, recorder.hints_record.buf))
    {
#ifdef DEBUGGING
      {
        FT_Byte* p;


        printf("  %d:\n", size);
        for (p = bufp; p < recorder.hints_record.buf; p += 2)
          printf(" %2d", *p * 256 + *(p + 1));
        printf("\n");
      }
#endif

      error = TA_add_hints_record(&hints_records,
                                  &num_hints_records,
                                  bufp, recorder.hints_record);
      if (error)
        goto Err;
    }
  }

  if (num_hints_records == 1 && !hints_records[0].num_actions)
  {
    /* don't emit anything if we only have a single empty record */
    ins_len = 0;
  }
  else
  {
    FT_Byte* p = bufp;


    /* otherwise, clear the temporarily used part of `ins_buf' */
    while (*p != INS_A0)
      *(p++) = INS_A0;

    bufp = TA_sfnt_emit_hints_records(sfnt,
                                      hints_records, num_hints_records,
                                      bufp);

    /* we are done, so reallocate the instruction array to its real size */
    if (*bufp == INS_A0)
    {
      /* search backwards */
      while (*bufp == INS_A0)
        bufp--;
      bufp++;
    }
    else
    {
      /* search forwards */
      while (*bufp != INS_A0)
        bufp++;
    }

    ins_len = bufp - ins_buf;
  }

  if (ins_len > sfnt->max_instructions)
    sfnt->max_instructions = ins_len;

  glyph->ins_buf = (FT_Byte*)realloc(ins_buf, ins_len);
  glyph->ins_len = ins_len;

  TA_free_hints_records(hints_records, num_hints_records);
  free(recorder.wrap_around_segments);

  return FT_Err_Ok;

Err:
  TA_free_hints_records(hints_records, num_hints_records);
  free(recorder.wrap_around_segments);
  free(ins_buf);

  return error;
}


FT_Error
TA_sfnt_build_glyf_hints(SFNT* sfnt,
                         FONT* font)
{
  FT_Face face = sfnt->face;
  FT_Long idx;
  FT_Error error;


  for (idx = 0; idx < face->num_glyphs; idx++)
  {
    error = TA_sfnt_build_glyph_instructions(sfnt, font, idx);
    if (error)
      return error;
  }

  return FT_Err_Ok;
}

/* end of tabytecode.c */
