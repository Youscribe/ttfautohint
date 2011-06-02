/* tabytecode.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#include "ta.h"
#include "tabytecode.h"


/* a simple macro to emit bytecode instructions */
#define BCI(code) *(bufp++) = (code)

/* we increase the stack depth by amount */
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

#define sal_counter 0
#define sal_limit sal_counter + 1
#define sal_scale sal_limit + 1
#define sal_0x10000 sal_scale + 1
#define sal_is_extra_light sal_0x10000 + 1
#define sal_pos sal_is_extra_light + 1
#define sal_segment_offset sal_pos + 1 /* must be last */


/* we need the following macro */
/* so that `func_name' doesn't get replaced with its #defined value */
/* (as defined in `tabytecode.h') */

#define FPGM(func_name) fpgm_ ## func_name


/* in the comments below, the top of the stack (`s:') */
/* is the rightmost element; the stack is shown */
/* after the instruction on the same line has been executed */

/* point 0 in the twilight zone (zp0) is originally located */
/* at the origin; we don't change that */

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
      PUSHB_1,
        56,
      MIN, /* dist = min(56, dist) */
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
      PUSHB_2,
        48,

};

/*      %c, index of std_width */

unsigned char FPGM(bci_compute_stem_width_c) [] = {

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
 *   elements of the range.  The called function must not change the
 *   stack.
 *
 * in: func_num
 *     end
 *     start
 *
 * uses: sal_counter (counter initialized with `start')
 *       sal_limit (`end')
 */

unsigned char FPGM(bci_loop) [] = {

  PUSHB_1,
    bci_loop,
  FDEF,

  ROLL,
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
 * bci_cvt_rescale
 *
 *   Rescale CVT value by a given factor.
 *
 * uses: sal_counter (CVT index)
 *       sal_scale (scale in 16.16 format)
 */

unsigned char FPGM(bci_cvt_rescale) [] = {

  PUSHB_1,
    bci_cvt_rescale,
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

  WCVTP,

  ENDF,

};


/*
 * bci_loop_sal_assign
 *
 *   Apply the WS instruction repeatedly to stack data.
 *
 * in: counter (N)
 *     offset
 *     data_0
 *     data_1
 *     ...
 *     data_(N-1)
 *
 * uses: bci_sal_assign
 */

unsigned char FPGM(bci_sal_assign) [] = {

  PUSHB_1,
    bci_sal_assign,
  FDEF,

  DUP,
  ROLL, /* s: offset offset data */
  WS,

  PUSHB_1,
    1,
  ADD, /* s: (offset + 1) */

  ENDF,

};

unsigned char FPGM(bci_loop_sal_assign) [] = {

  PUSHB_1,
    bci_loop_sal_assign,
  FDEF,

  /* process the stack, popping off the elements in a loop */
  PUSHB_1,
    bci_sal_assign,
  LOOPCALL,

  /* clean up stack */
  POP,

  ENDF,

};


/*
 * bci_blue_round
 *
 *   Round a blue ref value and adjust its corresponding shoot value.
 *
 * uses: sal_counter (CVT index)
 *
 */

unsigned char FPGM(bci_blue_round_a) [] = {

  PUSHB_1,
    bci_blue_round,
  FDEF,

  PUSHB_1,
    sal_counter,
  RS,
  DUP,
  RCVT, /* s: ref_idx ref */

  DUP,
  PUSHB_1,
    32,
  ADD,
  FLOOR,
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
 *   Align all points in a segment to the value in `sal_pos'.
 *
 * in: segment_index
 *
 * sal: sal_pos
 */

unsigned char FPGM(bci_align_segment) [] = {

  PUSHB_1,
    bci_align_segment,
  FDEF,

  PUSHB_6,
    1,
    1,
    sal_pos,
    0,
    0,
    0,
  SZP0, /* set zp0 to twilight zone 0 */
  SZP1, /* set zp1 to twilight zone 0 */

  /* we can't directly set rp0 to a stack value */
  MDAP_noround, /* reset rp0 (and rp1) to the origin in the twilight zone */
  RS,
  MSIRP_rp0, /* set point 1 and rp0 in the twilight zone to `sal_pos' */

  SZP1, /* set zp1 to normal zone 1 */

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
 *   Align segments to the value in `sal_pos'.
 *
 * in: first_segment
 *     loop_counter (N)
 *       segment_1
 *       segment_2
 *       ...
 *       segment_N
 *
 * sal: sal_pos
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


unsigned char FPGM(bci_action_adjust_bound) [] = {

  PUSHB_1,
    bci_action_adjust_bound,
  FDEF,

  PUSHB_1,
    bci_handle_segments,
  CALL,
  PUSHB_1,
    bci_handle_segments,
  CALL,
  PUSHB_1,
    bci_handle_segments,
  CALL,

  /* XXX */

  ENDF,

};

unsigned char FPGM(bci_action_stem_bound) [] = {

  PUSHB_1,
    bci_action_stem_bound,
  FDEF,

  PUSHB_1,
    bci_handle_segments,
  CALL,
  PUSHB_1,
    bci_handle_segments,
  CALL,
  PUSHB_1,
    bci_handle_segments,
  CALL,

  /* XXX */

  ENDF,

};


/*
 * bci_action_link
 *
 *   Handle the LINK action to link an edge to another one.
 *
 * in: base_point
 *     stem_point
 *     stem_is_serif
 *     base_is_round
 *     ... stuff for bci_align_segments ...
 *
 * sal: sal_pos
 *
 * XXX: Instead of `base_point', use the median of the first segment in the
 *      base edge.
 */

unsigned char FPGM(bci_action_link) [] = {

  PUSHB_1,
    bci_action_link,
  FDEF,

  PUSHB_5,
    1,
    sal_pos,
    3,
    0,
    1,
  SZP0, /* set zp0 to normal zone 1 */
  SZP1, /* set zp1 to twilight zone 0 */
  CINDEX, /* s: ... stem_point base_point 1 sal_pos base_point */

  /* get distance between base_point and twilight point 0 (at origin) */
  PUSHB_1,
    0,
  MD_cur, /* s: ... stem_point base_point 1 sal_pos base_point_y_pos */
  WS, /* sal_pos: base_point_y_pos */

  SZP1, /* set zp1 to normal zone 1 */

  MD_orig, /* s: base_is_round stem_is_serif dist */

  PUSHB_1,
    bci_compute_stem_width,
  CALL,  /* s: new_dist */

  PUSHB_1,
    sal_pos,
  RS,
  ADD,
  PUSHB_1,
    sal_pos,
  SWAP,
  WS, /* sal_pos: base_point_y_pos + new_dist */

  PUSHB_1,
    bci_align_segments,
  CALL,

  ENDF,

};

unsigned char FPGM(bci_action_anchor) [] = {

  PUSHB_1,
    bci_action_anchor,
  FDEF,

  PUSHB_1,
    bci_handle_segments,
  CALL,
  PUSHB_1,
    bci_handle_segments,
  CALL,

  /* XXX */

  ENDF,

};

unsigned char FPGM(bci_action_adjust) [] = {

  PUSHB_1,
    bci_action_adjust,
  FDEF,

  PUSHB_1,
    bci_handle_segments,
  CALL,
  PUSHB_1,
    bci_handle_segments,
  CALL,

  /* XXX */

  ENDF,

};

unsigned char FPGM(bci_action_stem) [] = {

  PUSHB_1,
    bci_action_stem,
  FDEF,

  PUSHB_1,
    bci_handle_segments,
  CALL,
  PUSHB_1,
    bci_handle_segments,
  CALL,

  /* XXX */

  ENDF,

};


/*
 * bci_action_blue
 *
 *   Handle the BLUE action to align an edge with a blue zone.
 *
 * in: blue_cvt_idx
 *     ... stuff for bci_align_segments ...
 */

unsigned char FPGM(bci_action_blue) [] = {

  PUSHB_1,
    bci_action_blue,
  FDEF,

  /* store blue position in `sal_pos' */
  RCVT,
  PUSHB_1,
    sal_pos,
  SWAP,
  WS,

  PUSHB_1,
    bci_align_segments,
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


  buf_len = sizeof (FPGM(bci_compute_stem_width_a))
            + 1
            + sizeof (FPGM(bci_compute_stem_width_b))
            + 1
            + sizeof (FPGM(bci_compute_stem_width_c))
            + sizeof (FPGM(bci_loop))
            + sizeof (FPGM(bci_cvt_rescale))
            + sizeof (FPGM(bci_sal_assign))
            + sizeof (FPGM(bci_loop_sal_assign))
            + sizeof (FPGM(bci_blue_round_a))
            + 1
            + sizeof (FPGM(bci_blue_round_b))
            + sizeof (FPGM(bci_handle_segment))
            + sizeof (FPGM(bci_align_segment))
            + sizeof (FPGM(bci_handle_segments))
            + sizeof (FPGM(bci_align_segments))
            + sizeof (FPGM(bci_action_adjust_bound))
            + sizeof (FPGM(bci_action_stem_bound))
            + sizeof (FPGM(bci_action_link))
            + sizeof (FPGM(bci_action_anchor))
            + sizeof (FPGM(bci_action_adjust))
            + sizeof (FPGM(bci_action_stem))
            + sizeof (FPGM(bci_action_blue))
            + sizeof (FPGM(bci_action_serif))
            + sizeof (FPGM(bci_action_anchor))
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

  COPY_FPGM(bci_compute_stem_width_a);
  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
  COPY_FPGM(bci_compute_stem_width_b);
  *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
  COPY_FPGM(bci_compute_stem_width_c);
  COPY_FPGM(bci_loop);
  COPY_FPGM(bci_cvt_rescale);
  COPY_FPGM(bci_sal_assign);
  COPY_FPGM(bci_loop_sal_assign);
  COPY_FPGM(bci_blue_round_a);
  *(buf_p++) = (unsigned char)CVT_BLUES_SIZE(font);
  COPY_FPGM(bci_blue_round_b);
  COPY_FPGM(bci_handle_segment);
  COPY_FPGM(bci_align_segment);
  COPY_FPGM(bci_handle_segments);
  COPY_FPGM(bci_align_segments);
  COPY_FPGM(bci_action_adjust_bound);
  COPY_FPGM(bci_action_stem_bound);
  COPY_FPGM(bci_action_link);
  COPY_FPGM(bci_action_anchor);
  COPY_FPGM(bci_action_adjust);
  COPY_FPGM(bci_action_stem);
  COPY_FPGM(bci_action_blue);
  COPY_FPGM(bci_action_serif);
  COPY_FPGM(bci_action_anchor);
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
                             FONT* font,
                             FT_Byte* bufp)
{
  TA_GlyphHints hints = &font->loader->hints;
  TA_AxisHints axis = &hints->axis[TA_DIMENSION_VERT];
  TA_Point points = hints->points;
  TA_Segment segments = axis->segments;
  TA_Segment seg;
  TA_Segment seg_limit;

  FT_UInt* args;
  FT_UInt* arg;
  FT_UInt num_args;
  FT_UInt nargs;

  FT_Bool need_words = 0;

  FT_UInt i, j;
  FT_UInt num_storage;
  FT_UInt num_stack_elements;


  seg_limit = segments + axis->num_segments;
  num_args = 2 * axis->num_segments + 3;

  /* collect all arguments temporarily in an array (in reverse order) */
  /* so that we can easily split into chunks of 255 args */
  /* as needed by NPUSHB and NPUSHW, respectively */
  args = (FT_UInt*)malloc(num_args * sizeof (FT_UInt));
  if (!args)
    return NULL;

  arg = args + num_args - 1;

  if (axis->num_segments > 0xFF)
    need_words = 1;

  *(arg--) = bci_loop_sal_assign;
  *(arg--) = axis->num_segments * 2;
  *(arg--) = sal_segment_offset;

  for (seg = segments; seg < seg_limit; seg++)
  {
    FT_UInt first = seg->first - points;
    FT_UInt last = seg->last - points;


    *(arg--) = first;
    *(arg--) = last;

    if (first > 0xFF || last > 0xFF)
      need_words = 1;
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

  num_storage = sal_segment_offset + axis->num_segments * 2;
  if (num_storage > sfnt->max_storage)
    sfnt->max_storage = num_storage;

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

  /* this instruction is essential for getting correct CVT values */
  /* if horizontal and vertical resolutions differ; */
  /* it assures that the projection vector is set to the y axis */
  /* so that CVT values are handled as being `vertical' */
  BCI(SVTCA_y);

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
                                  TA_Segment segments,
                                  TA_Edge edge)
{
  TA_Segment seg;
  FT_UInt seg_idx;
  FT_UInt num_segs = 0;


  seg_idx = edge->first - segments;

  /* we store everything as 16bit numbers */
  *(bufp++) = HIGH(seg_idx);
  *(bufp++) = LOW(seg_idx);

  seg = edge->first->edge_next;
  while (seg != edge->first)
  {
    seg = seg->edge_next;
    num_segs++;
  }

  *(bufp++) = HIGH(num_segs);
  *(bufp++) = LOW(num_segs);

  seg = edge->first->edge_next;
  while (seg != edge->first)
  {
    seg_idx = seg - segments;
    seg = seg->edge_next;

    *(bufp++) = HIGH(seg_idx);
    *(bufp++) = LOW(seg_idx);
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
  TA_Point points = hints->points;
  TA_Segment segments = axis->segments;

  Recorder* recorder = (Recorder*)hints->user;
  FONT* font = recorder->font;
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
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg1);
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg2);
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg3);
    break;

  case ta_stem_bound:
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg1);
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg2);
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg3);
    break;

  case ta_link:
    {
      TA_Edge base_edge = (TA_Edge)arg1;
      TA_Edge stem_edge = (TA_Edge)arg2;


      *(p++) = HIGH(base_edge->first->first - points);
      *(p++) = LOW(base_edge->first->first - points);
      *(p++) = HIGH(stem_edge->first->first - points);
      *(p++) = LOW(stem_edge->first->first - points);
      *(p++) = 0;
      *(p++) = stem_edge->flags & TA_EDGE_SERIF;
      *(p++) = 0;
      *(p++) = base_edge->flags & TA_EDGE_ROUND;

      p = TA_hints_recorder_handle_segments(p, segments, stem_edge);
    }
    break;

  case ta_anchor:
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg1);
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg2);
    break;

  case ta_adjust:
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg1);
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg2);
    break;

  case ta_stem:
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg1);
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg2);
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

      p = TA_hints_recorder_handle_segments(p, segments, edge);
    }
    break;

  case ta_serif:
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg1);
    break;

  case ta_serif_anchor:
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg1);
    break;

  case ta_serif_link1:
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg1);
    break;

  case ta_serif_link2:
    p = TA_hints_recorder_handle_segments(p, segments, (TA_Edge)arg1);
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

  bufp = TA_sfnt_build_glyph_segments(sfnt, font, ins_buf);

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
  recorder.font = font;
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

  /* clear `ins_buf' if we only have a single empty record */
  if (num_hints_records == 1 && !hints_records[0].num_actions)
    memset(ins_buf, INS_A0, (bufp + 2) - ins_buf);
  else
    bufp = TA_sfnt_emit_hints_records(sfnt,
                                      hints_records, num_hints_records,
                                      bufp);

  /* we are done, so reallocate the instruction array to its real size */
  /* (memrchr is a GNU glibc extension, so we do it manually) */
  bufp = ins_buf + ins_len;
  while (*(--bufp) == INS_A0)
    ;
  ins_len = bufp - ins_buf + 1;

  if (ins_len > sfnt->max_instructions)
    sfnt->max_instructions = ins_len;

  glyph->ins_buf = (FT_Byte*)realloc(ins_buf, ins_len);
  glyph->ins_len = ins_len;

  TA_free_hints_records(hints_records, num_hints_records);

  return FT_Err_Ok;

Err:
  TA_free_hints_records(hints_records, num_hints_records);
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
