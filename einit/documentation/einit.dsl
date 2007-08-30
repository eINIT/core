<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY html-ss
  PUBLIC "-//Norman Walsh//DOCUMENT DocBook HTML Stylesheet//EN" CDATA dsssl>
<!ENTITY print-ss
  PUBLIC "-//Norman Walsh//DOCUMENT DocBook Print Stylesheet//EN" CDATA dsssl>
]>

<style-sheet>
 <style-specification id="print" use="print-stylesheet">
  <style-specification-body> 

;; customize the print stylesheet
  </style-specification-body>
 </style-specification>
 <style-specification id="html" use="html-stylesheet">
  <style-specification-body> 

   (define %html-ext% ".html")
   (define %root-filename% "index")
   (define %use-id-as-filename% #t)

   (define %generate-legalnotice-link% #t)

<!-- some portions of this were inspired by the freebsd stylesheet, see
 http://www.freebsd.org/cgi/cvsweb.cgi/doc/share/sgml/freebsd.dsl?rev=1.91&content-type=text/x-cvsweb-markup -->
   (define ($footer$)
    (make sequence
     (make empty-element gi: "hr")
     (literal "hosting provided by:")
     (make empty-element gi: "br")
     (make element gi: "a"
       attributes: (list (list "href" "http://developer.berlios.de"))
     (make empty-element gi: "img"
       attributes: (list 
                     (list "src" "http://developer.berlios.de/bslogo.php?group_id=8423")
                     (list "width" "124")
                     (list "height" "32")
                     (list "border" "0")
                     (list "alt" "BerliOS Logo"))))))

   (define ($html-body-end$)
    (if (equal? $footer$ (normalize ""))
     (empty-sosofo)
     (make sequence
      (if nochunks
       (make empty-element gi: "hr")
       (empty-sosofo))
       ($footer$))))

  </style-specification-body>
 </style-specification>
 <external-specification id="print-stylesheet" document="print-ss">
 <external-specification id="html-stylesheet" document="html-ss">
</style-sheet>
