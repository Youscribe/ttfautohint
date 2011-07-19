# configure.ac

# Copyright (C) 2011 by Werner Lemberg.
#
# This file is part of the ttfautohint library, and may only be used,
# modified, and distributed under the terms given in `COPYING'.  By
# continuing to use, modify, or distribute this file you indicate that you
# have read `COPYING' and understand and accept it fully.
#
# The file `COPYING' mentioned in the previous paragraph is distributed
# with the ttfautohint library.

AC_INIT([ttfautohint],
        [0.2],
        [freetype-devel@nongnu.org])
AM_INIT_AUTOMAKE([-Wall -Werror gnits])

AM_SILENT_RULES([yes])

AC_PROG_CC
AC_PROG_CPP

LT_INIT

AC_ARG_WITH(freetype-config,
            [  --with-freetype-config=PROG   Use FreeType configuration program PROG],
            freetype_config=$withval,
            freetype_config=yes)

if test "$freetype_config" = "yes"; then 
  AC_PATH_PROG(ft_config,
               freetype-config,
               no)
  if test "$ft_config" = "no"; then
    AC_MSG_ERROR([FreeType library is missing; see http://www.freetype.org/])
  fi
else
  ft_config="$freetype_config"
fi

FREETYPE_CPPFLAGS="`$ft_config --cflags`"
FREETYPE_LDFLAGS="`$ft_config --libtool`"

AC_SUBST(FREETYPE_CPPFLAGS)
AC_SUBST(FREETYPE_LDFLAGS)


AC_MSG_CHECKING([whether FreeType version is 2.4.5 or higher])
old_CPPFLAGS="$CPPFLAGS"
CPPFLAGS=$FREETYPE_CPPFLAGS
AC_TRY_CPP([

#include <ft2build.h>
#include FT_FREETYPE_H
#if (FREETYPE_MAJOR*1000 + FREETYPE_MINOR)*1000 + FREETYPE_PATCH < 2004005
#error Freetype version too low.
#endif

],
[AC_MSG_RESULT(yes)
 CPPFLAGS="$old_CPPFLAGS"],
[AC_MSG_ERROR([Need FreeType library version 2.4.5 or higher])])


AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT

# end of configure.ac
