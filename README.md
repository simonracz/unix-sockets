# The Superpower of Unix Sockets
Code created during recording the [The Superpower of Unix Sockets](https://youtu.be/xK75CXZiJGE) video

# Building
```
gcc -std=gnu17 -o server ./server.c
```

# Running
```
pipenv shell
./server
python ./client.py
python ./server.py
tail -f /var/log/client.log
```

# Syslog config
/etc/rsyslog.d/80-custom-client.conf
```
:programname, contains, "client" /var/log/client.log
```

# Some man pages
```
man 2 sendmsg
man 3 CMSG
man -k epoll
man 2 socket
man 2 bind
man 2 listen
man 2 connect
```

# BONUS
New syscall [pidfd_getfd](https://man7.org/linux/man-pages/man2/pidfd_getfd.2.html)

# Copy of notes.txt

```
IPC

message queues, pipes, shared memory, local file?, sockets

TCP, UDP

TCP
connection oriented
reliable
ordered
stream based

UDP
connectionless
message based
unreliable
can have duplicated messages
unordered

Unix sockets
TCP, UDP
TCP + UDP
connection oriented
message based
reliable
no duplicated messages
ordered

You can pass filedescriptors through it.

PROCESS A, PROCESS B

Socket API

SERVER
socket() -> server_socket
bind()
listen()

loop:
   accept() -> client_socket

CLIENT
socket()
connect()

Communication
write()
read()
recv()
send()
sendmessage()
recvmessage()

close()

SERVER DESIGN

Multiple processes?
Multiple threads?
Or combine them?

I/O multiplexing
read()
non-blocking mode for filedescriptors

select()
poll()
->waiting mode
signal driven I/O
epoll()
io_uring

Language support:
Does it even support unix sockets?
Does it support the full feature set of unix sockets?
fd -> Socket object
Socket object -> fd
send_fds
recv_fds

Python, Go, Rust, Java
```
