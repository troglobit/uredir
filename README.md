UDP port redirector
===================

`uredir` is a small [zlib][] licensed tool to redirect UDP connections.
It can be used to forward connections on select external interfaces to
loopback.

- In normal mode it forwards packets to a given destination and
  remembers the sender's address.  Packets received from the given
  destination are forwarded to the sender.
- In echo mode `uredir` mirrors packets back to the sender.
- In inetd mode `uredir` lingers for three (3) seconds after forwarding
  a reply.  This to prevent inetd from spawning new instances for
  multiple connections, e.g. an SNMP walk.

Tested and used on Linux but should work on any POSIX system.

For a TCP port redirector, see [redir](https://github.com/troglobit/redir/).

Usage
-----

    uredir [-hinv] [-l LEVEL] [SRC:PORT] [DST:PORT]
    
      -h      Show this help text
      -i      Run in inetd mode, get SRC:PORT from stdin
      -l LVL  Set log level: none, err, info, notice (default), debug
      -n      Run in foreground, do not detach from controlling terminal
      -s      Use syslog, even if running in foreground, default w/o -n
      -v      Show program version
    
    If DST:PORT is left out the program operates in echo mode.


Example
-------

Command line examples:

    uredir 0.0.0.0:53 192.168.0.1:53
    uredir 0.0.0.0:7                   # Echo mode

Inetd example:

    snmp	dgram	udp	wait	root	/usr/sbin/tcpd /usr/local/bin/uredir -i 127.0.0.1:16161


Origin & References
-------------------

`uredir` is based on [udp_redirect.c][] by Ivan Tikhonov.  All bugs were
added by Joachim Nilsson, so please report them to [GitHub][].


[zlib]:           https://en.wikipedia.org/wiki/Zlib_License
[GitHub]:         https://github.com/troglobit/uredir
[udp_redirect.c]: http://brokestream.com/udp_redirect.html
