;; vim:ft=scheme -*- mode: Scheme; -*-
;;
;; VirtualBox Additions shared folder support virtual service
;;

(make-module 's-vboxvfs "VirtualBox Additions shared folder support (vboxvfs)"
             (list 'provides "s-vboxbfs" "vboxvfs")
	     (list 'requires "vboxadd"))

(define (running) (shell "lsmod | grep -q vboxvfs[^_-]"))

(define-module-action 's-vboxvfs 'enable
  (lambda (status)
    (call/cc
     (lambda (return)
       (if (not (running))
	   (if (not (shell "modprobe vboxvfs"))
	       (begin (if (shell "dmesg | grep 'vboxConnect failed'")
			  (begin (feedback status "Failed to start VirtualBox Additions shared folder support")
				 (feedback status "You may be trying to run Guest Additions from binary release")
				 (feedback status "of VirtualBox in the Open Source Edition.")
				 (return #f)))
		      (feedback status "modprobe vboxvfs failed")
		      (return #f))))
       (return #t)))))

(define-module-action 's-vboxvfs 'disable
  (lambda (status)
    (call/cc
     (lambda (return)
       (if (running)
	   (if (not (shell "rmmod vboxadd")
		    (begin (feedback status "Failed to unload vboxadd module")
			   (return #f)))))
       (return #t)))))
