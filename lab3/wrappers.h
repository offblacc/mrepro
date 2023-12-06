#include <err.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>

/* Socket wrapper - checks return value, errx if error
 * Returns socket file descriptor from socket call
 */
int Socket(int domain, int type, int protocol) {
    int sockfd = socket(domain, type, protocol);
    if (sockfd < 0) {
        errx(1, "socket creation failed");
    }
    return sockfd;
}

/* Getaddrinfo wrapper - checks return value, errx if error */
void Getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    int ret = getaddrinfo(node, service, hints, res);
    if (ret != 0) {
        errx(1, "getaddrinfo failed");
    }
}

/* Sendto wrapper - checks return value, errx if error
 * Returns number of bytes sent, forwarding return value of sendto call
 */
int Sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    int ret = sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    if (ret < 0) {
        perror("sendto failed");
        // exit(1);
    }
    return ret;
}

int Send(int sockfd, const void *buf, size_t len, int flags) {
    int ret = send(sockfd, buf, len, flags);
    if (ret < 0) {
        perror("send failed");
        exit(1);
    }
    return ret;
}

/* Recvfrom wrapper - checks return value, errx if error
 * Returns number of bytes received, forwarding return value of recvfrom call
 */
int Recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    int ret = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    if (ret < 0) {
        perror("recvfrom failed");
        // errx(1, "recvfrom failed");
    }
    return ret;
}

int Recv(int sockfd, void *buf, size_t len, int flags) {
    int ret = recv(sockfd, buf, len, flags);
    if (ret < 0) {
        perror("recv failed");
        errx(1, "recv failed");
    }
    return ret;
}

/* Bind wrapper - checks return value, errx if error */
void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int ret = bind(sockfd, addr, addrlen);
    if (ret < 0) {
        errx(1, "bind failed");
    }
}

/* Listen wrapper - checks return value, errx if error */
void Listen(int sockfd, int backlog) {
    int ret = listen(sockfd, backlog);
    if (ret < 0) {
        errx(1, "listen failed");
    }
}

/* Accept wrapper - checks return value, errx if error, else return accept call return value */
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int ret = accept(sockfd, addr, addrlen);
    if (ret < 0) {
        errx(1, "accept failed");
    }
    return ret;
}

/* Connect wrapper - checks return value, errx if error */
void Connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int ret = connect(sockfd, addr, addrlen);
    if (ret < 0) {
        errx(1, "connect failed");
    }
}

/* Fwrite wrapper - checks return value, errx if error, returns fwrite call return value */
int Fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    int ret = fwrite(ptr, size, nmemb, stream);
    if (ret < 0) {
        errx(1, "fwrite failed");
    }
    return ret;
}

// Setsockopt wrapper - wraps setsockopt, checks for error and errx if error
void Setsockopt(int socket, int level, int option_name, const void *option_value, socklen_t option_len) {
    if (setsockopt(socket, level, option_name, option_value, option_len) < 0) {
        errx(1, "setsockopt failed");
    }
}

// Select wrapper - wraps select, checks for error and errx if error
int Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    int selret = select(nfds, readfds, writefds, exceptfds, timeout);
    if (selret < 0) {
        perror("select failed");
    }
    return selret;
}

// Read wrapper
ssize_t Read(int fd, void *buf, size_t count) {
    ssize_t ret = read(fd, buf, count);
    if (ret < 0) {
        errx(1, "read failed");
    }
    return ret;
}

// Pthread create wrapper
void Pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    if (pthread_create(thread, attr, start_routine, arg) != 0) {
        perror("pthread_create failed");
    }
}

// getnameinfo wrapper
void Getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host, size_t hostlen, char *serv, size_t servlen, int flags) {
    int err;
    if ((err = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags)) != 0) {
        printf("getnameinfo failed: %s\n", gai_strerror(err));
        exit(1);
    }
}

// inet ntop wrapper
const char *Inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    const char *ret = inet_ntop(af, src, dst, size);
    if (ret == NULL) {
        perror("inet_ntop failed");
        exit(1);
    }
    return ret;
}