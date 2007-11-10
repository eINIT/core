(notice "hello world, from a scheme module")

(critical "hello world, from a scheme module, as a critical message")

(if (register-module 's-hello "some dummy module")
    (notice "dummy module created")
    (critical "couldn't create dummy module"))
