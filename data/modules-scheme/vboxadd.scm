;; vim:ft=scheme -*- mode: Scheme; -*-
;;
;; VirtualBox Additions virtual service
;;

(make-module 's-vboxadd "VirtualBox Additions (vboxadd)"
             (list 'provides "s-vboxadd" "vboxadd"))

(use-modules (ice-9 popen))

(define (read-pipe-line command)
  (let* ((port (open-input-pipe command))
         (pipe-line (read port)))
    (close-pipe port)
    pipe-line))

(define (running) (and (shell "lsmod | grep -q vboxadd[^_-]")
		       (shell "test -c /dev/vboxadd")))

(define-module-action 's-vboxadd 'enable
  (lambda (status)
    (call/cc
     ;; call/cc pwns, magnus is a luzer, neener neener neener :p
     (lambda (return)
       (if (not (running))
	   (begin (if (not (shell "rm -f /dev/vboxadd"))
		      (begin (feedback status "Failed to unlink /dev/vboxadd")
			     (return #f)))
		  (if (not (shell "modprobe vboxadd"))
		      (begin (feedback status "Failed to load vboxadd module")
			     (return #f)))
		  (shell "sleep .5")))
       (if (shell "test ! -c /dev/vboxadd")
	   (let ((maj (number->string (read-pipe-line "egrep '^([0-9]+) vboxadd$' /proc/devices | awk '{print $1}'")))
		 (min ""))
	     (if (not (zero? (string-length maj)))
		 (set! min "0")
		 (begin (set! min (number->string (read-pipe-line "egrep '^([0-9]+) vboxadd' /proc/misc | awk '{print $1}'")))
			(if (not (zero? (string-length min)))
			    (set! maj "10"))))
	     (if (zero? (string-length maj))
		 (begin (shell "rmmod vboxadd")
			(feedback status "Cannot locate the VirtualBox device")
			(return #f)))
	     (if (not (shell (string-append "mknod -m 0664 /dev/vboxadd c " maj " " min)))
		 (begin (shell "rmmod vboxadd")
			(feedback status
				  (string-append
				   "Cannot create device /dev/vboxadd with major "
				   maj " and minor " min))
			(return #f)))))
        (return #t))))) ; yes using (return) at the end is unnecessary. consistency ftw.

(define-module-action 's-vboxadd 'disable
  (lambda (status)
     (call/cc
     (lambda (return)
       (if (running)
	   (begin (if (not (shell "rmmod vboxadd"))
		      (begin (feedback status "Failed to unload vboxadd module")
			     (return #f)))
		  (if (not (shell "rm -f /dev/vboxadd"))
		      (begin (feedback status "Failed to unlink /dev/vboxadd")
			     (return #f)))))
       (return #t)))))
