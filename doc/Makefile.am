# Makefile.am

# Copyright (C) 2011-2012 by Werner Lemberg.
#
# This file is part of the ttfautohint library, and may only be used,
# modified, and distributed under the terms given in `COPYING'.  By
# continuing to use, modify, or distribute this file you indicate that you
# have read `COPYING' and understand and accept it fully.
#
# The file `COPYING' mentioned in the previous paragraph is distributed
# with the ttfautohint library.

DOCSRC = ttfautohint-1.pandoc \
         ttfautohint-2.pandoc \
         ttfautohint-3.pandoc

DOCIMGSRC = img/blue-zones.svg \
            img/glyph-terms.svg \
            img/o-and-i.svg \
            img/segment-edge.svg

DOCIMG = img/ttfautohintGUI.png \
         img/a-before-hinting.png \
         img/a-after-hinting.png \
         img/a-after-autohinting.png \
         img/afii10108-11px-after-hinting.png \
         img/afii10108-11px-before-hinting.png \
         img/afii10108-12px-after-hinting.png \
         img/afii10108-12px-before-hinting.png \
         img/blue-zones.png \
         img/e-17px-x14.png \
         img/e-17px-x17.png \
         img/glyph-terms.png \
         img/o-and-i.png \
         img/segment-edge.png

DOC = ttfautohint.html \
      ttfautohint.pdf \
      ttfautohint.txt \
      $(DOCIMG)

if WITH_DOC
  nobase_dist_doc_DATA = $(DOC) \
                         $(DOCIMGSRC)
endif


ttfautohint-2.pandoc: $(top_srcdir)/lib/ttfautohint.h
	$(SED) -f $(srcdir)/c2pandoc.sed < $< > $@

ttfautohint.txt: $(DOCSRC)
	cat $(DOCSRC) > $@

if WITH_DOC
  # this is a temporary solution:
  # the right one is to write a pandoc filter
  # which uses `foo.svg' for HTML and `foo.pdf' for PDF output
.svg.png:
	  $(INKSCAPE) --export-background=white --export-png=$@ $<

  # build snapshot image of ttfautohintGUI:
  # this needs X11 and ImageMagick's `import' tool
  # (in the `make-snaphshot.sh' script)
  img/ttfautohintGUI.png: $(top_srcdir)/frontend/maingui.cpp \
                          $(top_srcdir)/configure.ac
	  cd $(top_builddir)/frontend \
	    && $(MAKE) $(AM_MAKEFLAGS) ttfautohintGUI$(EXEEXT)
	  -mkdir img 2> /dev/null
	  $(SHELL) $(srcdir)/make-snapshot.sh \
	             $(top_builddir)/frontend/ttfautohintGUI$(EXEEXT) $@

  ttfautohint.html: ttfautohint.txt $(DOCIMG) \
                    ttfautohint.css template.html $(top_srcdir)/.version
	  $(PANDOC) --template=template.html \
	            --variable="version:$(VERSION)" \
	            --toc \
	            --include-in-header=ttfautohint.css \
	            --smart \
	            --standalone \
	            --output=$@ \
	            $<

  ttfautohint.pdf: ttfautohint.txt $(DOCIMG) \
                   template.tex $(top_srcdir)/.version
	  $(PANDOC) --latex-engine=$(PDFLATEX) \
	            --template=template.tex \
	            --variable="version:$(VERSION)" \
	            --number-sections \
	            --toc \
	            --smart \
	            --standalone \
	            --output=$@ \
	            $<
else
.svg.png:
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         please install inkscape and reconfigure"

  img/ttfautohintGUI.png: $(top_srcdir)/frontend/maingui.cpp \
                          $(top_srcdir)/configure.ac
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         please install ImageMagick's \`import' tool and reconfigure"

  ttfautohint.html: ttfautohint.txt $(DOCIMG) \
                    ttfautohint.css template.html $(top_srcdir)/.version
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         pleasae install pandoc and reconfigure"

  ttfautohint.pdf: $ttfautohint.txt $(DOCIMG) \
                   template.tex $(top_srcdir)/.version
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         please install pdftex and pandoc, then reconfigure"
endif

# end of Makefile.am
