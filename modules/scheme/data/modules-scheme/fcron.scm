;;;; FCron Scheme Module
(make-module "scheme-fcron" "Cron (fcron)"
	     '(("cron" "fcron") ("mount-critical"))
	     '("enable" . fcron-enable)
	     '("disable" . fcron-disable)
	     (cons "fcron-enable" (lambda () (pexec "/usr/sbin/fcron -b -c /etc/fcron/fcron.conf")))
	     (cons "fcron-disable" (lambda () (pexec "/bin/killall fcron"))))