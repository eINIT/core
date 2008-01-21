;;; vim:ft=scheme -*- mode: Scheme; -*-
;;;
;;; NFS Server virtual service module
;;;
;;; blame@: Leonardo Valeri Manera: lvalerimanera[AT]<NOSPAM>gmail.com

;;; Configuration.
(define exportfs-timeout (cdar (get-configuration 'configuration-services-exportfs)))
(define mountd-options (cdar (get-configuration 'configuration-services-mountd)))
(define nfsd-options (cdar (get-configuration 'configuration-services-nfsd)))
(define (nfs4?)
  (string=? (cdar (get-configuration 'configuration-services-nfs4))
	    "yes"))
(define (gssd?)
  (if (shell "test -e /etc/exports")
      (if (= 0 (system "awk '!/^[[:space:]]*#/ && $2 ~ /sec=/ { exit 0 } END { exit 1 }' /etc/exports"))
	  #t
	  #f)
      #f))
;;; Require-list generator.
(define (require)
  (let ((req-list '('requires "rpc.statd")))
    (if (nfs4?)
	(set! req-list (append req-list '("rpc.idmapd"))))
    (if (gssd?)
	(set! req-list (sppend req-list '("rpc.svcgsdd"))))
    req-list))

;;; Utility function to read a single line from a pipe.
;;; Here until it goes into the utility function library.
(use-modules (ice-9 popen))
(define (read-pipe-line command)
  (let* ((port (open-input-pipe command))
         (pipe-line (read port)))
    (close-pipe port)
    pipe-line))

;;; Magnus, your (shell) keeps truncating output, so I'm re-implementing it with Guile (system)
(define (shell command)
  (if (= 0 (system command))
      #t #f))

;;; Looks like this works...
(make-module 's-nfs "NFS Server (nfs)"
             (list 'provides "s-nfs" "nfs")
	     (require))

;;; Returnless Thunk to create the necessary paths
(define (make-nfs-directories)
  (do ((dirs '("/var/lib/nfs/rpc_pipefs" "/var/lib/nfs/v4recovery" "/var/lib/nfs/v4root") (cdr dirs)))
      ((null? dirs))
    (if (not (shell (string-append "test -d " (car dirs))))
	(shell (string-append "mkdir -p " (car dirs))))))

;;; exportfs handlers.
;;; This horrid little piece of sh code is necessary to kill exportfs on the proper timeout
;;; AND not freeze the whole module while waiting for the shell to finish.
;;; If you want to implement it in scheme, be my guest, I really can't be bothered :p
(define (wait-for-exportfs command)
  (if (= 0 (read-pipe-line (string-append command " &"
					  "\npid=$!"
					  "\n( sleep " exportfs-timeout "; kill -9 $pid 2>/dev/null ) &"
					  "\nwait $pid"
					  "\nret=$?"
					  "\necho $ret"
					  "\nexit")))
      #t
      #f))
(define (nfs-unexport status)
  (if (wait-for-exportfs "exportfs -ua")
      (begin (feedback status "Unexported NFS Directories")
	     #t)
      (begin (feedback status "Error Unexporting NFS Directories")
	     #f)))
(define (nfs-export status)
  (if (wait-for-exportfs "exportfs -r")
      (begin (feedback status "Exported NFS Directories")
	     #t)
      (begin (feedback status "Error Exporting NFS directories")
	     #f)))

;;; nfsd fs handler; if mountable and mounted mount, if not mounted at end fail.
(define (mount-nfsd status)
  (if (shell "test -e /proc/modules")
      (shell "modprobe -q nfsd"))
  (if (and (shell "grep -qs nfsd /proc/filesystems")
	   (not (shell "grep -qs 'nfsd /proc/fs/nfs' /proc/mounts")))
      (if (shell "mount -t nfsd -o nodev,noexec,nosuid nfsd /proc/fs/nfs")
	  (feedback status "Mounted nfsd filesystem in /proc")))
  (if (shell "grep 'nfsd /proc/fs/nfs' /proc/mounts")
      #t
      (begin (feedback status "nfsd filesystem not available, did you compile the nfs server kernel module?")
	     #f)))

;;; Application starting wrapper.
(define (start-it status name app options)
  (if (shell (string-append app " " options))
      (begin (feedback status (string-append "Started " name)) #t)
      (begin (feedback status (string-append "Error starting " name)) #f)))

;;; Utility deamon-starter and stopper to decrease code duplication accross events.
;;; Using nested (if)s because (and)s do not guarantee order of execution.
(define (start-daemons status)
  (if (not (start-it status "NFS Mount Deamon (rpc.mountd)" "rpc.mountd" mountd-options))
      #f
      (if (not (start-it status "NFS Daemon (rpc.nfsd)" "rpc.nfsd" nfsd-options))
	  #f
	  (if (not (start-it status "NSM Reboot Notifier (sm-notify)" "sm-notify" "-f"))
	      #f
	      #t))))
(define (stop-daemons status)
  (if (not (shell "killall -q -15 rpc.mountd"))
      (begin (feedback status "Failed to Stop NFS Mount Daemon (rpc.mountd)")
	     #f)
      (begin (feedback status "Stopped NFS Mount Daemon (rpc.mountd)")
	     (if (not (shell "killall -q -2 -u root nfsd"))
		 (begin (feedback status "Failed to Stop NFS Daemon (rpc.nfsd)")
			#f)
		 (begin (feedback status "Stopped NFS Daemon (rpc.nfsd)")
			#t)))))

;;; All that jazz above makes the actual action code readable.
;;; I'll let you imagine what they would be like without...

;;; If nfsd is not mounted rpc.nfsd will hang, so fail before that happens.
;;; In the gentoo script, exportfs failure does not fail the script...
;;; I'm keeping that behaviour as there is probably a good reason for it.
(define-module-action 's-nfs 'enable
  (lambda (status)
    (if (mount-nfsd status)
	(begin (make-nfs-directories)
	       (if (shell "grep -qs '^[[:space:]]*/' /etc/exports")
		   (nfs-export status))
	       (start-daemons status))
	#f)))

;;; Logic here might look iffy to some. Again, I'm duplicating Gentoo
;;; behaviour. It makes sense in a way to unexport even if we fail at
;;; killing the modules, I think.
(define-module-action 's-nfs 'disable
  (lambda (status)
    (let ((ret #t))
      (if (not (stop-daemons status))
	  (set! ret #f))
      (if (not (nfs-unexport status))
	  (set! ret #f))
      ret)))

;;; This is a little more elegant than Gentoo's solution re: not killing exportfs
;;; on restarts. I decided to make the whole thing fail if we cannot kill the daemons,
;;; but if I'm wrong, let me know why and I'll change it. Personally I think it's better
;;; this way, but if you believe leaving braindead daemons around and forking new ones is
;;; a better solution ... well, I'm open to suggestions.
(define-module-action 's-nfs 'reset
  (lambda (status)
    (if (stop-daemons status)
	(begin (if (mount-nfsd status)
		   (begin (make-nfs-directories)
			  (if (shell "grep -qs '^[[:space:]]*/' /etc/exports")
			      (nfs-export status))
			  (start-daemons status))
		   #f))
	#f)))

;;; This speaks for itself really.
(define-module-action 's-nfs 'reload
  (lambda (status)
    (feedback status "Reloading /etc/exports")
    (nfs-export status)))
