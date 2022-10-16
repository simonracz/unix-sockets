#define _GNU_SOURCE
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <signal.h>

int create_tcp_server_socket() {
	int sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (sock == -1) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8321);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (bind(sock, (struct sockaddr*) &addr, sizeof(addr)) == -1){
		perror("bind");
		exit(EXIT_FAILURE);
	}
	if (listen(sock, 1000) == -1) {
		perror("listen");
		exit(EXIT_FAILURE);
	}
	return sock;
}

int create_unix_server_socket() {
    int sock = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (sock == - 1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    if (remove("./unix.sock") == -1 && errno != ENOENT) {
        perror("remove");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "./unix.sock", sizeof(addr.sun_path) -1);
    int ret = bind(sock, (struct sockaddr*) &addr, sizeof(addr));
    if (ret == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    ret = listen(sock, 1);
    if (ret == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    return sock;
}

void send_fd(int uxs, int fd) {
    struct msghdr msg = { 0 };
    struct cmsghdr *cmsg;
    char iobuf[1];
    struct iovec io = {
        .iov_base = iobuf,
        .iov_len = sizeof(iobuf)
    };
    union {         /* Ancillary data buffer, wrapped in a union
                        in order to ensure it is suitably aligned */
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
    ssize_t ns = sendmsg(uxs, &msg, 0);
    if (ns == -1) {
        perror("sendmsg");
        exit(EXIT_FAILURE);
    }
}

void add_to_epoll(int efd, int fd) {
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		perror("epoll_ctl");
		exit(EXIT_FAILURE);
	}
}

void remove_from_epoll(int efd, int fd) {
        struct epoll_event ev;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(efd, EPOLL_CTL_DEL, fd, &ev) == -1) {
                perror("epoll_ctl");
                exit(EXIT_FAILURE);
        }
}

int set_up_epoll(int sock, int uxs) {
	int efd = epoll_create1(EPOLL_CLOEXEC);
	if (efd == -1) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}
	add_to_epoll(efd, sock);
	add_to_epoll(efd, uxs);
	return efd;
}

int accept_client(int sock) {
	int c_sock = accept4(sock, NULL, 0, SOCK_CLOEXEC | SOCK_NONBLOCK);
	if (c_sock == -1) {
		perror("accept");
		// Let's move on
	}
	return c_sock;
}

void respond_to_client(int fd, char*buffer, int* num_clients) {
	while(1) {
		ssize_t read_num = read(fd, buffer, 1024);
	        if (read_num == -1) {
			if (errno == EINTR) {
				continue;
			}
			perror("read");
			(*num_clients)--;
			close(fd);
			break;
		}
		if (read_num == 0) { // EOF -> client is closing
			(*num_clients)--;
			close(fd);
			break;
		}
		while (1) {
			if (write(fd, "Hello from C\n", 13) == -1) {
				if (errno == EINTR) {
					continue;
				}
				perror("write");
				(*num_clients)--;
				close(fd);
				break;
			}
			break;
		}
		break;
	}
}

void debug_epoll(struct epoll_event * e) {
    printf("  fd=%d; events: %s%s%s%s\n",
            (*e).data.fd,
            ((*e).events & EPOLLIN) ? "EPOLLIN " : "",
            ((*e).events & EPOLLHUP) ? "EPOLLHUP " : "",
            ((*e).events & EPOLLERR) ? "EPOLLERR " : "",
            ((*e).events & EPOLLRDHUP) ? "EPOLLRDHUP " : "");
}

int main() {
	signal(SIGPIPE, SIG_IGN);
	int sock = create_tcp_server_socket();
	int uxs = create_unix_server_socket();
	int efd = set_up_epoll(sock, uxs);
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	struct epoll_event events[10];
	memset(&events, 0, sizeof(events));
	int closing = 0;
	int uxc_client = -1; // We support a single Unix Socket client
	int num_clients = 0;
	while (1) {
		if (closing && num_clients == 0) break;
		int ret = epoll_wait(efd, events, 10, -1);
		if (ret == -1) {
			if (errno == EINTR) {
				continue;
			}
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < ret; ++i) {
			struct epoll_event e = events[i];
			debug_epoll(&e);
			if (e.data.fd == sock) { // Server listening socket
				int c_sock = accept_client(sock);
				if (c_sock == -1) continue;
				num_clients++;
				add_to_epoll(efd, c_sock);
				continue;
			}
			if (e.data.fd == uxs) { // Unix Server socket
				int uxc_sock = accept_client(uxs);
				if (uxc_sock == -1) continue; // Move on
				uxc_client = uxc_sock;
				add_to_epoll(efd, uxc_sock);
				continue;
			}
			if (e.data.fd == uxc_client) {
				if (e.events & (EPOLLHUP | EPOLLERR)) {
					close(uxc_client);
					continue;
				}
				int ret = read(uxc_client, buffer, 1024);
				// Check secret handshake here?
				if (ret == 0) {
					close(uxc_client);
					continue;
				}
				send_fd(uxc_client, sock); // We send the tcp listening socket
				closing = 1;
				remove_from_epoll(efd, sock); // Stop listening to new clients
				close(sock);
				continue;
			}
			if (e.events & EPOLLIN) { // Client request
				respond_to_client(e.data.fd, buffer, &num_clients);
			}
			if (e.events & (EPOLLERR | EPOLLHUP)) {
				num_clients--;
				close(e.data.fd);
			}
		}
	}
	printf("Server exiting");
	close(efd);
	// close(sock);
}




























