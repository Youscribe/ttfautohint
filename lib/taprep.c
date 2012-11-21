/* taprep.c */

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


#include "ta.h"


#define PREP(snippet_name) prep_ ## snippet_name


unsigned char PREP(hinting_limit_a) [] =
{

  /* first of all, check whether we do hinting at all */

  MPPEM,
  PUSHW_1,

};

/*  %d, hinting size limit */

unsigned char PREP(hinting_limit_b) [] =
{

  GT,
  IF,
    PUSHB_2,
      1, /* switch off hinting */
      1,
    INSTCTRL,
  EIF,

};

/* we often need 0x10000 which can't be pushed directly onto the stack, */
/* thus we provide it in the CVT as `cvtl_0x10000'; */
/* at the same time, we store it in CVT index `cvtl_funits_to_pixels' also */
/* as a scaled value to have a conversion factor from FUnits to pixels */

unsigned char PREP(store_0x10000) [] =
{

  PUSHW_2,
    0x08, /* 0x800 */
    0x00,
    0x08, /* 0x800 */
    0x00,
  MUL, /* 0x10000 */

  DUP,
  PUSHB_1,
    cvtl_0x10000,
  SWAP,
  WCVTP,

  DUP,
  PUSHB_1,
    cvtl_funits_to_pixels,
  SWAP,
  WCVTF, /* store value 1 in 16.16 format, scaled */

};

unsigned char PREP(align_top_a) [] =
{

  /* optimize the alignment of the top of small letters to the pixel grid */

  PUSHB_1,

};

/*  %c, index of alignment blue zone */

unsigned char PREP(align_top_b) [] =
{

  RCVT,
  DUP,
  DUP,

};

unsigned char PREP(align_top_c1a) [] =
{

  /* use this if option `increase_x_height' > 0 */
  /* apply much `stronger' rounding up of x height for */
  /* 6 <= PPEM <= increase_x_height */
  MPPEM,
  PUSHW_1,

};

/*  %d, x height increase limit */

unsigned char PREP(align_top_c1b) [] =
{

  LTEQ,
  MPPEM,
  PUSHB_1,
    6,
  GTEQ,
  AND,
  IF,
    PUSHB_1,
      52, /* threshold = 52 */

  ELSE,
    PUSHB_1,
      40, /* threshold = 40 */

  EIF,
  ADD,
  FLOOR, /* fitted = FLOOR(scaled + threshold) */

};

unsigned char PREP(align_top_c2) [] =
{

  PUSHB_1,
    40,
  ADD,
  FLOOR, /* fitted = FLOOR(scaled + 40) */

};

unsigned char PREP(align_top_d) [] =
{

  DUP, /* s: scaled scaled fitted fitted */
  ROLL,
  NEQ,
  IF, /* s: scaled fitted */
    PUSHB_1,
      2,
    CINDEX,
    SUB, /* s: scaled (fitted-scaled) */
    PUSHB_1,
      cvtl_0x10000,
    RCVT,
    MUL, /* (fitted-scaled) in 16.16 format */
    SWAP,
    DIV, /* ((fitted-scaled) / scaled) in 16.16 format */

    PUSHB_1,
      cvtl_scale,
    SWAP,
    WCVTP,

};

unsigned char PREP(loop_cvt_a) [] =
{

    /* loop over vertical CVT entries */
    PUSHB_4,

};

/*    %c, first vertical index */
/*    %c, last vertical index */

unsigned char PREP(loop_cvt_b) [] =
{

      bci_cvt_rescale,
      bci_loop,
    CALL,

    /* loop over blue refs */
    PUSHB_4,

};

/*    %c, first blue ref index */
/*    %c, last blue ref index */

unsigned char PREP(loop_cvt_c) [] =
{

      bci_cvt_rescale,
      bci_loop,
    CALL,

    /* loop over blue shoots */
    PUSHB_4,

};

/*    %c, first blue shoot index */
/*    %c, last blue shoot index */

unsigned char PREP(loop_cvt_d) [] =
{

      bci_cvt_rescale,
      bci_loop,
    CALL,
  EIF,

};

unsigned char PREP(compute_extra_light_a) [] =
{

  /* compute (vertical) `extra_light' flag */
  PUSHB_3,
    cvtl_is_extra_light,
    40,

};

/*  %c, index of vertical standard_width */

unsigned char PREP(compute_extra_light_b) [] =
{

  RCVT,
  GT, /* standard_width < 40 */
  WCVTP,

};

unsigned char PREP(round_blues_a) [] =
{

  /* use discrete values for blue zone widths */
  PUSHB_4,

};

/*  %c, first blue ref index */
/*  %c, last blue ref index */

unsigned char PREP(round_blues_b) [] =
{

    bci_blue_round,
    bci_loop,
  CALL

};

unsigned char PREP(set_stem_width_handling_a) [] =
{

  /*
   * There are two ClearType flavours available on Windows: The older GDI
   * ClearType, introduced in 2000, and the recent DW ClearType, introduced
   * in 2008.  The main difference is that the older incarnation behaves
   * like a B/W renderer along the y axis, while the newer version does
   * vertical smoothing also.
   *
   * The only possibility to differentiate between GDI and DW ClearType is
   * testing bit 10 in the GETINFO instruction (with return value in bit 17;
   * this works for TrueType version >= 38), checking whether sub-pixel
   * positioning is available.
   *
   * If GDI ClearType is active, we use a different stem width function
   * which snaps to integer pixels as much as possible.
   */

  /* set default positioning */
  PUSHB_2,
    cvtl_stem_width_function,

};

/*  %d, either bci_smooth_stem_width or bci_strong_stem_width */

unsigned char PREP(set_stem_width_handling_b) [] =
{

  WCVTP,

  /* get rasterizer version (bit 0) */
  PUSHB_2,
    36,
    0x01,
  GETINFO,

  /* `GDI ClearType': */
  /* version >= 36 and version < 38, ClearType enabled */
  LTEQ,
  IF,
    /* check whether ClearType is enabled (bit 6) */
    PUSHB_1,
      0x40,
    GETINFO,
    IF,
      PUSHB_2,
        cvtl_stem_width_function,
};

/*      %d, either bci_smooth_stem_width or bci_strong_stem_width */

unsigned char PREP(set_stem_width_handling_c) [] =
{

      WCVTP,

      /* get rasterizer version (bit 0) */
      PUSHB_2,
        38,
        0x01,
      GETINFO,

      /* `DW ClearType': */
      /* version >= 38, sub-pixel positioning is enabled */
      LTEQ,
      IF,
        /* check whether sub-pixel positioning is enabled (bit 10) */
        PUSHW_1,
          0x04,
          0x00,
        GETINFO,
        IF,
          PUSHB_2,
            cvtl_stem_width_function,

};

/*          %d, either bci_smooth_stem_width or bci_strong_stem_width */

unsigned char PREP(set_stem_width_handling_d) [] =
{

          WCVTP,
        EIF,
      EIF,
    EIF,
  EIF,

};

unsigned char PREP(set_dropout_mode) [] =
{

  PUSHW_1,
    0x01, /* 0x01FF, activate dropout handling unconditionally */
    0xFF,
  SCANCTRL,
  PUSHB_1,
    4, /* smart dropout include stubs */
  SCANTYPE,

};

unsigned char PREP(reset_component_counter) [] =
{

  /* In case an application tries to render `.ttfautohint' */
  /* (which it should never do), */
  /* hinting of all glyphs rendered afterwards is disabled */
  /* because the `cvtl_is_subglyph' counter gets incremented, */
  /* but there is no counterpart to decrement it. */
  /* Font inspection tools like the FreeType demo programs */
  /* are an exception to that rule, however, */
  /* since they can directly access a font by glyph indices. */
  /* The following guard alleviates the problem a bit: */
  /* Any change of the graphics state */
  /* (for example, rendering at a different size or with a different mode) */
  /* resets the counter to zero. */
  PUSHB_2,
    cvtl_is_subglyph,
    0,
  WCVTP,

};


/* this function allocates `buf', parsing `number_set' to create bytecode */
/* which eventually sets CVT index `cvtl_is_element' */
/* (in functions `bci_number_set_is_element' and */
/* `bci_number_set_is_element2') */

static FT_Byte*
TA_sfnt_build_number_set(SFNT* sfnt,
                         FT_Byte** buf,
                         number_range* number_set)
{
  FT_Byte* bufp = NULL;
  number_range* nr;

  FT_UInt num_singles2 = 0;
  FT_UInt* single2_args;
  FT_UInt* single2_arg;
  FT_UInt num_singles = 0;
  FT_UInt* single_args;
  FT_UInt* single_arg;

  FT_UInt num_ranges2 = 0;
  FT_UInt* range2_args;
  FT_UInt* range2_arg;
  FT_UInt num_ranges = 0;
  FT_UInt* range_args;
  FT_UInt* range_arg;

  FT_UInt have_single = 0;
  FT_UInt have_range = 0;

  FT_UShort num_stack_elements;


  /* build up four stacks to stay as compact as possible */
  nr = number_set;
  while (nr)
  {
    if (nr->start == nr->end)
    {
      if (nr->start < 256)
        num_singles++;
      else
        num_singles2++;
    }
    else
    {
      if (nr->start < 256 && nr->end < 256)
        num_ranges++;
      else
        num_ranges2++;
    }
    nr = nr->next;
  }

  /* collect all arguments temporarily in arrays (in reverse order) */
  /* so that we can easily split into chunks of 255 args */
  /* as needed by NPUSHB and friends; */
  /* for simplicity, always allocate an extra slot */
  single2_args = (FT_UInt*)malloc(num_singles2 * sizeof (FT_UInt) + 1);
  single_args = (FT_UInt*)malloc(num_singles * sizeof (FT_UInt) + 1);
  range2_args = (FT_UInt*)malloc(2 * num_ranges2 * sizeof (FT_UInt) + 1);
  range_args = (FT_UInt*)malloc(2 * num_ranges * sizeof (FT_UInt) + 1);
  if (!single2_args || !single_args
      || !range2_args || !range_args)
    goto Fail;

  /* check whether we need the extra slot for the argument to CALL */
  if (num_singles || num_singles2)
    have_single = 1;
  if (num_ranges || num_ranges2)
    have_range = 1;

  /* set function indices outside of argument loop (using the extra slot) */
  if (have_single)
    single_args[num_singles] = bci_number_set_is_element;
  if (have_range)
    range_args[num_singles] = bci_number_set_is_element2;

  single2_arg = single2_args + num_singles2 - 1;
  single_arg = single_args + num_singles - 1;
  range2_arg = range2_args + num_ranges2 - 1;
  range_arg = range_args + num_ranges - 1;

  nr = number_set;
  while (nr)
  {
    if (nr->start == nr->end)
    {
      if (nr->start < 256)
        *(single_arg--) = nr->start;
      else
        *(single2_arg--) = nr->start;
    }
    else
    {
      if (nr->start < 256 && nr->end < 256)
      {
        *(range_arg--) = nr->start;
        *(range_arg--) = nr->end;
      }
      else
      {
        *(range2_arg--) = nr->start;
        *(range2_arg--) = nr->end;
      }
    }
    nr = nr->next;
  }

  /* this rough estimate of the buffer size gets adjusted later on */
  *buf = (FT_Byte*)malloc((2 + 1) * num_singles2
                          + (1 + 1) * num_singles
                          + (4 + 1) * num_ranges2
                          + (2 + 1) * num_ranges
                          + 2);
  if (!*buf)
    goto Fail;
  bufp = *buf;

  bufp = TA_build_push(bufp, single2_args, num_singles2, 1, 1);
  bufp = TA_build_push(bufp, single_args, num_singles + have_single, 0, 1);
  if (have_single)
    BCI(CALL);

  bufp = TA_build_push(bufp, range2_args, num_ranges2, 1, 1);
  bufp = TA_build_push(bufp, range_args, num_ranges + have_range, 0, 1);
  if (have_range)
    BCI(CALL);

  num_stack_elements = num_singles + num_singles2;
  if (num_stack_elements > num_ranges + num_ranges2)
    num_stack_elements = num_ranges + num_ranges2;
  num_stack_elements += ADDITIONAL_STACK_ELEMENTS;
  if (num_stack_elements > sfnt->max_stack_elements)
    sfnt->max_stack_elements = num_stack_elements;

Fail:
  free(single2_args);
  free(single_args);
  free(range2_args);
  free(range_args);

  return bufp;
}


#define COPY_PREP(snippet_name) \
          do \
          { \
            memcpy(buf_p, prep_ ## snippet_name, \
                   sizeof (prep_ ## snippet_name)); \
            buf_p += sizeof (prep_ ## snippet_name); \
          } while (0)

static FT_Error
TA_table_build_prep(FT_Byte** prep,
                    FT_ULong* prep_len,
                    FONT* font)
{
  TA_LatinAxis vaxis;
  TA_LatinBlue blue_adjustment = NULL;
  FT_UInt i;

  FT_UInt buf_len = 0;
  FT_UInt len;
  FT_Byte* buf;
  FT_Byte* buf_p;


  if (font->loader->hints.metrics->clazz->script == TA_SCRIPT_NONE)
    vaxis = NULL;
  else
  {
    vaxis = &((TA_LatinMetrics)font->loader->hints.metrics)->axis[1];

    for (i = 0; i < vaxis->blue_count; i++)
    {
      if (vaxis->blues[i].flags & TA_LATIN_BLUE_ADJUSTMENT)
      {
        blue_adjustment = &vaxis->blues[i];
        break;
      }
    }
  }

  if (font->hinting_limit)
    buf_len += sizeof (PREP(hinting_limit_a))
               + 2
               + sizeof (PREP(hinting_limit_b));

  buf_len += sizeof (PREP(store_0x10000));

  if (blue_adjustment)
    buf_len += sizeof (PREP(align_top_a))
               + 1
               + sizeof (PREP(align_top_b))
               + (font->increase_x_height ? (sizeof (PREP(align_top_c1a))
                                             + 2
                                             + sizeof (PREP(align_top_c1b)))
                                          : sizeof (PREP(align_top_c2)))
               + sizeof (PREP(align_top_d))
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

  buf_len += sizeof (PREP(set_stem_width_handling_a))
             + 1
             + sizeof (PREP(set_stem_width_handling_b))
             + 1
             + sizeof (PREP(set_stem_width_handling_c))
             + 1
             + sizeof (PREP(set_stem_width_handling_d));
  buf_len += sizeof (PREP(set_dropout_mode));
  buf_len += sizeof (PREP(reset_component_counter));

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

  if (font->hinting_limit)
  {
    COPY_PREP(hinting_limit_a);
    *(buf_p++) = HIGH(font->hinting_limit);
    *(buf_p++) = LOW(font->hinting_limit);
    COPY_PREP(hinting_limit_b);
  }

  COPY_PREP(store_0x10000);

  if (blue_adjustment)
  {
    COPY_PREP(align_top_a);
    *(buf_p++) = (unsigned char)(CVT_BLUE_SHOOTS_OFFSET(font)
                                 + blue_adjustment - vaxis->blues);
    COPY_PREP(align_top_b);
    if (font->increase_x_height)
    {
      COPY_PREP(align_top_c1a);
      *(buf_p++) = HIGH(font->increase_x_height);
      *(buf_p++) = LOW(font->increase_x_height);
      COPY_PREP(align_top_c1b);
    }
    else
      COPY_PREP(align_top_c2);
    COPY_PREP(align_top_d);

    COPY_PREP(loop_cvt_a);
    *(buf_p++) = (unsigned char)CVT_VERT_WIDTHS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_VERT_WIDTHS_OFFSET(font)
                                 + CVT_VERT_WIDTHS_SIZE(font) - 1);
    /* don't loop over the artificial blue zones */
    COPY_PREP(loop_cvt_b);
    *(buf_p++) = (unsigned char)CVT_BLUE_REFS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_REFS_OFFSET(font)
                                 + CVT_BLUES_SIZE(font) - 1 - 2);
    COPY_PREP(loop_cvt_c);
    *(buf_p++) = (unsigned char)CVT_BLUE_SHOOTS_OFFSET(font);
    *(buf_p++) = (unsigned char)(CVT_BLUE_SHOOTS_OFFSET(font)
                                 + CVT_BLUES_SIZE(font) - 1 - 2);
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

  COPY_PREP(set_stem_width_handling_a);
  *(buf_p++) = font->gray_strong_stem_width ? bci_strong_stem_width
                                            : bci_smooth_stem_width;
  COPY_PREP(set_stem_width_handling_b);
  *(buf_p++) = font->gdi_cleartype_strong_stem_width ? bci_strong_stem_width
                                                     : bci_smooth_stem_width;
  COPY_PREP(set_stem_width_handling_c);
  *(buf_p++) = font->dw_cleartype_strong_stem_width ? bci_strong_stem_width
                                                    : bci_smooth_stem_width;
  COPY_PREP(set_stem_width_handling_d);
  COPY_PREP(set_dropout_mode);
  COPY_PREP(reset_component_counter);

  *prep = buf;
  *prep_len = buf_len;

  return FT_Err_Ok;
}


FT_Error
TA_sfnt_build_prep_table(SFNT* sfnt,
                         FONT* font)
{
  FT_Error error = FT_Err_Ok;

  SFNT_Table* glyf_table = &font->tables[sfnt->glyf_idx];
  glyf_Data* data = (glyf_Data*)glyf_table->data;

  FT_Byte* prep_buf;
  FT_ULong prep_len;


  error = TA_sfnt_add_table_info(sfnt);
  if (error)
    goto Exit;

  /* `glyf', `cvt', `fpgm', and `prep' are always used in parallel */
  if (glyf_table->processed)
  {
    sfnt->table_infos[sfnt->num_table_infos - 1] = data->prep_idx;
    goto Exit;
  }

  error = TA_table_build_prep(&prep_buf, &prep_len, font);
  if (error)
    goto Exit;

#if 0
  /* ttfautohint's bytecode in `fpgm' is larger */
  /* than the bytecode in `prep'; */
  /* this commented out code here is just for completeness */
  if (prep_len > sfnt->max_instructions)
    sfnt->max_instructions = prep_len;
#endif

  /* in case of success, `prep_buf' gets linked */
  /* and is eventually freed in `TA_font_unload' */
  error = TA_font_add_table(font,
                            &sfnt->table_infos[sfnt->num_table_infos - 1],
                            TTAG_prep, prep_len, prep_buf);
  if (error)
    free(prep_buf);
  else
    data->prep_idx = sfnt->table_infos[sfnt->num_table_infos - 1];

Exit:
  return error;
}

/* end of taprep.c */
