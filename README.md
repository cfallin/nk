nk: a small cooperative-thread/message-passing "nanokernel" in userspace
------------------------------------------------------------------------

`nk` ("nanokernel") is a runtime built on top of pthreads that implements
cooperative (non-preemptive) multithreading between cheap user threads and
one-off deferred procedure calls (DPCs). It supports message-passing between
these threads and deferred calls, and implements some other basic
synchronization such as semaphores/mutexes. It may coexist with other
pthreads-based code in the same process.


Building
--------

`nk` is built with CMake. Simply create a build directory, run `cmake [src
dir]` there, and `make`. The `nk_test` binary will run unit tests.

Author and License
------------------

`nk` was written by, and is copyright(c) 2016 by, Chris Fallin
&lt;cfallin@c1f.net&gt;. It is released under the MIT License, which may be
found in the `LICENSE` file.
