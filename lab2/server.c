#include <arpa/inet.h>
#include <err.h>
#include <getopt.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "wrappers.h"

#define DEFAULT_PORT "1234"
#define BACKLOG 10
#define STDIN 0
#define REQPAYLOAD "HELLO\n" /* Message I get when payload is requested from me */

// console commands
#define PRINT "PRINT"
#define SET "SET"
#define QUIT "QUIT"

char *parsecommand(char *buf);

int main(int argc, char **argv) {
    int ch;
    bool payload_set = false;
    char *payloads = "";  // delimiter is :
    char *tcp = DEFAULT_PORT;
    char *udp = DEFAULT_PORT;
    bool quitflag = false;

    while ((ch = getopt(argc, argv, "t:u:p:")) != -1) {
        switch (ch) {
            case 't':
                tcp = optarg;
                break;
            case 'u':
                udp = optarg;
                break;
            case 'p':
                payloads = malloc(strlen(optarg) + 2);
                strcpy(payloads, optarg);
                payloads[strlen(optarg)] = '\n';
                payloads[strlen(optarg) + 1] = '\0';
                payload_set = true;
                printf("Set payload to %s\n", payloads);
                break;
        }
    }

    if (optopt != 0) exit(1);  // no message, getopt will print one

    int udpsock = Socket(AF_INET, SOCK_DGRAM, 0);
    int tcpsock = Socket(AF_INET, SOCK_STREAM, 0);

    Setsockopt(udpsock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &(int){1}, sizeof(int));
    Setsockopt(tcpsock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &(int){1}, sizeof(int));

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
        .ai_flags = AI_PASSIVE,
    };
    struct addrinfo *res_udp;
    Getaddrinfo(NULL, udp, &hints, &res_udp);
    Bind(udpsock, res_udp->ai_addr, res_udp->ai_addrlen);
    freeaddrinfo(res_udp);

    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res_tcp;
    Getaddrinfo(NULL, tcp, &hints, &res_tcp);
    Bind(tcpsock, res_tcp->ai_addr, res_tcp->ai_addrlen);
    Listen(tcpsock, BACKLOG);
    freeaddrinfo(res_tcp);

    fd_set master;
    fd_set read_fds;
    int fdmax;

    int newfd;
    struct sockaddr_in remoteaddr;
    socklen_t addrlen = sizeof(remoteaddr);

    char buf[1024];

    char remoteIP[INET_ADDRSTRLEN];  // just for printing the IP

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    FD_SET(STDIN, &master);
    FD_SET(udpsock, &master);
    FD_SET(tcpsock, &master);

    fdmax = tcpsock > udpsock ? tcpsock : udpsock;
    for (;;) {
        if (quitflag) break;
        read_fds = master;
        Select(fdmax + 1, &read_fds, NULL, NULL, NULL);

        for (int i = 0; i <= fdmax; i++) {
            if (quitflag) break;
            memset(buf, 0, sizeof(buf));
            memset(&remoteaddr, 0, sizeof(remoteaddr));

            if (!FD_ISSET(i, &read_fds)) continue;
            if (i == STDIN) {
                Read(STDIN, buf, sizeof(buf));
                char *command = parsecommand(buf);
                if (strcmp(command, "err") == 0) {
                    printf("Unknown command\n");
                    continue;
                }
                if (strcmp(command, PRINT) == 0) {
                    if (!payload_set) {
                        printf("Payload not set\n");
                    } else {
                        printf("%s", payloads);
                    }
                } else if (strcmp(command, SET) == 0) {
                    payloads = malloc(strlen(buf + strlen(SET) + 1) + 1);
                    strcpy(payloads, buf + strlen(SET) + 1);
                    payloads[strlen(buf) - 1] = '\n';
                    payloads[strlen(buf)] = '\0';
                    printf("Payloads set to %s", payloads);
                    payload_set = true;
                } else if (strcmp(command, QUIT) == 0) {
                    printf("Quitting, bye!\n");
                    quitflag = true;
                } else {
                    printf("Unknown command\n");
                }

            } else if (i == udpsock) {
                Recvfrom(i, buf, sizeof(buf), 0, (struct sockaddr *)&remoteaddr, &addrlen);
                if (!payload_set) {
                    printf("Payload not set, ignoring udp request\n");
                    continue;
                }
                if (strcmp(buf, REQPAYLOAD) == 0) {
                    printf("Got payload request\n");
                    Sendto(i, payloads, strlen(payloads) + 1, 0, (struct sockaddr *)&remoteaddr, addrlen);
                } else {
                    printf("Got some random message, ignoring...\n");
                }
            } else if (i == tcpsock) {
                addrlen = sizeof(remoteaddr);
                newfd = Accept(tcpsock, (struct sockaddr *)&remoteaddr, &addrlen);
                if (newfd > fdmax) {
                    fdmax = newfd;
                }
                Recv(newfd, buf, sizeof(buf), 0);
                if (!payload_set) {
                    printf("Payload not set, ignoring tcp request\n");
                    continue;
                }
                printf("New connection from %s on socket %d\n", inet_ntop(AF_INET, &remoteaddr.sin_addr, remoteIP, INET_ADDRSTRLEN), newfd);
                if (strcmp(buf, REQPAYLOAD) == 0) {
                    printf("Got payload request\n");
                    Send(newfd, payloads, strlen(payloads) + 1, 0);
                } else {
                    printf("Got some random message, ignoring...\n");
                }
                close(newfd);
            }
        }
    }

    close(udpsock);
    close(tcpsock);
    return 0;
}

char *parsecommand(char *buf) {
    int len = strlen(buf);
    if (strncmp(buf, SET, len < strlen(SET) ? len : strlen(SET)) == 0) {
        return SET;
    } else if (strncmp(buf, QUIT, len < strlen(QUIT) ? len : strlen(QUIT)) == 0) {
        return QUIT;
    } else if (strncmp(buf, PRINT, len < strlen(PRINT) ? len : strlen(PRINT)) == 0) {
        return PRINT;
    } else {
        return "err";
    }
}