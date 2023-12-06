#include <err.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

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
        errx(1, "sendto failed");
    }
    return ret;
}

/* Recvfrom wrapper - checks return value, errx if error
 * Returns number of bytes received, forwarding return value of recvfrom call
 */
int Recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    int ret = recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    if (ret < 0) {
        errx(1, "recvfrom failed");
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