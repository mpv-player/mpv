# Makefile for generating the HTML documentation

# List of subdirectories to be processed.
SUBDIRS = en fr es ru pl

# Generated chunked HTML files go here.
HTML_CHUNKED = ../HTML

# Generated single HTML files go here.
HTML_SINGLE = ../HTML-single

.PHONY: all
all: build-html-chunked build-html-single

.PHONY: help
help:
	@echo "Targets:"
	@echo "********"
	@echo "all               : Build everything (default)."
	@echo "build-html-single : Build HTML documentation (single file)."
	@echo "build-html-chunked: Build HTML documentation (multiple files)."
	@echo "clean-html-single : Purge the 'HTML-single' directory."
	@echo "clean-html-chunked: Purge the 'HTML' directory."
	@echo "distclean         : Remove ALL generated files."

.PHONY: build-html-chunked
build-html-chunked: xsltproc.sh
	test -d $(HTML_CHUNKED) || mkdir $(HTML_CHUNKED)
	for dir in $(SUBDIRS); do\
		test -f $$dir/Makefile &&\
		(test -d $(HTML_CHUNKED)/$$dir || mkdir $(HTML_CHUNKED)/$$dir) &&\
		if $(MAKE) HTMLDIR=../$(HTML_CHUNKED)/$$dir -C $$dir html-chunked; then :; else exit 1; fi;\
	done

.PHONY: build-html-single
build-html-single: xsltproc.sh
	test -d $(HTML_SINGLE) || mkdir $(HTML_SINGLE)
	for dir in $(SUBDIRS); do\
		test -f $$dir/Makefile &&\
		(test -d $(HTML_SINGLE)/$$dir || mkdir $(HTML_SINGLE)/$$dir) &&\
		if $(MAKE) HTMLFILE=../$(HTML_SINGLE)/$$dir/MPlayer.html -C $$dir html; then :; else exit 1; fi;\
	done

.PHONY: clean-html-chunked
clean-html-chunked:
	-rm -rf $(HTML_CHUNKED)

.PHONY: clean-html-single
clean-html-single:
	-rm -rf $(HTML_SINGLE)

.PHONY: distclean
distclean: clean-html-chunked clean-html-single
	for dir in $(SUBDIRS); do\
		test -f $$dir/Makefile &&\
		if $(MAKE) HTMLDIR=../$(HTML_CHUNKED)/$$dir -C $$dir distclean ; then :; else exit 1; fi;\
	done
	-rm -f html.xsl html-chunk.xsl xsltproc.sh xmllint.sh

xsltproc.sh: configure
	./configure

clean: clean-html-chunked clean-html-single
