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
              $(FREETYPE_CPPFLAGS)
AM_LDFLAGS = $(top_builddir)/lib/libttfautohint.a \
             $(FREETYPE_LDFLAGS)

bin_PROGRAMS = ttfautohint
ttfautohint_SOURCES = hint.cpp

dist_man_MANS = ttfautohint.1

ttfautohint.1: $(top_srcdir)/frontend/hint.cpp $(top_srcdir)/configure.ac
	$(MAKE) $(AM_MAKEFLAGS) ttfautohint$(EXEEXT)
	$(HELP2MAN) --output=$@ \
	            --no-info \
	            --name="add new auto-generated hints to a TrueType font" \
	            ./ttfautohint$(EXEEXT)

## end of Makefile.am
