# Makefile for generating the HTML documentation

# List of subdirectories to be processed.
SUBDIRS = en es fr hu pl cs de ru zh

# Generated chunked HTML files go here.
HTML_CHUNKED = ../HTML

# Generated single HTML files go here.
HTML_SINGLE = ../HTML-single

.PHONY: all
all: html-chunked html-single

.PHONY: help
help:
	@echo "Targets:"
	@echo "********"
	@echo "all               : Build everything (default)."
	@echo "html-single       : Build HTML documentation (single file)."
	@echo "html-single-LANG  : As above, but only one language."
	@echo "html-chunked      : Build HTML documentation (multiple files)."
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
	@echo "Substitute LANG for one of $(SUBDIRS)"

.PHONY: html-chunked chunked-dir
html-chunked: xsltproc.sh xmllint.sh chunked-dir $(addprefix html-chunked-,$(SUBDIRS))
chunked-dir:
	-mkdir -p $(HTML_CHUNKED)

.PHONY: html-single single-dir
html-single: xsltproc.sh xmllint.sh single-dir $(addprefix html-single-,$(SUBDIRS))
single-dir:
	-mkdir -p $(HTML_SINGLE)

.PHONY: xmllint
xmllint: xmllint.sh $(addprefix xmllint-,$(SUBDIRS))

.PHONY: clean-html-chunked
clean-html-chunked:
	-rm -rf $(HTML_CHUNKED)

.PHONY: clean-html-single
clean-html-single:
	-rm -rf $(HTML_SINGLE)

.PHONY: releaseclean distclean clean
releaseclean: $(addprefix releaseclean-,$(SUBDIRS))
	-rm -f html-chunk.xsl html-single.xsl xsltproc.sh xmllint.sh

distclean: clean releaseclean

xsltproc.sh xmllint.sh:
	sh configure

clean: clean-html-chunked clean-html-single

define lang-def
.PHONY: html-chunked-$(1) html-single-$(1) xmllint-$(1) \
  distclean-$(1) releaseclean-$(1) clean-$(1)
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

$(foreach lang, $(SUBDIRS),$(eval $(call lang-def,$(lang))))
