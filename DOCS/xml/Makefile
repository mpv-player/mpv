# Makefile for generating the HTML documentation

include ../../config.mak

# Generated chunked HTML files go here.
HTML_CHUNKED = ../HTML

# Generated single HTML files go here.
HTML_SINGLE = ../HTML-single

all: html-chunked html-single

help:
	@echo "Targets:"
	@echo "********"
	@echo "all               : Build everything for configured languages (default)."
	@echo "html-single       : HTML documentation for configured languages (single file)"
	@echo "html-single-LANG  : As above, but only one language."
	@echo "html-chunked      : HTML documentation for configured languages (multiple files)"
	@echo "html-chunked-LANG : As above, but only one language."
	@echo "xmllint           : Check syntax of all xml files."
	@echo "xmllint-LANG      : Check syntax of LANG xml files."
	@echo "clean             : Purge the 'HTML' and 'HTML-single' directories."
	@echo "releaseclean      : Remove generated files but keep the HTML."
	@echo "distclean         : Remove ALL generated files."
	@echo "Substitute LANG for one of $(DOC_LANG_ALL)"

html-chunked html-single: xsltproc.sh xmllint.sh
html-chunked: $(addprefix html-chunked-,$(DOC_LANGS))
html-single:  $(addprefix html-single-,$(DOC_LANGS))

xmllint: xmllint.sh $(addprefix xmllint-,$(DOC_LANG_ALL))

$(foreach lang,$(DOC_LANG_ALL),$(HTML_CHUNKED)/$(lang) $(HTML_SINGLE)/$(lang)):
	-mkdir -p $@

xsltproc.sh xmllint.sh:
	sh configure

define lang-def
html-chunked-$(1): $(HTML_CHUNKED)/$(1) xsltproc.sh xmllint.sh
	$(MAKE) HTMLDIR=../$$< -C $(1) html-chunked

html-single-$(1): $(HTML_SINGLE)/$(1) xsltproc.sh xmllint.sh
	$(MAKE) HTMLFILE=../$$</MPlayer.html -C $(1) html-single

xmllint-$(1): xmllint.sh
	$(MAKE) -C $(1) xmllint
endef

$(foreach lang, $(DOC_LANG_ALL),$(eval $(call lang-def,$(lang))))

clean:
	rm -rf $(HTML_CHUNKED) $(HTML_SINGLE)

releaseclean:
	rm -f $(foreach lang,$(DOC_LANG_ALL),$(lang)/main.xml)
	rm -f $$(find . -name *.xml -type l)
	rm -f html-chunk.xsl html-single.xsl xsltproc.sh xmllint.sh

distclean: clean releaseclean

.PHONY: all help html-chunked* html-single* xmllint xmllint-* *clean*
