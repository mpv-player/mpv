#
# Makefile.inc for Makefiles in subdirectories.
#

HTML_CHUNK_XSL := ../html-chunk.xsl
CHUNK_XSL_DEPS := $(HTML_CHUNK_XSL) ../html-common.xsl

HTML_SINGLE_XSL := ../html-single.xsl
XSL_DEPS := $(HTML_SINGLE_XSL) ../html-common.xsl

# This is the main target...
all: html-chunked html-single
html-chunked: $(HTMLDIR)/index.html
html-single:  $(HTMLDIR)/MPlayer.html

SYMLINKS_DEPS:=$(patsubst ../en/%,%,$(wildcard ../en/*.xml))

xmllint: main.xml $(SYMLINKS_DEPS)
	../xmllint.sh $<

$(HTMLDIR)/default.css:
	cp -f ../default.css $(@D)

$(HTMLDIR)/index.html: main.xml $(CHUNK_XSL_DEPS) $(SYMLINKS_DEPS) xmllint $(HTMLDIR)/default.css
	../xsltproc.sh $(HTMLDIR)/ $(HTML_CHUNK_XSL) $<

$(HTMLDIR)/MPlayer.html: main.xml $(XSL_DEPS) $(SYMLINKS_DEPS) xmllint $(HTMLDIR)/default.css
	../xsltproc.sh $@ $(HTML_SINGLE_XSL) $<

../html-chunk.xsl ../html-single.xsl main.xml:
	cd .. && sh configure

$(filter-out main.xml, $(patsubst ../en/%,%, $(wildcard ../en/*.xml))):
	for file in ../en/*.xml ; do \
	if ! test -r `basename $$file` ; then \
		ln -s $$file `basename $$file` ; \
	fi; \
	done

.PHONY: all html-chunked html-single xmllint
