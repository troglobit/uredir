
youdp design
============

```
                                                           o. :: outer
                                                           i. :: inner
        redirect() => (o.sd) --------.
                                     ¦
                                     V
                .-----------------[ uEv ]---------------.
                |                    ^                  |
              (o.sd)                 |                (i.sd)
                |                  (i.sd)               |
                V                    |                  V
        { outer_to_inner }           .           { conn_to_outer }
                |                   /                   |
           conn_find() --> conn_new()              recv(i.sd)
                |             |                         |
                '------.------'                    send(o.sd)
                       |
					   V
                  conn_to_inner()
                       |
                  send(i.sd)
```
