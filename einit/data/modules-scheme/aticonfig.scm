;;;;;;;
;;; aticonfig wrapper
;;;;;;;

(letrec 
 ((check-aticonfig (lambda (node)
   (if (null? node)
       #f
       (if (and
            (string=? (car (car node)) "b")
            (string=? (cdr (car node)) "yes"))
           #t
           (check-aticonfig (cdr node)))))))

 (if (check-aticonfig (get-configuration 'configuration-services-aticonfig-enable))
     (begin
       (make-module 's-aticonfig "ATIConfig Wrapper")

       (letrec
         ((get-aticonfig-options (lambda (node)
           (if (null? node)
               ""
               (if (string=? (car (car node)) "s")
                   (cdr (car node))
                   (get-aticonfig-options (cdr node)))))))

         (event-listen 'any
           (lambda (event)
             (shell (string-append "aticonfig " (get-aticonfig-options (get-configuration 'configuration-services-aticonfig-options))))))))))
