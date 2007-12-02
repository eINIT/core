;;;;;;;
;;; aticonfig wrapper
;;;;;;;

(letrec
  ((check-aticonfig (lambda ()
     (let ((a (assq "b" (get-configuration 'configuration-services-aticonfig-enable))))
          (if a (string=? a "yes") #f)))))

  (if (check-aticonfig)
      (begin
        (make-module 's-aticonfig "ATIConfig Wrapper")

        (letrec
          ((get-aticonfig-options (lambda ()
            (let ((a (assq "s" (get-configuration 'configuration-services-aticonfig-options)))
                 (if a (cdr a) #f))))))

          (event-listen 'core/mode-switching
            (lambda (event)
              (shell (string-append "aticonfig " (get-aticonfig-options)))))))))
