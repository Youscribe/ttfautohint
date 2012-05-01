## Makefile.am

# due to a limitation of `autoreconf',
# all -I directives currently must be set on a single line
ACLOCAL_AMFLAGS = -I gnulib/m4 -I m4

SUBDIRS = gnulib/src \
          lib \
          frontend \
          doc
EXTRA_DIST = bootstrap \
             bootstrap.conf \
             FTL.TXT \
             gnulib/m4/gnulib-cache.m4 \
             GPLv2.TXT \
             README \
             TODO \
             .version

BUILT_SOURCES = .version
.version:
	echo $(VERSION) > $@-t && mv $@-t $@

dist-hook:
	echo $(VERSION) > $(distdir)/VERSION

## end of Makefile.am
