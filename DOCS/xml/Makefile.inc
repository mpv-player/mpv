#
# Makefile.inc for Makefiles in subdirectories.
#

# Use customized html.xsl file if it exists...
ifeq (html.xsl,$(wildcard html.xsl))
HTML_XSL := html.xsl
XSL_DEPS := $(HTML_XSL) ../html.xsl ../html-common.xsl
else
HTML_XSL := ../html.xsl
XSL_DEPS := $(HTML_XSL) ../html-common.xsl
endif

# Fall back to the default HTML stylesheet if none is specified.
HTML_STYLESHEET ?= ../default.css

# This is the main target...
$(HTMLDIR)/index.html: documentation.xml $(XSL_DEPS)
	-rm -f $(HTMLDIR)/*
	../xmllint.sh $<
	cp $(HTML_STYLESHEET) $(HTMLDIR)/
	../xsltproc.sh $(HTMLDIR)/ $(HTML_XSL) $<

../html.xsl:
	cd .. && sh configure
