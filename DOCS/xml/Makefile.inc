#
# Makefile.inc for Makefiles in subdirectories.
#

HTML_CHUNK_XSL := ../html-chunk.xsl
CHUNK_XSL_DEPS := $(HTML_CHUNK_XSL) ../html-common.xsl

HTML_SINGLE_XSL := ../html-single.xsl
SINGLE_XSL_DEPS := $(HTML_SINGLE_XSL) ../html-common.xsl

# This is the main target...
all: html-chunked html-single
html-chunked: $(HTMLDIR)/index.html
html-single:  $(HTMLDIR)/MPlayer.html

SYMLINKS_DEPS := $(filter-out main.xml,$(patsubst ../en/%,%,$(wildcard ../en/*.xml)))

xmllint: main.xml $(SYMLINKS_DEPS)
	../xmllint.sh $<

$(HTMLDIR)/default.css:
	cp -f ../default.css $(@D)

$(HTMLDIR)/index.html: main.xml $(CHUNK_XSL_DEPS) $(SYMLINKS_DEPS) xmllint $(HTMLDIR)/default.css
	../xsltproc.sh $(HTMLDIR)/ $(HTML_CHUNK_XSL) $<

$(HTMLDIR)/MPlayer.html: main.xml $(SINGLE_XSL_DEPS) $(SYMLINKS_DEPS) xmllint $(HTMLDIR)/default.css
	../xsltproc.sh $@ $(HTML_SINGLE_XSL) $<

../html-chunk.xsl ../html-single.xsl main.xml:
	cd .. && sh configure

$(SYMLINKS_DEPS):
	test -r $(@F) || ln -s ../en/$@ $@

.PHONY: all html-chunked html-single xmllint
