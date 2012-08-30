sockjmp
=======

Lightweight TCP socket bridge

1. Objectives

The main goal of sockjmp is to provide a way to create various TCP channels
even with high constraints. Sockjmp provides a way to create various sockets
(server sockets, client sockets and encrypted sockets) and to bridge them
automatically.

This tool may be very useful during a pentest when a transparent bridge has
to be set up on a compromised server to allow bouncing from the Internet to
an internal service. Moreover, some cases are hard to handle, especially when
reverse TCP connections are used to connect a client (on pentester side) to a
remote service (on target server side).

2. Features

Sockjmp allows basic TCP socket bridging operations:

- Local listening TCP socket creation
- Remote destination TCP socket creation
- Encrypted TCP socket creation
- Unified bridging between all types of created sockets

Therefore, it is easy to create a channel like this:

[ pentester client soft.  ]----
                              | (connected to localhost:1234)
[ localhost:1234 (server) ]<---
                              | (local bridge, pentester side)
[ 0.0.0.0:1235 (server)   ]<---
                              | (normal reverse TCP connection)
[ remote server:6555      ]----

In this case, the pentester set up two listening sockets, make the remote
server connect to its machine on port 1235, and its client software connect
to localhost on port 1234, sockjmp handling the bridging stuff, running on
pentester side.

This is a particular use case netcat or even ssh cannot help whereas sockjmp
does. To avoid information leak, the reverse TCP connection coming from the
compromised server can be encrypted thanks again to sockjmp. A quick way to
do this consists in dropping onto the remote server a statically linked
sockjmp version, and use it to create the reverse TCP connection. Therefore,
all data sent between the pentester sockjmp instance and the server-side
instance is encrypted and cannot be intercepted (well, this is possible but
not trivial).





