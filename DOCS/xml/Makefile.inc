#
# Makefile.inc for Makefiles in subdirectories.
#

# Use customized html-chunk.xsl and/or html-single.xsl file if they exist...
# Also add html-common.xsl to depends if it exists.
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

ifeq (html-single.xsl,$(wildcard html-single.xsl))
HTML_SINGLE_XSL := html-single.xsl
ifeq (html-common.xsl,$(wildcard html-common.xsl))
XSL_DEPS := $(HTML_SINGLE_XSL) html-common.xsl ../html-single.xsl ../html-common.xsl
else
XSL_DEPS := $(HTML_SINGLE_XSL) ../html-single.xsl ../html-common.xsl
endif
else
HTML_SINGLE_XSL := ../html-single.xsl
XSL_DEPS := $(HTML_SINGLE_XSL) ../html-common.xsl
endif

# Fall back to the default HTML stylesheet if none is specified.
HTML_STYLESHEET ?= ../default.css

# This is the main target...
all: html-chunked html-single
html-chunked: $(HTMLDIR)/index.html
html-single: $(HTMLFILE)

$(HTMLDIR)/index.html: documentation.xml $(CHUNK_XSL_DEPS)
	@if test "$(HTMLDIR)" = "" ; then \
		echo "Error: HTMLDIR not set!!!"; \
		echo "Typically this means, that you've run make from a subdir of DOCS/xml."; \
		echo "Don't do this!"; \
		false; \
	fi
	@if test "$(USE_SYMLINKS)" = "yes" ; then \
		for file in ../en/*.xml ; do \
		if ! test -r `basename $$file` ; then \
			ln -s $$file `basename $$file` ; \
		fi ; \
		done ; \
	fi
	-rm -f $(HTMLDIR)/*
	../xmllint.sh $<
	cp -f $(HTML_STYLESHEET) $(HTMLDIR)/
	../xsltproc.sh $(HTMLDIR)/ $(HTML_CHUNK_XSL) $<

$(HTMLFILE): documentation.xml $(XSL_DEPS)
	@if test "$(HTMLFILE)" = "" ; then \
		echo "Error: HTMLFILE not set!!!"; \
		echo "Typically this means, that you've run make from a subdir of DOCS/xml."; \
		echo "Don't do this!"; \
		false; \
	fi
	@if test "$(USE_SYMLINKS)" = "yes" ; then \
		for file in ../en/*.xml ; do \
		if ! test -r `basename $$file` ; then \
			ln -s $$file `basename $$file` ; \
		fi ; \
		done ; \
	fi
	-rm -f $(HTMLFILE)
	../xmllint.sh $<
	cp -f $(HTML_STYLESHEET) `dirname $(HTMLFILE)`
	../xsltproc.sh $(HTMLFILE) $(HTML_SINGLE_XSL) $<

../html-chunk.xsl ../html-single.xsl:
	cd .. && sh configure

distclean:
	@if test "$(USE_SYMLINKS)" = "yes" ; then \
		rm -f `find *.xml -type l`; \
	fi
