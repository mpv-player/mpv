# Makefile for generating the HTML documentation

include ../../config.mak
include xml.mak

# Generated HTML files go here.
HTML = ../HTML

MAIN_XML_ALL = $(foreach lang,$(DOC_LANG_ALL),$(lang)/main.xml)
CONFIGURE_GENERATED = html-chunk.xsl html-single.xsl xml.mak

all: html-chunked html-single

html-chunked: $(addprefix html-chunked-,$(DOC_LANGS))
html-single:  $(addprefix html-single-,$(DOC_LANGS))

xmllint: $(addprefix xmllint-,$(DOC_LANGS))

$(CONFIGURE_GENERATED) $(MAIN_XML_ALL):
	./configure

define lang-def
html-chunked-$(lang): $(HTML)/$(lang)/dummy.html
html-single-$(lang):  $(HTML)/$(lang)/MPlayer.html
$(HTML)/$(lang)/dummy.html $(HTML)/$(lang)/MPlayer.html: $(lang)/main.xml $(wildcard $(lang)/*.xml) html-common.xsl $(HTML)/$(lang)/default.css

$(HTML)/$(lang)/default.css:
	mkdir -p $$(@D)
	cp -f default.css $$(@D)

$(HTML)/$(lang)/dummy.html:
	SGML_CATALOG_FILES=$(CATALOG) $(XSLT_COMMAND) $$@ html-chunk.xsl $$<

$(HTML)/$(lang)/MPlayer.html:
	SGML_CATALOG_FILES=$(CATALOG) $(XSLT_COMMAND) $$@ html-single.xsl $$<

xmllint-$(lang):
	SGML_CATALOG_FILES=$(CATALOG) $(XMLLINT_COMMAND) $(lang)/main.xml
endef

$(foreach lang, $(DOC_LANG_ALL),$(eval $(lang-def)))

clean:
	-rm -rf $(HTML)

# Remove generated files but keep the HTML (for release tarballs).
releaseclean:
	-rm -f $(CONFIGURE_GENERATED)
	-rm -f $(MAIN_XML_ALL)

distclean: clean releaseclean

.PHONY: all html-chunked html-single xmllint
.PHONY: html-chunked-* html-single-* xmllint-* *clean
