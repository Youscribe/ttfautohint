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

DOCIMAGES = img/ttfautohintGUI.png

DOCGEN = ttfautohint.html \
         ttfautohint.pdf \
         $(DOCIMAGES)

if WITH_DOC
  nobase_dist_doc_DATA = $(DOCSRC) \
                         ttfautohint.css \
                         template.html \
                         $(DOCGEN)
endif

ttfautohint-2.pandoc: $(top_srcdir)/lib/ttfautohint.h
	$(SED) -f $(srcdir)/c2pandoc.sed < $< > $@

if WITH_DOC
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

  ttfautohint.html: $(DOCSRC) $(DOCIMAGES) ttfautohint.css template.html
	  $(PANDOC) --template=template.html \
	            --toc \
	            --include-in-header=ttfautohint.css \
	            --smart \
	            --standalone \
	            --output=$@ \
	            $(DOCSRC)

  # XXX write template
  ttfautohint.pdf: $(DOCSRC) $(DOCIMAGES)
	  $(PANDOC) --latex-engine=$(PDFLATEX) --toc -S -s -o $@ $(DOCSRC)
else
  img/ttfautohintGUI.png: $(top_srcdir)/frontend/maingui.cpp \
                          $(top_srcdir)/configure.ac
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         please install ImageMagick's \`import' tool and reconfigure"

  ttfautohint.html: $(DOCSRC) $(DOCIMAGES) ttfautohint.css template.html
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         pleasae install pandoc and reconfigure"

  ttfautohint.pdf: $(DOCSRC) $(DOCIMAGES)
	  @echo 1>&2 "warning: can't generate \`$@'"
	  @echo 1>&2 "         please install pdftex and pandoc, then reconfigure"
endif

# end of Makefile.am
