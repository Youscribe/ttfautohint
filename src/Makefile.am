## Makefile.am

AM_CPPFLAGS = $(FREETYPE_CPPFLAGS)
AM_LDFLAGS = $(FREETYPE_LDFLAGS)

noinst_LIBRARIES = libttfautohint.a
libttfautohint_a_SOURCES = ta.h \
                           tabytecode.c tabytecode.h \
                           tacvt.c \
                           tadummy.c tadummy.h \
                           tafpgm.c \
                           tagloadr.c tagloadr.h \
                           taglobal.c taglobal.h \
                           tahints.c tahints.h \
                           talatin.c talatin.h \
                           taloader.c taloader.h \
                           taprep.c \
                           tasort.c tasort.h \
                           tatables.c tatables.h \
                           tatypes.h \
                           ttfautohint.c ttfautohint.h

bin_PROGRAMS = ttfautohint
ttfautohint_SOURCES = hint.c \
                      ttfautohint.h
ttfautohint_LDADD = libttfautohint.a \
                    $(FREETYPE_LDFLAGS)

## end of Makefile.am
