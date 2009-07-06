<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">

<xsl:output method="html" indent="no"
            doctype-system="http://www.w3.org/TR/html4/loose.dtd"
            doctype-public="-//W3C//DTD HTML 4.01 Transitional//EN"/>

<!-- **************
     Set parameters
     ************** -->

  <xsl:param name="chunker.output.encoding" select="'utf-8'"/>
  <xsl:output encoding="utf-8"/>

  <xsl:param name="toc.section.depth" select="'4'"/>

  <xsl:param name="html.stylesheet" select="'default.css'"/>
  <xsl:param name="html.stylesheet.type" select="'text/css'"/>

  <xsl:param name="html.cleanup" select="'1'"/>
  <xsl:param name="make.valid.html" select="'1'"/>
  <xsl:param name="make.single.year.ranges" select="'1'"/>
  <xsl:param name="make.year.ranges" select="'1'"/>

  <!-- Use ID value for generated filenames -->
  <xsl:param name="use.id.as.filename" select="'1'"/>

  <!-- Depth to which sections are chunked -->
  <xsl:param name="chunk.section.depth" select="'1'"/>

  <!-- Create a chunk for the 1st top-level section too  -->
  <xsl:param name="chunk.first.sections" select="'1'"/>

  <xsl:param name="admon.graphics" select="'0'"/>
  <xsl:param name="navig.graphics" select="'0'"/>
  <xsl:param name="navig.showtitles" select="'1'"/>

  <!-- Generate more links for Site Navigation Bar -->
  <xsl:param name="html.extra.head.links" select="1"/>

  <!-- Label sections too (eg. 2.1, 2.1.1) -->
  <xsl:param name="section.autolabel" select="'1'"/>
  <xsl:param name="section.label.includes.component.label" select="'1'"/>

  <!-- Use informal procedures; no need to number them -->
  <xsl:param name="formal.procedures" select="'0'"/>

  <xsl:param name="generate.toc">
    appendix  toc
    article   toc
    book      toc
    chapter   toc
    part      toc
    preface   toc
    qandadiv  toc
    qandaset  toc
    reference toc
    section   toc
    set       toc
  </xsl:param>

<!-- *********
     Templates
     ********* -->

  <xsl:template match="application">
    <span class="application"><xsl:apply-templates/></span>
  </xsl:template>

  <xsl:template match="option">
    <tt class="option"><xsl:apply-templates/></tt>
  </xsl:template>

  <xsl:template match="filename">
    <tt class="filename"><xsl:apply-templates/></tt>
  </xsl:template>

  <xsl:template match="keycap">
    <span class="keycap"><b><xsl:apply-templates/></b></span>
  </xsl:template>

  <xsl:template match="guimenu">
    <span class="guimenu"><xsl:apply-templates/></span>
  </xsl:template>

  <xsl:template match="guisubmenu">
    <span class="guisubmenu"><xsl:apply-templates/></span>
  </xsl:template>

  <xsl:template match="guimenuitem">
    <span class="guimenuitem"><xsl:apply-templates/></span>
  </xsl:template>

</xsl:stylesheet>
