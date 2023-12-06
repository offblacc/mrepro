#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // TODO clean up these afterwards
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "wrappers.h"

#define DEFAULT_PORT "1234"
#define UDP_GREET "HELLO\n"
#define DEF_PAYLOAD ""

int main(int argc, char **argv) {
    char *payload = DEF_PAYLOAD;
    char *port = DEFAULT_PORT;

    int ch;
    while ((ch = getopt(argc, argv, "l:p:")) != -1) {
        switch (ch) {
            case 'l':
                port = optarg;
                break;
            case 'p':
                payload = optarg;
                break;
        }
    }

    printf("Payload is %s\n", payload);

    struct sockaddr_in myaddr, claddr;
    myaddr.sin_port = htons(argc > 1 ? atoi(port) : atoi(DEFAULT_PORT));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;

    int sock = Socket(AF_INET, SOCK_DGRAM, 0);  // IPv4 family, datagram socket, default (udp

    Bind(sock, (struct sockaddr *)&myaddr, sizeof(myaddr));

    char recv_msg[10];
    socklen_t addrsize = sizeof(claddr);
    while (1) {
        // don't need this, any residue values will be overwritten or skipped because of nulterm
        memset(&recv_msg, 0, sizeof(recv_msg));

        Recvfrom(sock, &recv_msg, sizeof(recv_msg), 0, (struct sockaddr *)&claddr, &addrsize);
        if (strcmp(recv_msg, UDP_GREET) != 0) {
            printf("Unknown message recieved.\n");
            continue;
        }
        printf("Hello recieved. Sending payload back to bot\n");

        int psize = strlen(payload);
        int nsent = Sendto(sock, payload, psize, 0, (struct sockaddr *)&claddr, sizeof(claddr));
        if (nsent < psize) printf("Payload sent, but only %d bytes out of %d\n", nsent, psize);
    }
    return 0;
}