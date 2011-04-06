## Makefile.am

AM_CPPFLAGS = $(FREETYPE_CPPFLAGS)
AM_LDFLAGS = $(FREETYPE_LDFLAGS)

noinst_LIBRARIES = libttfautohint.a
libttfautohint_a_SOURCES = ta.h \
                           tabytecode.c \
                           tabytecode.h \
                           tadummy.c \
                           tadummy.h \
                           tagloadr.c \
                           tagloadr.h \
                           taglobal.c \
                           taglobal.h \
                           tahints.c \
                           tahints.h \
                           talatin.c \
                           talatin.h \
                           taloader.c \
                           taloader.h \
                           tasort.c \
                           tasort.h \
                           tatypes.h \
                           ttfautohint.c \
                           ttfautohint.h

noinst_PROGRAMS = hint
hint_SOURCES = hint.c \
               ttfautohint.h
hint_LDADD = libttfautohint.a

## end of Makefile.am
