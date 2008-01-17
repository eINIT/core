;; vim:ft=scheme -*- mode: Scheme; -*-
;;
;; rpcsec_gss daemon virtual service
;;

(make-module 's-rpc.gssd "RPCSec_GSS daemon (rpc.gssd)"
             (list 'provides "s-rpc.gssd" "rpc.gssd")
	     (list 'requires "portmap"))

;; configuration
(define gssd-options (get-configuration 'configuration-services-gssd-options))
(define scvgssd-options (get-configuration 'configuration-services-scvgssd-options))

(define (mount-pipefs)
  (if (and (shell "grep -qs rpc_pipefs /proc/filesystems")
	   (not (shell "grep -qs 'rpc_pipefs /var/lib/nfs/rpc_pipefs' /proc/mounts")))
      (begin (shell "mkdir -p /var/lib/nfs/rpc_pipefs")
	     (if (shell "mount -t rpc_pipefs rpc_pipefs /var/lib/nfs/rpc_pipefs")
		 (feedback status "Mounted RPC pipefs")
		 (feedback status "Failed to mount RPC pipefs")))))

(define-module-action 's-rpc.gssd 'enable
  (lambda (status)
    (mount-pipefs)
    (if (and (shell (string-append "rpc.gssd " gssd-options))
	     (shell (string-append "rpc.scvgssd " svcgssd-options)))
	#t
	#f)))

(define-module-action 's-rpc.gssd 'disable
  (lambda (status)
    (if (and (shell "killall -q rpc.gssd")
	     (shell "killall -q rpc.svcgssd"))
	#t
	#f)))
