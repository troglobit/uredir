udp_redirect - linux udp port forward
=====================================

A small tool to redirect udp packets to another destination. I used it
to test VoIP tool looping back RTP port.

In a normal mode udp_redirect forwards packets to a specified
destination and remember a sender's address.

Packets received from a specified destination are sent back to a
remembered sender. It is mostly what symmetric NAT do.

In an echo mode udp_redirect forwards packets back to a sender.

Tested and used on Linux but should be working on any POSIX system.


License
-------

* [zlib](https://en.wikipedia.org/wiki/Zlib_License)


Version
-------

2008-11-09


Download
--------

* [udp_redirect.c](http://brokestream.com/udp_redirect.html) (1k)


Build
-----

    gcc -o udp_redirect udp_redirect.c


Usage
-----

    ./udp_redirect our-ip our-port send-to-ip send-to-port
    ./udp_redirect our-ip our-port (echo mode)


Example
-------

    ./udp_redirect 0.0.0.0 53 192.168.0.1 53
    ./udp_redirect 0.0.0.0 7
