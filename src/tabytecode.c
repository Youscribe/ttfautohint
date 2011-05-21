/* tabytecode.c */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#include "ta.h"
#include "tabytecode.h"


/* a simple macro to emit bytecode instructions */
#define BCI(code) *(bufp++) = (code)


#ifdef TA_DEBUG
int _ta_debug = 1;
int _ta_debug_disable_horz_hints;
int _ta_debug_disable_vert_hints;
int _ta_debug_disable_blue_hints;
void* _ta_debug_hints;
#endif


/* structures for hinting sets */
typedef struct Edge2Blue_ {
  FT_UInt first_segment;

  FT_Bool is_serif;
  FT_Bool is_round;

  FT_UInt num_remaining_segments;
  FT_UInt* remaining_segments;
} Edge2Blue;

typedef struct Edge2Link_ {
  FT_UInt first_segment;

  FT_UInt num_remaining_segments;
  FT_UInt* remaining_segments;
} Edge2Link;

typedef struct Hinting_Set_ {
  FT_UInt size;

  FT_UInt num_edges2blues;
  Edge2Blue* edges2blues;

  FT_UInt num_edges2links;
  Edge2Link* edges2links;

  FT_Bool need_words;
  FT_UInt num_args;
} Hinting_Set;


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
#define sal_limit sal_counter + 1
#define sal_scale sal_limit + 1
#define sal_0x10000 sal_scale + 1
#define sal_segment_offset sal_0x10000 + 1 /* must be last */


/* we need the following macro */
/* so that `func_name' doesn't get replaced with its #defined value */
/* (as defined in `tabytecode.h') */

#define FPGM(func_name) fpgm_ ## func_name


/* in the comments below, the top of the stack (`s:') */
/* is the rightmost element; the stack is shown */
/* after the instruction on the same line has been executed */

/*
 * bci_compute_stem_width
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
 * in: width
 *     stem_is_serif
 *     base_is_round
 * out: new_width
 * CVT: is_extra_light   XXX
 *      std_width
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
      NEG, /* dist = -dist */

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
 * bci_rescale
 *
 *   All entries in the CVT table get scaled automatically using the
 *   vertical resolution.  However, some widths must be scaled with the
 *   horizontal resolution, and others get adjusted later on.
 *
 * uses: sal_counter (CVT index)
 *       sal_scale (scale in 16.16 format)
 */

unsigned char FPGM(bci_rescale) [] = {

  PUSHB_1,
    bci_rescale,
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
 * bci_hint_glyph
 *
 *   This is the top-level glyph hinting function
 *   which parses the arguments on the stack and calls subroutines.
 *
 * in: num_edges2blues (M)
 *       edge2blue_0.first_segment
 *                  .is_serif
 *                  .is_round
 *                  .num_remaining_segments (N)
 *                  .remaining_segments_0
 *                                     _1
 *                                     ...
 *                                     _(N-1)
 *                _1
 *                ...
 *                _(M-1)
 *
 *     num_edges2links (P)
 *       edge2link_0.first_segment
 *                  .num_remaining_segments (Q)
 *                  .remaining_segments_0
 *                                     _1
 *                                     ...
 *                                     _(Q-1)
 *                _1
 *                ...
 *                _(P-1)
 *
 * uses: bci_edge2blue
 *       bci_edge2link
 */

unsigned char FPGM(bci_remaining_edges) [] = {

  PUSHB_1,
    bci_remaining_edges,
  FDEF,

  POP, /* XXX remaining segment */

  ENDF,

};

unsigned char FPGM(bci_edge2blue) [] = {

  PUSHB_1,
    bci_edge2blue,
  FDEF,

  POP, /* XXX first_segment */
  POP, /* XXX is_serif */
  POP, /* XXX is_round */
  PUSHB_1,
    bci_remaining_edges,
  LOOPCALL,

  ENDF,

};

unsigned char FPGM(bci_edge2link) [] = {

  PUSHB_1,
    bci_edge2link,
  FDEF,

  POP, /* XXX first_segment */
  PUSHB_1,
    bci_remaining_edges,
  LOOPCALL,

  ENDF,

};

unsigned char FPGM(bci_hint_glyph) [] = {

  PUSHB_1,
    bci_hint_glyph,
  FDEF,

  PUSHB_1,
    bci_edge2blue,
  LOOPCALL,

  PUSHB_1,
    bci_edge2link,
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
            + sizeof (FPGM(bci_rescale))
            + sizeof (FPGM(bci_sal_assign))
            + sizeof (FPGM(bci_loop_sal_assign))
            + sizeof (FPGM(bci_remaining_edges))
            + sizeof (FPGM(bci_edge2blue))
            + sizeof (FPGM(bci_edge2link))
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
  COPY_FPGM(bci_rescale);
  COPY_FPGM(bci_sal_assign);
  COPY_FPGM(bci_loop_sal_assign);
  COPY_FPGM(bci_remaining_edges);
  COPY_FPGM(bci_edge2blue);
  COPY_FPGM(bci_edge2link);
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

/* we often need 0x10000 which can't be pushed directly onto the stack, */
/* thus we provide it in the storage area */

unsigned char prep_A[] = {

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

      bci_rescale,
      bci_loop,
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

    /* loop over vertical CVT entries */
    PUSHB_4,

};

/*    %c, first vertical index */
/*    %c, last vertical index */

unsigned char prep_e[] = {

      bci_rescale,
      bci_loop,
    CALL,

    /* loop over blue refs */
    PUSHB_4,

};

/*    %c, first blue ref index */
/*    %c, last blue ref index */

unsigned char prep_f[] = {

      bci_rescale,
      bci_loop,
    CALL,

    /* loop over blue shoots */
    PUSHB_4,

};

/*    %c, first blue shoot index */
/*    %c, last blue shoot index */

unsigned char prep_g[] = {

      bci_rescale,
      bci_loop,
    CALL,

  EIF,

};

/* XXX talatin.c: 577 */
/* XXX talatin.c: 1671 */
/* XXX talatin.c: 1708 */


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

  buf_len = sizeof (prep_A)
            + sizeof (prep_a)
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

  memcpy(buf_p, prep_A, sizeof (prep_A));
  buf_p += sizeof (prep_A);

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

    *(buf_p++) = blue_adjustment - vaxis->blues
                 + CVT_BLUE_SHOOTS_OFFSET(font);

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

  /* XXX handle extra_light */

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
TA_font_build_glyph_segments(FONT* font,
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

  free(args);

  return bufp;
}


static void
TA_font_clear_edge_DONE_flag(FONT* font)
{
  TA_GlyphHints hints = &font->loader->hints;
  TA_AxisHints axis = &hints->axis[TA_DIMENSION_VERT];
  TA_Edge edges = axis->edges;
  TA_Edge edge_limit = edges + axis->num_edges;
  TA_Edge edge;


  for (edge = edges; edge < edge_limit; edge++)
    edge->flags &= ~TA_EDGE_DONE;
}


static FT_Error
TA_construct_hinting_set(FONT* font,
                         FT_UInt size,
                         Hinting_Set* hinting_set)
{
  TA_GlyphHints hints = &font->loader->hints;
  TA_AxisHints axis = &hints->axis[TA_DIMENSION_VERT];
  TA_Segment segments = axis->segments;
  TA_Edge edges = axis->edges;
  TA_Edge limit = edges + axis->num_edges;
  TA_Edge edge;

  Edge2Blue* edge2blue;
  Edge2Link* edge2link;


  hinting_set->size = size;
  hinting_set->need_words = 0;
  hinting_set->num_args = 0;

  hinting_set->num_edges2blues = 0;
  hinting_set->edges2blues = NULL;
  hinting_set->num_edges2links = 0;
  hinting_set->edges2links = NULL;

  for (edge = edges; edge < limit; edge++)
  {
    if (edge->blue_edge)
      hinting_set->num_edges2blues++;
    else
      hinting_set->num_edges2links++;
  }

  if (hinting_set->num_edges2blues > 0xFF
      || hinting_set->num_edges2links > 0xFF)
    hinting_set->need_words = 1;

  /* we push num_edges2blues and num_edges2links */
  hinting_set->num_args += 2;

  if (hinting_set->num_edges2blues)
  {
    hinting_set->edges2blues =
      (Edge2Blue*)calloc(1, hinting_set->num_edges2blues
                            * sizeof (Edge2Blue));
    if (!hinting_set->edges2blues)
      return FT_Err_Out_Of_Memory;
  }

  if (hinting_set->num_edges2links)
  {
    hinting_set->edges2links =
      (Edge2Link*)calloc(1, hinting_set->num_edges2links
                            * sizeof (Edge2Link));
    if (!hinting_set->edges2links)
      return FT_Err_Out_Of_Memory;
  }

  edge2blue = hinting_set->edges2blues;
  edge2link = hinting_set->edges2links;

  for (edge = edges; edge < limit; edge++)
  {
    TA_Segment seg;
    FT_UInt* remaining_segment;


    if (edge->blue_edge)
    {
      edge2blue->first_segment = edge->first - segments;
      edge2blue->is_serif = edge->flags & TA_EDGE_SERIF;
      edge2blue->is_round = edge->flags & TA_EDGE_ROUND;

      seg = edge->first->edge_next;
      while (seg != edge->first)
      {
        edge2blue->num_remaining_segments++;
        seg = seg->edge_next;
      }

      if (edge2blue->first_segment > 0xFF
          || edge2blue->num_remaining_segments > 0xFF)
        hinting_set->need_words = 1;

      if (edge2blue->num_remaining_segments)
      {
        edge2blue->remaining_segments =
          (FT_UInt*)calloc(1, edge2blue->num_remaining_segments
                              * sizeof (FT_UInt));
        if (!edge2blue->remaining_segments)
          return FT_Err_Out_Of_Memory;
      }

      seg = edge->first->edge_next;
      remaining_segment = edge2blue->remaining_segments;
      while (seg != edge->first)
      {
        *remaining_segment = seg - segments;
        seg = seg->edge_next;

        if (*remaining_segment > 0xFF)
          hinting_set->need_words = 1;

        remaining_segment++;
      }

      /* we push the number of remaining segments, is_serif, is_round, */
      /* the first segment, and the remaining segments */
      hinting_set->num_args += edge2blue->num_remaining_segments + 4;

      edge2blue++;
    }
    else
    {
      edge2link->first_segment = edge->first - segments;

      seg = edge->first->edge_next;
      while (seg != edge->first)
      {
        edge2link->num_remaining_segments++;
        seg = seg->edge_next;
      }

      if (edge2link->first_segment > 0xFF
          || edge2link->num_remaining_segments > 0xFF)
        hinting_set->need_words = 1;

      if (edge2link->num_remaining_segments)
      {
        edge2link->remaining_segments =
          (FT_UInt*)calloc(1, edge2link->num_remaining_segments
                              * sizeof (FT_UInt));
        if (!edge2link->remaining_segments)
          return FT_Err_Out_Of_Memory;
      }

      seg = edge->first->edge_next;
      remaining_segment = edge2link->remaining_segments;
      while (seg != edge->first)
      {
        *remaining_segment = seg - segments;
        seg = seg->edge_next;

        if (*remaining_segment > 0xFF)
          hinting_set->need_words = 1;

        remaining_segment++;
      }

      /* we push the number of remaining segments, */
      /* the first segment, and the remaining segments */
      hinting_set->num_args += edge2link->num_remaining_segments + 2;

      edge2link++;
    }
  }

  return FT_Err_Ok;
}


static FT_Bool
TA_hinting_set_is_different(Hinting_Set* hinting_sets,
                            FT_UInt num_hinting_sets,
                            Hinting_Set hinting_set)
{
  Hinting_Set last_hinting_set;

  Edge2Blue* edge2blue;
  Edge2Blue* last_edge2blue;
  Edge2Link* edge2link;
  Edge2Link* last_edge2link;

  FT_UInt i;


  if (!hinting_sets)
    return 1;

  /* we only need to compare with the last hinting set */
  last_hinting_set = hinting_sets[num_hinting_sets - 1];

  if (hinting_set.num_edges2blues
      != last_hinting_set.num_edges2blues)
    return 1;

  edge2blue = hinting_set.edges2blues;
  last_edge2blue = last_hinting_set.edges2blues;

  for (i = 0;
       i < hinting_set.num_edges2blues;
       i++, edge2blue++, last_edge2blue++)
  {
    if (edge2blue->num_remaining_segments
        != last_edge2blue->num_remaining_segments)
      return 1;

    if (edge2blue->remaining_segments)
    {
      if (memcmp(edge2blue->remaining_segments,
                 last_edge2blue->remaining_segments,
                 sizeof (FT_UInt) * edge2blue->num_remaining_segments))
        return 1;
    }
  }

  if (hinting_set.num_edges2links
      != last_hinting_set.num_edges2links)
    return 1;

  edge2link = hinting_set.edges2links;
  last_edge2link = last_hinting_set.edges2links;

  for (i = 0;
       i < hinting_set.num_edges2links;
       i++, edge2link++, last_edge2link++)
  {
    if (edge2link->num_remaining_segments
        != last_edge2link->num_remaining_segments)
      return 1;

    if (edge2link->remaining_segments)
    {
      if (memcmp(edge2link->remaining_segments,
                 last_edge2link->remaining_segments,
                 sizeof (FT_UInt) * edge2link->num_remaining_segments))
        return 1;
    }
  }

  return 0;
}


static FT_Error
TA_add_hinting_set(Hinting_Set** hinting_sets,
                   FT_UInt* num_hinting_sets,
                   Hinting_Set hinting_set)
{
  Hinting_Set* hinting_sets_new;


  (*num_hinting_sets)++;
  hinting_sets_new =
    (Hinting_Set*)realloc(*hinting_sets, *num_hinting_sets
                                         * sizeof (Hinting_Set));
  if (!hinting_sets_new)
  {
    (*num_hinting_sets)--;
    return FT_Err_Out_Of_Memory;
  }
  else
    *hinting_sets = hinting_sets_new;

  (*hinting_sets)[*num_hinting_sets - 1] = hinting_set;

  return FT_Err_Ok;
}


static FT_Byte*
TA_emit_hinting_set(Hinting_Set* hinting_set,
                    FT_Byte* bufp)
{
  FT_UInt* args;
  FT_UInt* arg;

  Edge2Blue* edge2blue;
  Edge2Blue* edge2blue_limit;
  Edge2Link* edge2link;
  Edge2Link* edge2link_limit;

  FT_UInt* seg;
  FT_UInt* seg_limit;

  FT_UInt i, j;
  FT_UInt num_args;


  /* collect all arguments temporarily in an array (in reverse order) */
  /* so that we can easily split into chunks of 255 args */
  /* as needed by NPUSHB and NPUSHW, respectively */
  args = (FT_UInt*)malloc(hinting_set->num_args * sizeof (FT_UInt));
  if (!args)
    return NULL;

  arg = args + hinting_set->num_args - 1;

  *(arg--) = hinting_set->num_edges2blues;

  edge2blue_limit = hinting_set->edges2blues
                    + hinting_set->num_edges2blues;
  for (edge2blue = hinting_set->edges2blues;
       edge2blue < edge2blue_limit;
       edge2blue++)
  {
    *(arg--) = edge2blue->first_segment;
    *(arg--) = edge2blue->is_serif;
    *(arg--) = edge2blue->is_round;
    *(arg--) = edge2blue->num_remaining_segments;

    seg_limit = edge2blue->remaining_segments
                + edge2blue->num_remaining_segments;
    for (seg = edge2blue->remaining_segments; seg < seg_limit; seg++)
      *(arg--) = *seg;
  }

  *(arg--) = hinting_set->num_edges2links;

  edge2link_limit = hinting_set->edges2links
                    + hinting_set->num_edges2links;
  for (edge2link = hinting_set->edges2links;
       edge2link < edge2link_limit;
       edge2link++)
  {
    *(arg--) = edge2link->first_segment;
    *(arg--) = edge2link->num_remaining_segments;

    seg_limit = edge2link->remaining_segments
                + edge2link->num_remaining_segments;
    for (seg = edge2link->remaining_segments; seg < seg_limit; seg++)
      *(arg--) = *seg;
  }

  /* with most fonts it is very rare */
  /* that any of the pushed arguments is larger than 0xFF, */
  /* thus we refrain from further optimizing this case */

  arg = args;

  if (hinting_set->need_words)
  {
    for (i = 0; i < hinting_set->num_args; i += 255)
    {
      num_args = (hinting_set->num_args - i > 255)
                   ? 255
                   : hinting_set->num_args - i;

      BCI(NPUSHW);
      BCI(num_args);
      for (j = 0; j < num_args; j++)
      {
        BCI(HIGH(*arg));
        BCI(LOW(*arg));
        arg++;
      }
    }
  }
  else
  {
    for (i = 0; i < hinting_set->num_args; i += 255)
    {
      num_args = (hinting_set->num_args - i > 255)
                   ? 255
                   : hinting_set->num_args - i;

      BCI(NPUSHB);
      BCI(num_args);
      for (j = 0; j < num_args; j++)
      {
        BCI(*arg);
        arg++;
      }
    }
  }

  free(args);

  return bufp;
}


static FT_Byte*
TA_emit_hinting_sets(Hinting_Set* hinting_sets,
                     FT_UInt num_hinting_sets,
                     FT_Byte* bufp)
{
  FT_UInt i;
  Hinting_Set* hinting_set;


  hinting_set = hinting_sets;

  BCI(SVTCA_y);

  for (i = 0; i < num_hinting_sets - 1; i++)
  {
    BCI(MPPEM);
    if (hinting_set->size > 0xFF)
    {
      BCI(PUSHW_1);
      BCI(HIGH((hinting_set + 1)->size));
      BCI(LOW((hinting_set + 1)->size));
    }
    else
    {
      BCI(PUSHB_1);
      BCI((hinting_set + 1)->size);
    }
    BCI(LT);
    BCI(IF);
    bufp = TA_emit_hinting_set(hinting_set, bufp);
    if (!bufp)
      return NULL;
    BCI(ELSE);

    hinting_set++;
  }

  bufp = TA_emit_hinting_set(hinting_set, bufp);
  if (!bufp)
    return NULL;

  for (i = 0; i < num_hinting_sets - 1; i++)
    BCI(EIF);

  BCI(PUSHB_1);
  BCI(bci_hint_glyph);
  BCI(CALL);

  return bufp;
}


static void
TA_free_hinting_set(Hinting_Set hinting_set)
{
  FT_UInt i;


  for (i = 0; i < hinting_set.num_edges2blues; i++)
    free(hinting_set.edges2blues[i].remaining_segments);
  free(hinting_set.edges2blues);

  for (i = 0; i < hinting_set.num_edges2links; i++)
    free(hinting_set.edges2links[i].remaining_segments);
  free(hinting_set.edges2links);
}


static void
TA_free_hinting_sets(Hinting_Set* hinting_sets,
                     FT_UInt num_hinting_sets)
{
  FT_UInt i;


  for (i = 0; i < num_hinting_sets; i++)
    TA_free_hinting_set(hinting_sets[i]);

  free(hinting_sets);
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

  FT_UInt num_hinting_sets;
  Hinting_Set* hinting_sets;

  FT_UInt size;


  if (idx < 0)
    return FT_Err_Invalid_Argument;

  /* computing the segments is resolution independent, */
  /* thus the pixel size in this call is arbitrary */
  error = FT_Set_Pixel_Sizes(face, 20, 20);
  if (error)
    return error;

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

  bufp = TA_font_build_glyph_segments(font, ins_buf);

  /* now we loop over a large range of pixel sizes */
  /* to find hinting sets which get pushed onto the bytecode stack */
  num_hinting_sets = 0;
  hinting_sets = NULL;

#if 0
  printf("glyph %ld\n", idx);
#endif

  for (size = 8; size <= 1000; size++)
  {
    Hinting_Set hinting_set;


    error = FT_Set_Pixel_Sizes(face, size, size);
    if (error)
      goto Err;

    error = ta_loader_load_glyph(font->loader, face, idx, 0);
    if (error)
      goto Err;

    TA_font_clear_edge_DONE_flag(font);

    error = TA_construct_hinting_set(font, size, &hinting_set);
    if (error)
      goto Err;

    if (TA_hinting_set_is_different(hinting_sets,
                                    num_hinting_sets,
                                    hinting_set))
    {
#if 0
      if (num_hinting_sets > 0)
        printf("  additional hinting set for size %d\n", size);
#endif

      error = TA_add_hinting_set(&hinting_sets,
                                 &num_hinting_sets,
                                 hinting_set);
      if (error)
      {
        TA_free_hinting_set(hinting_set);
        goto Err;
      }
    }
    else
      TA_free_hinting_set(hinting_set);
  }

  bufp = TA_emit_hinting_sets(hinting_sets, num_hinting_sets, bufp);
  if (!bufp)
    return FT_Err_Out_Of_Memory;

  /* we are done, so reallocate the instruction array to its real size */
  bufp = (FT_Byte*)memchr((char*)ins_buf, INS_A0, ins_len);
  ins_len = bufp - ins_buf;

  if (ins_len > sfnt->max_instructions)
    sfnt->max_instructions = ins_len;

  glyph->ins_buf = (FT_Byte*)realloc(ins_buf, ins_len);
  glyph->ins_len = ins_len;

  TA_free_hinting_sets(hinting_sets, num_hinting_sets);

  return FT_Err_Ok;

Err:
  TA_free_hinting_sets(hinting_sets, num_hinting_sets);
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

  /* XXX provide real values or better estimates */
  sfnt->max_storage = 1000;
  sfnt->max_stack_elements = 1000;

  return FT_Err_Ok;
}

/* end of tabytecode.c */
