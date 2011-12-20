/* tagpos.c */

/*
 * Copyright (C) 2011 by Werner Lemberg.
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


/* the code below contains many redundancies; */
/* it has been written for clarity */

#define VALUE(val, p) val = *(p++) << 8; \
                      val += *(p++)
#define OFFSET(val, base, p) val = base; \
                             val += *(p++) << 8; \
                             val += *(p++)


/* We add a subglyph for each composite glyph. */
/* Since subglyphs must contain at least one point, */
/* we have to adjust all AnchorPoints in GPOS AnchorTables accordingly. */
/* Using the `endpoints' array of the `GLYPH' structure, */
/* it is straightforward to do that: */
/* Assuming that the anchor point x is in the interval */
/* endpoints[n] < x <= endpoints[n + 1], */
/* the new point index is x + n. */

static FT_Error
TA_update_anchor(FT_Byte* p,
                 FT_Byte* cov,
                 SFNT* sfnt,
                 FONT* font)
{
  SFNT_Table* GPOS_table = &font->tables[sfnt->GPOS_idx];

  FT_UShort AnchorFormat;


  VALUE(AnchorFormat, p);

  if (AnchorFormat == 2)
  {
    FT_UShort AnchorPoint;


    p += 4; /* skip XCoordinate and YCoordinate */
    VALUE(AnchorPoint, p);

    if (p > GPOS_table->buf + GPOS_table->len)
      return FT_Err_Invalid_Table;

    /* XXX */
#if 0
    *(p - 2) = HIGH(AnchorPoint + offset);
    *(p - 1) = LOW(AnchorPoint + offset);
#endif
  }

  return TA_Err_Ok;
}


static FT_Error
TA_handle_cursive_lookup(FT_Byte* Lookup,
                         FT_Byte* p,
                         SFNT* sfnt,
                         FONT* font)
{
  FT_UShort SubTableCount;


  p += 2; /* skip LookupFlag */
  VALUE(SubTableCount, p);

  /* loop over p */
  for (; SubTableCount > 0; SubTableCount--)
  {
    FT_Byte* CursivePosFormat1;
    FT_Byte* Coverage;
    FT_UShort EntryExitCount;
    FT_Byte* q;


    OFFSET(CursivePosFormat1, Lookup, p);

    q = CursivePosFormat1;
    q += 2; /* skip PosFormat */
    OFFSET(Coverage, CursivePosFormat1, q);
    VALUE(EntryExitCount, q);

    /* loop over q */
    for (; EntryExitCount > 0; EntryExitCount--)
    {
      FT_Byte* EntryAnchor;
      FT_Byte* ExitAnchor;
      FT_Error error;


      OFFSET(EntryAnchor, CursivePosFormat1, q);
      error = TA_update_anchor(EntryAnchor, Coverage, sfnt, font);
      if (error)
        return error;

      OFFSET(ExitAnchor, CursivePosFormat1, q);
      error = TA_update_anchor(ExitAnchor, Coverage, sfnt, font);
      if (error)
        return error;
    }
  }

  return TA_Err_Ok;
}


static FT_Error
TA_handle_markbase_lookup(FT_Byte* Lookup,
                          FT_Byte* p,
                          SFNT* sfnt,
                          FONT* font)
{
  FT_UShort SubTableCount;


  p += 2; /* skip LookupFlag */
  VALUE(SubTableCount, p);

  /* loop over p */
  for (; SubTableCount > 0; SubTableCount--)
  {
    FT_Byte* MarkBasePosFormat1;
    FT_Byte* MarkCoverage;
    FT_Byte* BaseCoverage;
    FT_UShort ClassCount;
    FT_UShort MarkCount;
    FT_Byte* MarkArray;
    FT_UShort BaseCount;
    FT_Byte* BaseArray;
    FT_Byte* q;


    OFFSET(MarkBasePosFormat1, Lookup, p);

    q = MarkBasePosFormat1;
    q += 2; /* skip PosFormat */
    OFFSET(MarkCoverage, MarkBasePosFormat1, q);
    OFFSET(BaseCoverage, MarkBasePosFormat1, q);
    VALUE(ClassCount, q);
    OFFSET(MarkArray, MarkBasePosFormat1, q);
    OFFSET(BaseArray, MarkBasePosFormat1, q);

    q = MarkArray;
    VALUE(MarkCount, q);

    /* loop over q */
    for (; MarkCount > 0; MarkCount--)
    {
      FT_Byte* MarkAnchor;
      FT_Error error;


      q += 2; /* skip Class */
      OFFSET(MarkAnchor, MarkArray, q);
      error = TA_update_anchor(MarkAnchor, MarkCoverage, sfnt, font);
      if (error)
        return error;
    }

    q = BaseArray;
    VALUE(BaseCount, q);

    /* loop over q */
    for (; BaseCount > 0; BaseCount--)
    {
      FT_UShort cc = ClassCount;


      for (; cc > 0; cc--)
      {
        FT_Byte* BaseAnchor;
        FT_Error error;


        OFFSET(BaseAnchor, BaseArray, q);
        error = TA_update_anchor(BaseAnchor, BaseCoverage, sfnt, font);
        if (error)
          return error;
      }
    }
  }

  return TA_Err_Ok;
}


static FT_Error
TA_handle_marklig_lookup(FT_Byte* Lookup,
                         FT_Byte* p,
                         SFNT* sfnt,
                         FONT* font)
{
  FT_UShort SubTableCount;


  p += 2; /* skip LookupFlag */
  VALUE(SubTableCount, p);

  /* loop over p */
  for (; SubTableCount > 0; SubTableCount--)
  {
    FT_Byte* MarkLigPosFormat1;
    FT_Byte* MarkCoverage;
    FT_Byte* LigatureCoverage;
    FT_UShort ClassCount;
    FT_UShort MarkCount;
    FT_Byte* MarkArray;
    FT_UShort LigatureCount;
    FT_Byte* LigatureArray;
    FT_Byte* q;


    OFFSET(MarkLigPosFormat1, Lookup, p);

    q = MarkLigPosFormat1;
    q += 2; /* skip PosFormat */
    OFFSET(MarkCoverage, MarkLigPosFormat1, q);
    OFFSET(LigatureCoverage, MarkLigPosFormat1, q);
    VALUE(ClassCount, q);
    OFFSET(MarkArray, MarkLigPosFormat1, q);
    OFFSET(LigatureArray, MarkLigPosFormat1, q);

    q = MarkArray;
    VALUE(MarkCount, q);

    /* loop over q */
    for (; MarkCount > 0; MarkCount--)
    {
      FT_Byte* MarkAnchor;
      FT_Error error;


      q += 2; /* skip Class */
      OFFSET(MarkAnchor, MarkArray, q);
      error = TA_update_anchor(MarkAnchor, MarkCoverage, sfnt, font);
      if (error)
        return error;
    }

    q = LigatureArray;
    VALUE(LigatureCount, q);

    /* loop over q */
    for (; LigatureCount > 0; LigatureCount--)
    {
      FT_Byte* LigatureAttach;
      FT_UShort ComponentCount;
      FT_Byte* r;


      OFFSET(LigatureAttach, LigatureArray, q);

      r = LigatureAttach;
      VALUE(ComponentCount, r);

      /* loop over r */
      for (; ComponentCount > 0; ComponentCount--)
      {
        FT_UShort cc = ClassCount;


        for (; cc > 0; cc--)
        {
          FT_Byte* LigatureAnchor;
          FT_Error error;


          OFFSET(LigatureAnchor, LigatureAttach, r);
          error = TA_update_anchor(LigatureAnchor, LigatureCoverage,
                                   sfnt, font);
          if (error)
            return error;
        }
      }
    }
  }

  return TA_Err_Ok;
}


static FT_Error
TA_handle_markmark_lookup(FT_Byte* Lookup,
                          FT_Byte* p,
                          SFNT* sfnt,
                          FONT* font)
{
  FT_UShort SubTableCount;


  p += 2; /* skip LookupFlag */
  VALUE(SubTableCount, p);

  /* loop over p */
  for (; SubTableCount > 0; SubTableCount--)
  {
    FT_Byte* MarkMarkPosFormat1;
    FT_Byte* Mark1Coverage;
    FT_Byte* Mark2Coverage;
    FT_UShort ClassCount;
    FT_UShort Mark1Count;
    FT_Byte* Mark1Array;
    FT_UShort Mark2Count;
    FT_Byte* Mark2Array;
    FT_Byte* q;


    OFFSET(MarkMarkPosFormat1, Lookup, p);

    q = MarkMarkPosFormat1;
    q += 2; /* skip PosFormat */
    OFFSET(Mark1Coverage, MarkMarkPosFormat1, q);
    OFFSET(Mark2Coverage, MarkMarkPosFormat1, q);
    VALUE(ClassCount, q);
    OFFSET(Mark1Array, MarkMarkPosFormat1, q);
    OFFSET(Mark2Array, MarkMarkPosFormat1, q);

    q = Mark1Array;
    VALUE(Mark1Count, q);

    /* loop over q */
    for (; Mark1Count > 0; Mark1Count--)
    {
      FT_Byte* Mark1Anchor;
      FT_Error error;


      q += 2; /* skip Class */
      OFFSET(Mark1Anchor, Mark1Array, q);
      error = TA_update_anchor(Mark1Anchor, Mark1Coverage, sfnt, font);
      if (error)
        return error;
    }

    q = Mark2Array;
    VALUE(Mark2Count, q);

    /* loop over q */
    for (; Mark2Count > 0; Mark2Count--)
    {
      FT_UShort cc = ClassCount;


      for (; cc > 0; cc--)
      {
        FT_Byte* Mark2Anchor;
        FT_Error error;


        OFFSET(Mark2Anchor, Mark2Array, q);
        error = TA_update_anchor(Mark2Anchor, Mark2Coverage, sfnt, font);
        if (error)
          return error;
      }
    }
  }

  return TA_Err_Ok;
}


#define Cursive 3
#define MarkBase 4
#define MarkLig 5
#define MarkMark 6

FT_Error
TA_sfnt_update_GPOS_table(SFNT* sfnt,
                          FONT* font)
{
  SFNT_Table* GPOS_table = &font->tables[sfnt->GPOS_idx];
  FT_Byte* buf = GPOS_table->buf;

  FT_Byte* LookupList;
  FT_UShort LookupCount;
  FT_Byte* p;


  p = buf;

  if (GPOS_table->processed)
    return TA_Err_Ok;

  p += 8; /* skip Version, ScriptList, and FeatureList */
  OFFSET(LookupList, buf, p);

  p = LookupList;
  VALUE(LookupCount, p);

  /* loop over p */
  for (; LookupCount > 0; LookupCount--)
  {
    FT_Byte* Lookup;
    FT_UShort LookupType;
    FT_Byte *q;
    FT_Error error;


    OFFSET(Lookup, LookupList, p);

    q = Lookup;
    VALUE(LookupType, q);

    if (LookupType == Cursive)
      error = TA_handle_cursive_lookup(Lookup, q, sfnt, font);
    else if (LookupType == MarkBase)
      error = TA_handle_markbase_lookup(Lookup, q, sfnt, font);
    else if (LookupType == MarkLig)
      error = TA_handle_marklig_lookup(Lookup, q, sfnt, font);
    else if (LookupType == MarkMark)
      error = TA_handle_markmark_lookup(Lookup, q, sfnt, font);

    if (error)
      return error;
  }

  return TA_Err_Ok;
}

/* end of tagpos.c */
