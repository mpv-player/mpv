#
# Makefile.inc for Makefiles in subdirectories.
#

# Use customized html.xsl and/or html-chunk.xsl file if they exist...
# Also add html-common.xsl to depends if it exists.
ifeq (html.xsl,$(wildcard html.xsl))
HTML_XSL := html.xsl
ifeq (html-common.xsl,$(wildcard html-common.xsl))
XSL_DEPS := $(HTML_XSL) html-common.xsl ../html.xsl ../html-common.xsl
else
XSL_DEPS := $(HTML_XSL) html-common.xsl ../html.xsl ../html-common.xsl
endif
else
HTML_XSL := ../html.xsl
XSL_DEPS := $(HTML_XSL) ../html-common.xsl
endif

ifeq (html-chunk.xsl,$(wildcard html-chunk.xsl))
HTML_CHUNK_XSL := html-chunk.xsl
ifeq (html-common.xsl,$(wildcard html-common.xsl))
CHUNK_XSL_DEPS := $(HTML_CHUNK_XSL) html-common.xsl ../html-chunk.xsl ../html-common.xsl
else
CHUNK_XSL_DEPS := $(HTML_CHUNK_XSL) ../html-chunk.xsl ../html-common.xsl
endif
else
HTML_CHUNK_XSL := ../html-chunk.xsl
CHUNK_XSL_DEPS := $(HTML_CHUNK_XSL) ../html-common.xsl
endif

# Fall back to the default HTML stylesheet if none is specified.
HTML_STYLESHEET ?= ../default.css

# This is the main target...
all: html html-chunked
html: $(HTMLFILE)
html-chunked: $(HTMLDIR)/index.html

$(HTMLDIR)/index.html: documentation.xml $(CHUNK_XSL_DEPS)
	@if test "$(HTMLDIR)" = "" ; then echo "Error: HTMLDIR not set!!!"; echo "Typically this means, that you've run make from subdir of DOCS/xml. Don't do this!" ; false; fi
	if test "$(USE_SYMLINKS)" = "yes" ; then for file in ../en/*.xml ; do if ! test -r `basename $$file` ; then ln -s $$file `basename $$file` ; fi ; done ; fi
	-rm -f $(HTMLDIR)/*
	../xmllint.sh $<
	cp $(HTML_STYLESHEET) $(HTMLDIR)/
	../xsltproc.sh $(HTMLDIR)/ $(HTML_CHUNK_XSL) $<

$(HTMLFILE): documentation.xml $(XSL_DEPS)
	@if test "$(HTMLFILE)" = "" ; then echo "Error: HTMLFILE not set!!!"; echo "Typically this means, that you've run make from subdir of DOCS/xml. Don't do this!" ; false; fi
	if test "$(USE_SYMLINKS)" = "yes" ; then for file in ../en/*.xml ; do if ! test -r `basename $$file` ; then ln -s $$file `basename $$file` ; fi ; done ; fi
	-rm -f $(HTMLFILE)
	../xmllint.sh $<
	cp -f $(HTML_STYLESHEET) `dirname $(HTMLFILE)`
	../xsltproc.sh $(HTMLFILE) $(HTML_XSL) $<

../html.xsl:
	cd .. && sh configure

distclean:
	if test "$(USE_SYMLINKS)" = "yes" ; then for file in *.xml ; do if test -L $$file && test "`readlink $$file`" = "../en/$$file" ; then rm $$file ; fi ; done ; fi
