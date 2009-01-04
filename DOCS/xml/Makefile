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
	@echo "clean-html-single : Purge the 'HTML-single' directory."
	@echo "clean-html-chunked: Purge the 'HTML' directory."
	@echo "clean             : Purge the 'HTML' and 'HTML-single' directories."
	@echo "releaseclean      : Remove generated files but keep the HTML."
	@echo "distclean         : Remove ALL generated files."
	@echo "clean-LANG        : Remove HTML files for one language."
	@echo "releaseclean-LANG : Remove generated files for one language."
	@echo "distclean-LANG    : Remove ALL generated files for one language."
	@echo "Substitute LANG for one of $(DOC_LANG_ALL)"

html-chunked: xsltproc.sh xmllint.sh chunked-dir $(addprefix html-chunked-,$(DOC_LANGS))
chunked-dir:
	-mkdir -p $(HTML_CHUNKED)

html-single: xsltproc.sh xmllint.sh single-dir $(addprefix html-single-,$(DOC_LANGS))
single-dir:
	-mkdir -p $(HTML_SINGLE)

xmllint: xmllint.sh $(addprefix xmllint-,$(DOC_LANG_ALL))

clean-html-chunked:
	-rm -rf $(HTML_CHUNKED)

clean-html-single:
	-rm -rf $(HTML_SINGLE)

releaseclean: $(addprefix releaseclean-,$(DOC_LANG_ALL))
	-rm -f html-chunk.xsl html-single.xsl xsltproc.sh xmllint.sh

distclean: clean releaseclean

xsltproc.sh xmllint.sh:
	sh configure

clean: clean-html-chunked clean-html-single

define lang-def
html-chunked-$(1): xsltproc.sh xmllint.sh chunked-dir
	-(mkdir -p $(HTML_CHUNKED)/$(1))
	$(MAKE) HTMLDIR=../$(HTML_CHUNKED)/$(1) -C $(1) html-chunked

html-single-$(1): xsltproc.sh xmllint.sh single-dir
	-(mkdir -p $(HTML_SINGLE)/$(1))
	$(MAKE) HTMLFILE=../$(HTML_SINGLE)/$(1)/MPlayer.html -C $(1) html-single

xmllint-$(1): xmllint.sh
	$(MAKE) HTMLFILE=../$(HTML_SINGLE)/$(1)/MPlayer.html -C $(1) xmllint

distclean-$(1): clean-$(1) releaseclean-$(1)

clean-$(1):
	-rm -rf $(HTML_SINGLE)/$(1) $(HTML_CHUNKED)/$(1)

releaseclean-$(1):
	$(MAKE) HTMLDIR=../$(HTML_CHUNKED)/$(1) -C $(1) distclean
endef

$(foreach lang, $(DOC_LANG_ALL),$(eval $(call lang-def,$(lang))))

.PHONY: all help html-chunked* chunked-dir html-single* single-dir xmllint* *clean*
