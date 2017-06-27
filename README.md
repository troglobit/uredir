UDP port redirector
===================
[![Badge][]][ISC] [![Travis Status][]][Travis]

`uredir` is a small Linux daemon to redirect UDP connections.  It can be
used to forward connections on small and embedded systems that do not
have (or want to use) iptables.

`uredir` can be used with the built-in inetd service in [Finit][] to
listen for, e.g. SNMP connections, on some (safe) interfaces and forward
to an SNMP daemon on loopback.

- In normal mode it forwards packets to a given destination and
  remembers the sender's address.  Packets received from the given
  destination are forwarded to the sender.
- In inetd mode `uredir` lingers for three (3) seconds after forwarding
  a reply.  This to prevent inetd from spawning new instances for
  multiple connections, e.g. an SNMP walk.

For a TCP port redirector, see [redir][].


Usage
-----

    uredir [-hinsv] [-I NAME] [-l LEVEL] [-t SEC] [SRC:PORT] DST:PORT
    
      -h      Show this help text
      -i      Run in inetd mode, get SRC:PORT from stdin, implies -n
      -I NAME Identity, tag syslog messages with NAME, default: uredir
      -l LVL  Set log level: none, err, info, notice (default), debug
      -n      Run in foreground, do not detach from controlling terminal
      -s      Use syslog, even if running in foreground, default w/o -n
      -t SEC  Set timeout to SEC seconds for inetd connections, default 3
      -v      Show program version

     Bug report address: https://github.com/troglobit/uredir/issues


Example
-------

Command line examples:

    uredir :53 192.168.0.1:53

To run `uredir` from a process monitor like [Finit][] or systemd, tell it
to not background itself and to only use the syslog for log messages:

    uredir -ns :53 127.0.0.1:53

Inetd example:

    snmp  dgram  udp  wait  root  /usr/sbin/tcpd /usr/local/bin/uredir -i 127.0.0.1:16161


Origin & References
-------------------

The `uredir` project is open source under the [ISC][] license and
actively maintained at [GitHub][].  Please use its interface for
reporting bugs and an pull requests.

`uredir` was heavily inspired by [redir(1)][redir], by Sam Creasey.

[ISC]:               https://en.wikipedia.org/wiki/ISC_license
[Badge]:             https://img.shields.io/badge/License-ISC-blue.svg
[Finit]:             https://github.com/troglobit/finit
[GitHub]:            https://github.com/troglobit/uredir
[redir]:             https://github.com/troglobit/redir/
[Joachim Nilsson]:   http://troglobit.com
[Travis]:            https://travis-ci.org/troglobit/uredir
[Travis Status]:     https://travis-ci.org/troglobit/uredir.png?branch=master
