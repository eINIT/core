;;;;;;;
;;; feedback module
;;; aural, using festival
;;;;;;;

(define (festival-vocalise str)
 (shell (string-append "echo '(SayText \"" str "\")'|festival; true")))

(let ((a (get-configuration 'configuration-feedback-aural-festival-active)))
 (if (and (list? a) (pair? (car a)) (string? (cdr (car a))) (string=? (cdr (car a)) "yes"))
  (begin

   (make-module 'scheme-feedback-aural-festival "Feedback :: Aural/Festival")

   (event-listen 'core/mode-switching
    (lambda (event)
     (festival-vocalise (string-append "switching to mode " (event-string event) "..." ))))

   (event-listen 'core/mode-switch-done
    (lambda (event)
     (festival-vocalise (string-append "switch to mode " (event-string event) " complete.")))))))
