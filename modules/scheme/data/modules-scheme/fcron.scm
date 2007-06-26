;;;; FCron Scheme Module
(define (fcron-enable)
  (pexec "/usr/sbin/fcron -b -c /etc/fcron/fcron.conf"))

(define (fcron-disable)
  (pexec /bin/killall fcron"))

; or (this one should be equivalent to the definition above):
; (define fcron-disable
;   (lambda ()
;      (pexec /bin/killall fcron")))

(make-module "scheme-fcron" "Cron (fcron)"
  '(("cron" "fcron") ("mount-critical"))
  '(enable . fcron-enable)
  '(disable . fcron-disable))