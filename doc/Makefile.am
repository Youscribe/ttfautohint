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

DOCIMGSVG = img/blue-zones.svg \
            img/glyph-terms.svg \
            img/o-and-i.svg \
            img/segment-edge.svg

DOCIMGPDF = img/blue-zones.pdf \
            img/glyph-terms.pdf \
            img/o-and-i.pdf \
            img/segment-edge.pdf

DOCIMGPNG = img/ttfautohintGUI.png \
            img/a-before-hinting.png \
            img/a-after-hinting.png \
            img/a-after-autohinting.png \
            img/afii10108-11px-after-hinting.png \
            img/afii10108-11px-before-hinting.png \
            img/afii10108-12px-after-hinting.png \
            img/afii10108-12px-before-hinting.png \
            img/e-17px-x14.png \
            img/e-17px-x17.png \
            img/ff-g-26px.png \
            img/ff-g-26px-wD.png

DOC = ttfautohint.html \
      ttfautohint.pdf \
      ttfautohint.txt \
      $(DOCIMGPNG) \
      $(DOCIMGSVG)

EXTRA_DIST = c2pandoc.sed \
             handle-images.hs \
             make-snapshot.sh \
             strip-comments.sh \
             ttfautohint-1.pandoc \
             ttfautohint-2.pandoc \
             ttfautohint-3.pandoc \
             template.html \
             template.tex \
             ttfautohint.css

if WITH_DOC
  nobase_dist_doc_DATA = $(DOC)
endif


ttfautohint-2.pandoc: $(top_srcdir)/lib/ttfautohint.h
	$(SED) -f $(srcdir)/c2pandoc.sed < $< > $@

ttfautohint.txt: $(DOCSRC)
	$(SHELL) $(srcdir)/strip-comments.sh $^ > $@

if WITH_DOC

  # the pandoc image filter
  handle-images: handle-images.hs
	  $(GHC) -dynamic --make $@

  # suffix rules must always start in column 0
.svg.pdf:
	  $(INKSCAPE) --export-pdf=$@ $<

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

  ttfautohint.html: ttfautohint.txt $(DOCIMGPNG) $(DOCIMGSVG) handle-images \
                    ttfautohint.css template.html $(top_srcdir)/.version
	  $(PANDOC) --to=json \
	            --smart \
	            $< \
	  | ./handle-images ".svg" \
	  | $(PANDOC) --from=json \
	              --template=$(srcdir)/template.html \
	              --variable="version:$(VERSION)" \
	              --toc \
	              --include-in-header=$(srcdir)/ttfautohint.css \
	              --standalone \
	              --output=$@

  ttfautohint.pdf: ttfautohint.txt $(DOCIMGPNG) $(DOCIMGPDF) handle-images \
                   template.tex $(top_srcdir)/.version
	  $(PANDOC) --to=json \
	            --smart \
	            $< \
	  | ./handle-images ".pdf" \
	  | TEXINPUTS="$(srcdir);" \
	    $(PANDOC) --from=json \
	              --latex-engine=$(PDFLATEX) \
	              --template=$(srcdir)/template.tex \
	              --variable="version:$(VERSION)" \
	              --number-sections \
	              --toc \
	              --chapters \
	              --standalone \
	              --output=$@
else
.svg.pdf:
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         please install inkscape and reconfigure"

  img/ttfautohintGUI.png: $(top_srcdir)/frontend/maingui.cpp \
                          $(top_srcdir)/configure.ac
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         please install ImageMagick's \`import' tool and reconfigure"

  ttfautohint.html: ttfautohint.txt $(DOCIMGPNG) $(DOCIMGSVG) \
                    ttfautohint.css template.html $(top_srcdir)/.version
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         pleasae install pandoc and reconfigure"

  ttfautohint.pdf: $ttfautohint.txt $(DOCIMGPNG) $(DOCIMGPDF) \
                   template.tex $(top_srcdir)/.version
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         please install pdftex and pandoc, then reconfigure"
endif

# end of Makefile.am
