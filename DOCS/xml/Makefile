# Makefile for generating the HTML documentation

#####[ Configuration ]##################################################

# The xsltproc program.
XSLTPROC = xsltproc

# The xmllint program.
XMLLINT = xmllint

# A colon separated list of catalog entry files.
# Without this properly set up, xmllint and xsltproc might be unable
# to find the DTDs for the system identifiers specified in the XML files.
# If the SGML_CATALOG_FILES environment variable is not set, list one
# or more catalogs here.
#
# on debian (potato?) systems, maybe others
#SGML_CATALOG_FILES ?= /etc/sgml/catalog
#
# on Mandrake (9.0?) systems, maybe others
SGML_CATALOG_FILES ?= /usr/share/sgml/docbook/xml-dtd-4.1.2/xmlcatalog
#SGML_CATALOG_FILES ?= /usr/share/apps/ksgmltools2/customization/en/catalog

# Full path of the "chunker" DocBook XSL stylesheet used to generate
# the HTML files.
#
# on debian (potato?)
#CHUNK_XSL = /usr/share/sgml/docbook/stylesheet/xsl/nwalsh/html/chunk.xsl
#
# on Mandrake (9.0?) systems, maybe others
CHUNK_XSL = /usr/share/sgml/docbook/yelp/docbook/html/chunk.xsl

# List of subdirectories to be processed.
SUBDIRS = en

# Here all generated html's go
HTML_TOP = ../HTML

#####[ End of configuration ]###########################################

export CHUNK_XSL SGML_CATALOG_FILES XMLLINT XSLTPROC

.PHONY: no-target
no-target:
	@echo "What to make?"
	@echo
	@echo "Targets"
	@echo "*******"
	@echo "all       : Build everything (same as build-html for now)."
	@echo "build-html: Build HTML documentation."
	@echo "clean-html: Purge the 'HTML' directory."
	@echo "distclean : Remove ALL generated files."

.PHONY: all
all: build-html

.PHONY: build-html
build-html:
	test -d $(HTML_TOP) || mkdir $(HTML_TOP)
	for d in $(SUBDIRS); do\
		test -f $$d/Makefile &&\
		(test -d $(HTML_TOP)/$$d || mkdir $(HTML_TOP)/$$d) &&\
		if $(MAKE) HTMLDIR=../$(HTML_TOP)/$$d -C $$d; then :; else exit 1; fi;\
	done

.PHONY: test
test:
	@if command -v $(XSLTPROC) >/dev/null; then :; else exit 1; fi
	@if command -v $(XMLLINT) >/dev/null; then :; else exit 1; fi
	@test -f $(CHUNK_XSL) || (echo "file not found: $(CHUNK_XSL)"; exit 1)
	@echo "All tests passed."

.PHONY: clean-html
clean-html:
	-rm -rf $(HTML_TOP)

.PHONY: distclean
distclean: clean-html
	-rm -f html.xsl
