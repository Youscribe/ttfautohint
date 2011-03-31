/* tasort.h */

/* originally part of file `aftypes.h' (2011-Mar-28) from FreeType */

/* heavily modified 2011 by Werner Lemberg <wl@gnu.org> */

#ifndef __TASORT_H__
#define __TASORT_H__

#include "tatypes.h"


void
ta_sort_pos(FT_UInt count,
            FT_Pos* table);

void
ta_sort_widths(FT_UInt count,
               TA_Width widths);

#endif /* __TASORT_H__ */

/* end of tasort.h */
