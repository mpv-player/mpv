# Makefile for generating the HTML documentation

# List of subdirectories to be processed.
SUBDIRS = en fr es ru

# Generated HTML files go here.
HTML_TOP = ../HTML


.PHONY: all
all: build-html

.PHONY: help
help:
	@echo "Targets:"
	@echo "********"
	@echo "all       : Build everything (same as build-html for now)."
	@echo "build-html: Build HTML documentation."
	@echo "clean-html: Purge the 'HTML' directory."
	@echo "distclean : Remove ALL generated files."

.PHONY: build-html
build-html: xsltproc.sh
	test -d $(HTML_TOP) || mkdir $(HTML_TOP)
	for dir in $(SUBDIRS); do\
		test -f $$dir/Makefile &&\
		(test -d $(HTML_TOP)/$$dir || mkdir $(HTML_TOP)/$$dir) &&\
		if $(MAKE) HTMLDIR=../$(HTML_TOP)/$$dir -C $$dir; then :; else exit 1; fi;\
	done

.PHONY: clean-html
clean-html:
	-rm -rf $(HTML_TOP)

.PHONY: distclean
distclean: clean-html
	for dir in $(SUBDIRS); do\
		test -f $$dir/Makefile &&\
		if $(MAKE) HTMLDIR=../$(HTML_TOP)/$$dir -C $$dir distclean ; then :; else exit 1; fi;\
	done
	-rm -f html.xsl xsltproc.sh xmllint.sh

xsltproc.sh: configure
	./configure
