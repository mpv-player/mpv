# Makefile for generating the HTML documentation

include ../../config.mak

# Generated HTML files go here.
HTML = ../HTML

MAIN_XML_ALL = $(foreach lang,$(DOC_LANG_ALL),$(lang)/main.xml)
CONFIGURE_GENERATED = html-chunk.xsl html-single.xsl xsltproc.sh xmllint.sh

all: html-chunked html-single

help:
	@echo "Targets:"
	@echo "********"
	@echo "all               : Build everything for configured languages (default)."
	@echo "html-single       : HTML documentation for configured languages (single file)"
	@echo "html-single-LANG  : As above, but only one language."
	@echo "html-chunked      : HTML documentation for configured languages (multiple files)"
	@echo "html-chunked-LANG : As above, but only one language."
	@echo "xmllint           : Check syntax of XML files for configured languages."
	@echo "xmllint-LANG      : Check syntax of LANG XML files."
	@echo "clean             : Purge the 'HTML' directory."
	@echo "releaseclean      : Remove generated files but keep the HTML."
	@echo "distclean         : Remove ALL generated files."
	@echo "Substitute LANG for one of $(DOC_LANG_ALL)"

html-chunked: $(addprefix html-chunked-,$(DOC_LANGS))
html-single:  $(addprefix html-single-,$(DOC_LANGS))

xmllint: $(addprefix xmllint-,$(DOC_LANGS))

$(foreach lang,$(DOC_LANG_ALL),$(HTML)/$(lang)):
	mkdir -p $@

$(CONFIGURE_GENERATED) $(MAIN_XML_ALL):
	sh configure

define lang-def
html-chunked-$(lang) html-single-$(lang): $(HTML)/$(lang) $(CONFIGURE_GENERATED)

html-chunked-$(lang):
	$(MAKE) HTMLDIR=$$< LANG=$(lang) -f Makefile.inc html-chunked

html-single-$(lang):
	$(MAKE) HTMLDIR=$$< LANG=$(lang) -f Makefile.inc html-single

xmllint-$(lang): xmllint.sh
	$(MAKE) -C $(lang) xmllint
endef

$(foreach lang, $(DOC_LANG_ALL),$(eval $(lang-def)))

clean:
	-rm -rf $(HTML)

releaseclean:
	-rm -f $(CONFIGURE_GENERATED)
	-rm -f $(MAIN_XML_ALL)

distclean: clean releaseclean

.PHONY: all help html-chunked html-single xmllint
.PHONY: html-chunked-* html-single-* xmllint-* *clean*
