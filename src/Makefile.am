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

AM_CPPFLAGS = $(FREETYPE_CPPFLAGS)
AM_LDFLAGS = $(FREETYPE_LDFLAGS)

noinst_LIBRARIES = libttfautohint.a
libttfautohint_a_SOURCES = ta.h \
                           tabytecode.c tabytecode.h \
                           tacvt.c \
                           tadsig.c \
                           tadummy.c tadummy.h \
                           tafpgm.c \
                           tagloadr.c tagloadr.h \
                           taglobal.c taglobal.h \
                           taglyf.c \
                           tahints.c tahints.h \
                           talatin.c talatin.h \
                           taloader.c taloader.h \
                           taloca.c \
                           tamaxp.c \
                           taprep.c \
                           tasort.c tasort.h \
                           tatables.c tatables.h \
                           tatypes.h \
                           ttfautohint.c ttfautohint.h ttfautohint-errors.h

bin_PROGRAMS = ttfautohint
ttfautohint_SOURCES = hint.c \
                      ttfautohint.h
ttfautohint_LDADD = libttfautohint.a \
                    $(FREETYPE_LDFLAGS)

dist_man_MANS = ttfautohint.1

ttfautohint.1: hint.c $(top_srcdir)/configure.ac
	$(MAKE) $(AM_MAKEFLAGS) ttfautohint$(EXEEXT)
	help2man --output=ttfautohint.1 \
	         --no-info \
	         --name="add new auto-generated hints to a TrueType font" \
	         ./ttfautohint$(EXEEXT)

## end of Makefile.am
