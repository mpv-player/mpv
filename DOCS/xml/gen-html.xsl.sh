#
# Helper script to generate html.xsl.
#

if test $# -ne 1; then
	echo "Usage: $0 <path to chunk.xsl>"
	exit 1
fi

if test -f "$1"; then :; else
	echo "$0: file not found: \"$1\""
	exit 1
fi

cat << EOF
<?xml version="1.0" encoding="ISO-8859-1"?>
<!-- ***************************************************
     This file is generated automatically.  DO NOT EDIT.
     *************************************************** -->
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

  <xsl:import href="$1"/>
  <xsl:include href="html-common.xsl"/>

</xsl:stylesheet>
EOF
