include ../Makefile.inc

# Set if you are using your own HTML stylesheet...
#HTML_STYLESHEET = mystyle.css

# Change to yes to enable symlinking missing files to English master versions
USE_SYMLINKS = yes

# Dependency information.
$(HTMLDIR)/index.html: $(wildcard *.xml) html-common.xsl
$(HTMLFILE): $(wildcard *.xml) html-common.xsl
