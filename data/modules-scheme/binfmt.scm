;;
;; Binfmt_MISC registering virtual service
;;
;; blame@: Leonardo Valeri Manera - lvalerimanera[AT]<NOSPAM>gmail.com

(make-module 's-binfmt "Register Binfmt_misc Entries (binfmt)"
             (list 'provides "s-binfmt" "binfmt"))

(define-module-action 's-binfmt 'enable
  (lambda (status)
    (call/cc
     (lambda (return)
       ;; try loading the module
       (shell "modprobe -q binfmt_misc")
       (if (shell "mount | grep -c binfmt_misc")
	   (if (not (shell "mount -t binfmt_misc binfmt_misc /proc/sys/fs/binfmt_misc > /dev/null 2>/dev/null"))
	       (begin (feedback status "You need in-kernel or module MISC binary support.")
		      (return #f))))
       (do ((types (get-configuration 'configuration-services-binfmt) (cdr types)))
	   ((null? types) #t)
	 (let ((type (caar types))
	       (register (cdar types))
	       (executable (caddr (cddddr (string-split (cdar types) #\:)))))
	   (feedback status (string-append "Registering " (string-upcase type)
					   " binaries with " executable))
	   (shell (string-append "echo '" register "' > /proc/sys/fs/binfmt_misc/register"))))))))

(define-module-action 's-binfmt 'disable
  (lambda (status) (if #t #t)))