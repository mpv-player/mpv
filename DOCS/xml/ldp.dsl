<!DOCTYPE style-sheet PUBLIC
          "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY % html "IGNORE">
<![%html;[
<!ENTITY % print "IGNORE">
<!ENTITY docbook.dsl PUBLIC
         "-//Norman Walsh//DOCUMENT DocBook HTML Stylesheet//EN"
         CDATA dsssl>
]]>
<!ENTITY % print "INCLUDE">
<![%print;[
<!ENTITY docbook.dsl PUBLIC
         "-//Norman Walsh//DOCUMENT DocBook Print Stylesheet//EN"
         CDATA dsssl>
]]>
]>

<style-sheet>

;; ------------------------------------------------------------------------
;; ldp.dsl - LDP Customized DSSSL Stylesheet
;; v1.11, 2003-02-03
;; Copyright (C) 2000-2003
;;
;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program; if not, write to the Free Software
;; Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
;; ------------------------------------------------------------------------

<style-specification id="print" use="docbook">
<style-specification-body> 

;; customize the print stylesheet

(declare-characteristic preserve-sdata?
  ;; this is necessary because right now jadetex does not understand
  ;; symbolic entities, whereas things work well with numeric entities.
  "UNREGISTERED::James Clark//Characteristic::preserve-sdata?"
  #f)

(define %generate-article-toc%
  ;; Should a Table of Contents be produced for Articles?
  #t)

(define (toc-depth nd)
  4)

(define %generate-article-titlepage-on-separate-page%
  ;; Should the article title page be on a separate page?
  #t)

(define %section-autolabel%
  ;; Are sections enumerated?
  #t)

(define %footnote-ulinks%
  ;; Generate footnotes for ULinks?
  #f)

(define %bop-footnotes%
  ;; Make "bottom-of-page" footnotes?
  #f)

(define %body-start-indent%
  ;; Default indent of body text
  0pi)

(define %para-indent-firstpara%
  ;; First line start-indent for the first paragraph
  0pt)

(define %para-indent%
  ;; First line start-indent for paragraphs (other than the first)
  0pt)

(define %block-start-indent%
  ;; Extra start-indent for block-elements
  0pt)

(define formal-object-float
  ;; Do formal objects float?
  #t)

(define %hyphenation%
  ;; Allow automatic hyphenation?
  #t)

(define %admon-graphics%
  ;; Use graphics in admonitions?
  #f)

(define %default-quadding%
  ;; Full justification.
  'justify)

(define (book-titlepage-verso-elements)
  ;;added publisher, releaseinfo to the default list
  (list (normalize "title")
        (normalize "subtitle")
        (normalize "corpauthor")
        (normalize "authorgroup")
        (normalize "author")
        (normalize "publisher")
        (normalize "releaseinfo")
        (normalize "editor")
        (normalize "edition")
        (normalize "pubdate")
        (normalize "copyright")
        (normalize "abstract")
        (normalize "legalnotice")
        (normalize "revhistory")))

</style-specification-body>
</style-specification>


<!--
;; customize the html stylesheet; parts borrowed from 
;; Cygnus at http://sourceware.cygnus.com/ (cygnus-both.dsl)
-->

<style-specification id="html" use="docbook">
<style-specification-body> 

(declare-characteristic preserve-sdata?
  ;; this is necessary because right now jadetex does not understand
  ;; symbolic entities, whereas things work well with numeric entities.
  "UNREGISTERED::James Clark//Characteristic::preserve-sdata?"
  #f)

(declare-flow-object-class element
  ;; for redhat
  "UNREGISTERED::James Clark//Flow Object Class::element")

(define %generate-legalnotice-link%
  ;; put the legal notice in a separate file
  #t)

(define %admon-graphics-path%
  ;; use graphics in admonitions, set their
  "../images/")

(define %admon-graphics%
  #t)

(define %funcsynopsis-decoration%
  ;; make funcsynopsis look pretty
  #t)

(define %html-ext%
  ;; when producing HTML files, use this extension
  ".html")

(define %generate-book-toc%
  ;; Should a Table of Contents be produced for books?
  #t)

(define %generate-article-toc% 
  ;; Should a Table of Contents be produced for articles?
  #t)

(define %generate-part-toc%
  ;; Should a Table of Contents be produced for parts?
  #t)

(define %generate-book-titlepage%
  ;; produce a title page for books
  #t)

(define %generate-article-titlepage%
  ;; produce a title page for articles
  #t)

(define (chunk-skip-first-element-list)
  ;; forces the Table of Contents on separate page
  '())

(define (list-element-list)
  ;; fixes bug in Table of Contents generation
  '())

(define %root-filename%
  ;; The filename of the root HTML document (e.g, "index").
  "index")

(define %shade-verbatim%
  ;; verbatim sections will be shaded if t(rue)
  #t)

(define %use-id-as-filename%
  ;; Use ID attributes as name for component HTML files?
  #t)

(define %graphic-extensions%
  ;; graphic extensions allowed
  '("gif" "png" "jpg" "jpeg" "tif" "tiff" "eps" "epsf" ))

(define %graphic-default-extension% 
  "gif")

(define %section-autolabel%
  ;; For enumerated sections (1.1, 1.1.1, 1.2, etc.)
  #t)

(define (toc-depth nd)
  ;; more depth (2 levels) to toc; instead of flat hierarchy
  2)

(element emphasis
  ;; make role=strong equate to bold for emphasis tag
  (if (equal? (attribute-string "role") "strong")
     (make element gi: "STRONG" (process-children))
     (make element gi: "EM" (process-children))))

(define (book-titlepage-recto-elements)
  ;; elements on a book's titlepage
  (list (normalize "title")
        (normalize "subtitle")
        (normalize "graphic")
        (normalize "mediaobject")
        (normalize "corpauthor")
        (normalize "authorgroup")
        (normalize "author")
        (normalize "othercredit")
        (normalize "edition")
        (normalize "releaseinfo")
        (normalize "publisher")
        (normalize "editor")
        (normalize "copyright")
        (normalize "pubdate")
        (normalize "revhistory")
        (normalize "abstract")
        (normalize "legalnotice")))

(define (article-titlepage-recto-elements)
  ;; elements on an article's titlepage
  (list (normalize "title")
        (normalize "subtitle")
        (normalize "authorgroup")
        (normalize "author")
        (normalize "othercredit")
        (normalize "releaseinfo")
        (normalize "copyright")
        (normalize "pubdate")
        (normalize "revhistory")
        (normalize "abstract")
        (normalize "legalnotice")))

(define (process-contrib #!optional (sosofo (process-children)))
  ;; print out with othercredit information; for translators, etc.
  (make sequence
    (make element gi: "SPAN"
          attributes: (list (list "CLASS" (gi)))
          (process-children))))

(define (process-othercredit #!optional (sosofo (process-children)))
  ;; print out othercredit information; for translators, etc.
  (let ((author-name  (author-string))
        (author-contrib (select-elements (children (current-node))
                                          (normalize "contrib"))))
    (make element gi: "P"
         attributes: (list (list "CLASS" (gi)))
         (make element gi: "B"
              (literal author-name)
              (literal " - "))
         (process-node-list author-contrib))))

(mode article-titlepage-recto-mode
  (element contrib (process-contrib))
  (element othercredit (process-othercredit))
)

(mode book-titlepage-recto-mode
  (element contrib (process-contrib))
  (element othercredit (process-othercredit))
)

(define (article-title nd)
  (let* ((artchild  (children nd))
         (artheader (select-elements artchild (normalize "artheader")))
         (artinfo   (select-elements artchild (normalize "articleinfo")))
         (ahdr (if (node-list-empty? artheader)
                   artinfo
                   artheader))
         (ahtitles  (select-elements (children ahdr)
                                     (normalize "title")))
         (artitles  (select-elements artchild (normalize "title")))
         (titles    (if (node-list-empty? artitles)
                        ahtitles
                        artitles)))
    (if (node-list-empty? titles)
        ""
        (node-list-first titles))))

(mode subtitle-mode
  ;; do not print subtitle on subsequent pages
  (element subtitle (empty-sosofo)))

;; Redefinition of $verbatim-display$
;; Origin: dbverb.dsl
;; Different foreground and background colors for verbatim elements
;; Author: Philippe Martin (feloy@free.fr) 2001-04-07

(define ($verbatim-display$ indent line-numbers?)
  (let ((verbatim-element (gi))
        (content (make element gi: "PRE"
                       attributes: (list
                                    (list "CLASS" (gi)))
                       (if (or indent line-numbers?)
                           ($verbatim-line-by-line$ indent line-numbers?)
                           (process-children)))))
    (if %shade-verbatim%
        (make element gi: "TABLE"
              attributes: (shade-verbatim-attr-element verbatim-element)
              (make element gi: "TR"
                    (make element gi: "TD"
                          (make element gi: "FONT" 
                                attributes: (list
                                             (list "COLOR" (car (shade-verbatim-element-colors
                                                                 verbatim-element))))
                                content))))
        content)))

;;
;; Customize this function
;; to change the foreground and background colors
;; of the different verbatim elements
;; Return (list "foreground color" "background color")
;;
(define (shade-verbatim-element-colors element)
  (case element
    (("SYNOPSIS") (list "#000000" "#6495ED"))
    ;; ...
    ;; Add your verbatim elements here
    ;; ...
    (else (list "#000000" "#E0E0E0"))))

(define (shade-verbatim-attr-element element)
  (list
   (list "BORDER" 
	(cond
		((equal? element (normalize "SCREEN")) "1")
		(else "0")))
   (list "BGCOLOR" (car (cdr (shade-verbatim-element-colors element))))
   (list "WIDTH" ($table-width$))))

;; End of $verbatim-display$ redefinition

</style-specification-body>
</style-specification>

<external-specification id="docbook" document="docbook.dsl">

</style-sheet>

