#
# Makefile.inc for Makefiles in sub-directories.
#

export SGML_CATALOG_FILES

# Use customized html.xsl file if exists...
ifeq (html.xsl,$(wildcard html.xsl))
HTML_XSL := html.xsl
XSL_DEPS := $(HTML_XSL) ../html.xsl ../html-common.xsl
else
HTML_XSL := ../html.xsl
XSL_DEPS := $(HTML_XSL) ../html-common.xsl
endif

# Fall back to the default HTML stylesheet if not specified.
HTML_STYLESHEET ?= ../default.css

# This is the main target...
$(HTMLDIR)/index.html: documentation.xml $(XSL_DEPS)
	-rm -f $(HTMLDIR)/*
	$(XMLLINT) --noout --noent --postvalid --catalogs $<
	$(XSLTPROC) --catalogs -o $(HTMLDIR)/ $(HTML_XSL) $<
	cp $(HTML_STYLESHEET) $(HTMLDIR)/

../html.xsl:
	sh ../gen-html.xsl.sh $(CHUNK_XSL) > $@
