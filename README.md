UDP port redirector
===================
[![Badge][]][ISC] [![Travis Status][]][Travis]

`uredir` is a small [ISC][] licensed tool to redirect UDP connections.
It can be used to forward connections on select external interfaces to
loopback.

- In normal mode it forwards packets to a given destination and
  remembers the sender's address.  Packets received from the given
  destination are forwarded to the sender.
- In inetd mode `uredir` lingers for three (3) seconds after forwarding
  a reply.  This to prevent inetd from spawning new instances for
  multiple connections, e.g. an SNMP walk.

Tested and built for Linux systems.

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


Example
-------

Command line examples:

    uredir 0.0.0.0:53 192.168.0.1:53

To run `uredir` from a process monitor like [Finit][] or systemd, tell it
to not background itself and to only use the syslog for log messages:

    uredir -n -s :53 127.0.0.1:53

Inetd example:

    snmp  dgram  udp  wait  root  /usr/sbin/tcpd /usr/local/bin/uredir -i 127.0.0.1:16161


Origin & References
-------------------

`uredir` was originally based on [udp_redirect.c][] by Ivan Tikhonov.
For v3.0 the `tuby()` backend was replaced with the `youdp.c`, written
by [Tobias Waldekranz][].  The project then also changed license from
zlib to ISC.  The project is actively maintained by [Joachim Nilsson][]
at [GitHub][], please use its interface for reporting bugs and an pull
requests.

`uredir` was heavily inspired by redir(1), originally by Sam Creasey but
now also maintained by Joachim.

[ISC]:               https://en.wikipedia.org/wiki/ISC_license
[Badge]:             https://img.shields.io/badge/License-ISC-blue.svg
[Finit]:             https://github.com/troglobit/finit
[GitHub]:            https://github.com/troglobit/uredir
[udp_redirect.c]:    http://brokestream.com/udp_redirect.html
[Joachim Nilsson]:   http://troglobit.com
[Tobias Waldekranz]: https://github.com/wkz
[Travis]:            https://travis-ci.org/troglobit/uredir
[Travis Status]:     https://travis-ci.org/troglobit/uredir.png?branch=master

<!--
  -- Local Variables:
  -- mode: markdown
  -- End:
  -->
