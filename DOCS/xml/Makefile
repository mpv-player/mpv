# Makefile for generating the HTML documentation

# List of subdirectories to be processed.
SUBDIRS = en fr es ru pl

# Generated HTML files go here.
HTML_TOP = ../HTML

# Here go unchunked files (all DOCS in single HTML file)
HTML_UNCHUNKED = ../HTML-single

.PHONY: all
all: build-html build-html-chunked

.PHONY: help
help:
	@echo "Targets:"
	@echo "********"
	@echo "all               : Build everything"
	@echo "                    (same as build-html and build-html-chunked for now)."
	@echo "build-html        : Build HTML documentation (single file)."
	@echo "build-html-chunked: Build HTML documentation (multiple files)."
	@echo "clean-html        : Purge the 'HTML-single' directory."
	@echo "clean-html-chunked: Purge the 'HTML' directory."
	@echo "distclean         : Remove ALL generated files."

.PHONY: build-html-chunked
build-html-chunked: xsltproc.sh
	test -d $(HTML_TOP) || mkdir $(HTML_TOP)
	for dir in $(SUBDIRS); do\
		test -f $$dir/Makefile &&\
		(test -d $(HTML_TOP)/$$dir || mkdir $(HTML_TOP)/$$dir) &&\
		if $(MAKE) HTMLDIR=../$(HTML_TOP)/$$dir -C $$dir html-chunked; then :; else exit 1; fi;\
	done

.PHONY: build-html
build-html: xsltproc.sh
	test -d $(HTML_TOP) || mkdir $(HTML_TOP)
	test -d $(HTML_UNCHUNKED) || mkdir $(HTML_UNCHUNKED)
	for dir in $(SUBDIRS); do\
		test -f $$dir/Makefile &&\
		if $(MAKE) HTMLFILE=../$(HTML_UNCHUNKED)/mplayer_docs_$$dir.html -C $$dir html; then :; else exit 1; fi;\
	done

.PHONY: clean-html-chunked
clean-html:
	-rm -rf $(HTML_TOP)

.PHONY: clean-html
clean-html:
	-rm -rf $(HTML_UNCHUNKED)

.PHONY: distclean
distclean: clean-html clean-html-chunked
	for dir in $(SUBDIRS); do\
		test -f $$dir/Makefile &&\
		if $(MAKE) HTMLDIR=../$(HTML_TOP)/$$dir -C $$dir distclean ; then :; else exit 1; fi;\
	done
	-rm -f html.xsl html-chunk.xsl xsltproc.sh xmllint.sh

xsltproc.sh: configure
	./configure

clean: clean-html
