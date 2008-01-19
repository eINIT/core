;;
;; FUSE virtual service
;;
;; blame@: Leonardo Valeri Manera

(make-module 's-fuse "Filesystem in USErspace (fuse)"
             (list 'provides "s-fuse" "fuse"))

(define-module-action 's-fuse 'enable
  (lambda (status)
    (call/cc
     (lambda (return)
       (if (not (shell "grep -qw fuse /proc/filesystems"))
	   (if (not (shell "modprobe fuse"))
	       (begin (feedback status "Error loading FUSE module")
		      (return #f))
	       (feedback status "Loaded FUSE module" )))
       (if (and (shell "grep -qw fusectl /proc/filesystems")
		(not (shell "grep -qw /sys/fs/fuse/connections /proc/mounts")))
	   (if (not (shell "mount -t fusectl none /sys/fs/fuse/connections"))
	       (begin (feedback status "Error mounting FUSE control filesystem")
		      (return #f))
	       (feedback status "Mounted FUSE control filsystem")))
       (return #t)))))

(define-module-action 's-fuse 'disable
  (lambda (status)
    (if (shell "grep -qw /sys/fs/fuse/connections /proc/mounts")
	(if (shell "umount /sys/fs/fuse/connections")
	    (begin (feedback status "Unmounted FUSE Control filesystem")
		   #t)
	    (begin (feedback status "Error unmounting FUSE control filesystem")
		   #f))
	#t)))
