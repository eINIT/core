(notice "hello world, from a scheme module")

(critical "hello world, from a scheme module, as a critical message")

(if (make-module 's-hello "some dummy module"
                 (list 'provides "s-dummy")
;                 (list 'requires "c" "d")
;                 (list 'after "e" "f")
;                 (list 'before "g" "h")
    )
    (notice "dummy module created")
    (critical "couldn't create dummy module"))

(define-module-action 's-hello 'enable
 (lambda (status)
  (begin
   (feedback status "abc")
   (feedback status "def")
   (feedback status "ghi")
   (shell "echo hello world; sleep 10; true"
          'feedback: status))))

(define-module-action 's-hello 'disable
 (lambda (status) #t))
