#
# Makefile.inc for Makefiles in subdirectories.
#

# Use customized html-chunk.xsl and/or html-single.xsl file if they exist...
# Also add html-common.xsl to depends if it exists.
ifeq (html-common.xsl,$(wildcard html-common.xsl))
COMMON_XSL_DEPS := html-common.xsl ../html-common.xsl
else
COMMON_XSL_DEPS := ../html-common.xsl
endif

ifeq (html-chunk.xsl,$(wildcard html-chunk.xsl))
HTML_CHUNK_XSL := html-chunk.xsl
CHUNK_XSL_DEPS := $(HTML_CHUNK_XSL) ../html-chunk.xsl $(COMMON_XSL_DEPS)
else
HTML_CHUNK_XSL := ../html-chunk.xsl
CHUNK_XSL_DEPS := $(HTML_CHUNK_XSL) ../html-common.xsl
endif

ifeq (html-single.xsl,$(wildcard html-single.xsl))
HTML_SINGLE_XSL := html-single.xsl
XSL_DEPS := $(HTML_SINGLE_XSL) ../html-single.xsl $(COMMON_XSL_DEPS)
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

SYMLINKS_DEPS:=$(if $(findstring yes,$(USE_SYMLINKS)), $(patsubst ../en/%,%,$(wildcard ../en/*.xml)))

xmllint: main.xml $(SYMLINKS_DEPS)
	../xmllint.sh $<

$(HTMLDIR)/index.html: main.xml $(CHUNK_XSL_DEPS) $(SYMLINKS_DEPS)
ifndef HTMLDIR
	$(warning $(HTMLDIR))
	$(warning Error: HTMLDIR not set!!!)
	$(warning Typically this means, that you've run make from a subdir of DOCS/xml.)
	$(error Don't do this!)
endif
	-rm -f $(HTMLDIR)/*
	../xmllint.sh $<
	cp -f $(HTML_STYLESHEET) $(HTMLDIR)/
	../xsltproc.sh $(HTMLDIR)/ $(HTML_CHUNK_XSL) $<

$(HTMLFILE): main.xml $(XSL_DEPS) $(SYMLINKS_DEPS)
ifndef HTMLFILE
	$(warning Error: HTMLFILE not set!!!)
	$(warning Typically this means, that you've run make from a subdir of DOCS/xml.)
	$(error Don't do this!)
endif
	-rm -f $(HTMLFILE)
	../xmllint.sh $<
	cp -f $(HTML_STYLESHEET) `dirname $(HTMLFILE)`
	../xsltproc.sh $(HTMLFILE) $(HTML_SINGLE_XSL) $<

../html-chunk.xsl ../html-single.xsl main.xml:
	cd .. && sh configure

$(filter-out main.xml, $(patsubst ../en/%,%, $(wildcard ../en/*.xml))):
	$(if $(findstring yes,$(USE_SYMLINKS)), ,exit 0;)\
	for file in ../en/*.xml ; do \
	if ! test -r `basename $$file` ; then \
		ln -s $$file `basename $$file` ; \
	fi; \
	done

.PHONY: all html-chunked html-single xmllint
