## Makefile.am

# Copyright (C) 2011 by Werner Lemberg.
#
# This file is part of the ttfautohint library, and may only be used,
# modified, and distributed under the terms given in `COPYING'.  By
# continuing to use, modify, or distribute this file you indicate that you
# have read `COPYING' and understand and accept it fully.
#
# The file `COPYING' mentioned in the previous paragraph is distributed
# with the ttfautohint library.

AM_CPPFLAGS = -I$(top_srcdir)/lib \
              -I$(top_builddir)/gnulib/src \
              -I$(top_srcdir)/gnulib/src \
              $(FREETYPE_CPPFLAGS)
AM_LDFLAGS = $(top_builddir)/lib/libttfautohint.a \
             $(top_builddir)/gnulib/src/libgnu.la \
             $(LTLIBINTL) \
             $(FREETYPE_LDFLAGS)

bin_PROGRAMS = ttfautohint
ttfautohint_SOURCES = main.cpp

dist_man_MANS = ttfautohint.1

ttfautohint.1: $(top_srcdir)/frontend/main.cpp $(top_srcdir)/configure.ac
	$(MAKE) $(AM_MAKEFLAGS) ttfautohint$(EXEEXT)
	$(HELP2MAN) --output=$@ \
	            --no-info \
	            --name="add new auto-generated hints to a TrueType font" \
	            ./ttfautohint$(EXEEXT)

## end of Makefile.am
