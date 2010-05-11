#
# Makefile.inc for Makefiles in subdirectories.
#

# Dependency information.
$(HTMLDIR)/index.html $(HTMLDIR)/MPlayer.html: $(wildcard $(LANG)/*.xml)

# This is the main target...
all: html-chunked html-single
html-chunked: $(HTMLDIR)/index.html
html-single:  $(HTMLDIR)/MPlayer.html

xmllint: $(LANG)/main.xml
	./xmllint.sh $<

$(HTMLDIR)/default.css:
	cp -f default.css $(@D)

$(HTMLDIR)/index.html: $(LANG)/main.xml html-chunk.xsl html-common.xsl $(HTMLDIR)/default.css xmllint
	./xsltproc.sh $(HTMLDIR)/ html-chunk.xsl $<

$(HTMLDIR)/MPlayer.html: $(LANG)/main.xml html-single.xsl html-common.xsl $(HTMLDIR)/default.css xmllint
	./xsltproc.sh $@ html-single.xsl $<

html-chunk.xsl html-single.xsl $(LANG)/main.xml:
	sh configure

.PHONY: all html-chunked html-single xmllint
