# configure.ac

AC_INIT([ttfautohint],
        [0.1],
        [freetype-devel@nongnu.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AM_SILENT_RULES([yes])

AC_PROG_CC
AC_PROG_RANLIB

AC_ARG_WITH(freetype-config,
            [  --with-freetype-config=PROG   Use FreeType configuration program PROG],
            freetype_config=$withval,
            freetype_config=yes)

if test "$freetype_config" = "yes"; then 
  AC_PATH_PROG(ft_config,
               freetype-config,
               no)
  if test "$ft_config" = "no"; then
    AC_MSG_ERROR([You must have FreeType installed; see http://www.freetype.org/])
  fi
else
  ft_config="$freetype_config"
fi

FREETYPE_CPPFLAGS="`$ft_config --cflags`"
FREETYPE_LDFLAGS="`$ft_config --libs`"

AC_SUBST(FREETYPE_CPPFLAGS)
AC_SUBST(FREETYPE_LDFLAGS)


AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT

# end of configure.ac
