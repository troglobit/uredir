uredir - UDP port redirector
============================

`uredir` is a small [zlib][] licensed tool to redirect UDP connections.
In normal mode it forwards packets to a given destination and remembers
the sender's address.  Packets received from the given destination are
sent back to the sender.  Roughly what symmetric NAT does.

In echo mode `uredir` forwards packets back to the sender.

Tested and used on Linux but should work on any POSIX system.

Usage
-----

    uredir [-hinv] [SRC:PORT] [DST:PORT]
    
      -h  Show this help text
      -i  Run in inetd mode, get SRC:PORT from stdin
      -n  Run in foreground, do not detach from controlling terminal
      -v  Show program version
    
    If DST:PORT is left out the program operates in echo mode.


Example
-------

    uredir 0.0.0.0:53 192.168.0.1:53
    uredir 0.0.0.0:7                   # Echo mode


Origin & References
-------------------

`uredir` is based on [udp_redirect.c][] by Ivan Tikhonov.  All bugs were
added by Joachim Nilsson, so please report them to [GitHub][].


[zlib]: https://en.wikipedia.org/wiki/Zlib_License
[udp_redirect.c]: http://brokestream.com/udp_redirect.html
[GitHub]: https://github.com/troglobit/uredir
