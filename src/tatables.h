/* tatables.h */

/* written 2011 by Werner Lemberg <wl@gnu.org> */

#ifndef __TATABLES_H__
#define __TATABLES_H__

#include "ta.h"


FT_Error
TA_sfnt_add_table_info(SFNT* sfnt);

FT_ULong
TA_table_compute_checksum(FT_Byte* buf,
                          FT_ULong len);

FT_Error
TA_font_add_table(FONT* font,
                  SFNT_Table_Info* table_info,
                  FT_ULong tag,
                  FT_ULong len,
                  FT_Byte* buf);

void
TA_sfnt_sort_table_info(SFNT* sfnt,
                        FONT* font);

#endif /* __TATABLES_H__ */

/* end of tatables.h */
