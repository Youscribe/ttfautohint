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

DOCIMAGES = ttfautohintGUI.png

DOCGEN = ttfautohint.html \
         ttfautohint.pdf

dist_doc_DATA = $(DOCSRC) \
                $(DOCIMAGES) \
                $(DOCGEN)

ttfautohint-2.pandoc: $(top_srcdir)/lib/ttfautohint.h
	$(SED) -f $(srcdir)/c2pandoc.sed < $< > $@

# XXX embed CSS data
ttfautohint.html: $(DOCSRC) $(DOCIMAGES)
	$(PANDOC) --toc -S -s -o $@ $(DOCSRC)

# XXX write template
ttfautohint.pdf: $(DOCSRC) $(DOCIMAGES)
	$(PANDOC) --toc -S -s -o $@ $(DOCSRC)

# end of Makefile.am
