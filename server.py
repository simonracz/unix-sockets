import socket
import select

def main():
    uxs = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    uxs.connect("./unix.sock")
    uxs.send(b"Hello")
    lfd = socket.recv_fds(uxs, 512, 1) # lfd is a 4 tuple
    uxs.close()
    sock = socket.socket(fileno=lfd[1][0])
    # sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM | socket.SOCK_NONBLOCK | socket.SOCK_CLOEXEC);
    # server_address = ('localhost', 8321)
    # sock.bind(server_address)
    # sock.listen(10)
    epoll = select.epoll()
    with sock, epoll:
        fd_to_socket = {}
        epoll.register(sock, select.EPOLLIN)
        while True:
            events = epoll.poll()
            for fd, e in events:
                print(f"fd:{fd} events:{e}")
                if fd == sock.fileno(): # server socket, new client incoming
                    if e & (select.EPOLLERR | select.EPOLLHUP):
                        raise Exception("Server socket error")
                    c_sock, _ = sock.accept()
                    c_sock.setblocking(0)
                    epoll.register(c_sock, select.EPOLLIN)
                    fd_to_socket[c_sock.fileno()] = c_sock
                    continue
                if e & select.EPOLLIN:
                    c_sock = fd_to_socket[fd]
                    data = c_sock.recv(1024)
                    if not data: # EOF, client closed connection
                        del fd_to_socket[fd]
                        c_sock.close()
                    else:
                        c_sock.sendall(b"Hello from Python\n")
                if e & (select.EPOLLERR | select.EPOLLHUP):
                    del fd_to_socket[fd]
                    s = socket.socket(fileno=fd)
                    s.close()


if __name__ == '__main__':
    main()

