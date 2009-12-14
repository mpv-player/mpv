#
# Makefile.inc for Makefiles in subdirectories.
#

# Use customized html-chunk.xsl and/or html-single.xsl file if they exist...
# Also add html-common.xsl to depends if it exists.
COMMON_XSL_DEPS := ../html-common.xsl $(wildcard html-common.xsl)

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
html-single:  $(HTMLDIR)/MPlayer.html

SYMLINKS_DEPS:=$(if $(findstring yes,$(USE_SYMLINKS)), $(patsubst ../en/%,%,$(wildcard ../en/*.xml)))

xmllint: main.xml $(SYMLINKS_DEPS)
	../xmllint.sh $<

$(HTMLDIR)/default.css:
	cp -f $(HTML_STYLESHEET) $(@D)

$(HTMLDIR)/index.html: main.xml $(CHUNK_XSL_DEPS) $(SYMLINKS_DEPS) xmllint $(HTMLDIR)/default.css
	../xsltproc.sh $(HTMLDIR)/ $(HTML_CHUNK_XSL) $<

$(HTMLDIR)/MPlayer.html: main.xml $(XSL_DEPS) $(SYMLINKS_DEPS) xmllint $(HTMLDIR)/default.css
	../xsltproc.sh $@ $(HTML_SINGLE_XSL) $<

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
