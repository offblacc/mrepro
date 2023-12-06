#include <arpa/inet.h>
#include <err.h>
#include <getopt.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "message.h"
#include "wrappers.h"

#define DEFAULT_PORT "1234"
#define BACKLOG 10

int main(int argc, char *argv[]) {
    int ch;
    char *port = DEFAULT_PORT;

    while ((ch = getopt(argc, argv, "p:")) != -1) {
        switch (ch) {
            case 'p':
                port = optarg;
                break;
            default:
                errx(1, "Usage: tcpserver [-p port]");
        }
    }

    // check there are no extra arguments
    if (optind < argc) errx(1, "Usage: tcpserver [-p port]");

    int sock; /* Server socket */

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    Getaddrinfo(NULL, port, &hints, &res);

    sock = Socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(port)),
        .sin_addr = {.s_addr = INADDR_ANY},
    };

    Bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    Listen(sock, BACKLOG);

    int newfd; /* Client socket */
    struct sockaddr cliaddr;
    socklen_t sizeofcliaddr = sizeof(cliaddr);
    while (true) {
        printf("Waiting for connection...\n");
        memset(&cliaddr, 0, sizeof(cliaddr));  // the rest i don't need to reset (maybe even this)
        newfd = Accept(sock, &cliaddr, &sizeofcliaddr);

        ssize_t nread, sumread = 0;
        unsigned offset = 0;
        char buf[TCP_BUFFER_SIZE];
        memset(buf, 0, TCP_BUFFER_SIZE);
        sumread = 0;
        while ((nread = Recv(newfd, buf, TCP_BUFFER_SIZE, 0)) > 0) {
            memcpy(buf + sumread, buf, nread);
            sumread += nread;
            if (sumread > sizeof(offset) && ((char *)&buf)[sumread - 1] == '\0') break;
        }

        memcpy(&offset, buf, sizeof(offset));
        printf("I got request with offset %d\n", offset);
        char *filename = malloc(sumread - sizeof(offset) + 1);
        memset(filename, 0, sumread - sizeof(offset) + 1);
        memcpy(filename, (buf + sizeof(offset)), sumread - sizeof(offset));

        bool nosuch = access(filename, F_OK) == -1 || strchr(filename, '/') != NULL;
        struct response resp;
        if (nosuch) {
            printf("No such file\n");
            resp.status_code = NOSUCHFILE;
            char *resptext = "No such file\n";
            strcpy(resp.data, resptext);
            Send(newfd, &resp, 1 + strlen(resptext), 0);
            close(newfd);
            continue;
        } else if (access(filename, R_OK) == -1) {
            printf("No access\n");
            resp.status_code = NOACCESS;
            char *resptext = "Cannot read from file\n";
            strcpy(resp.data, resptext);
            Send(newfd, &resp, 1 + strlen(resptext), 0);
            close(newfd);
            continue;
        }

        resp.status_code = OK;
        FILE *f = fopen(filename, "r");
        if (f == NULL) errx(1, "fopen");


        fseek(f, offset, SEEK_SET);
        long long bytes_read = offset;

        int nsent = 0;
        nread = -1;
        bool include_status = true; // only in first package
        while (true) {
            nread = fread(resp.data, 1, TCP_BUFFER_SIZE, f);
            if (nread < 0) errx(1, "error AND not wrapped one TODO"); // TODO <===
            if (nread == 0) break;
            if (include_status){
                nsent = Send(newfd, &resp, sizeof(resp.status_code) + nread, 0);
                include_status = false;
            } else {
                nsent = Send(newfd, &resp.data, nread, 0);
            }
            printf("Sent %d\n", nsent);
            bytes_read += nread;
        }

        printf("File sent.\n");
        fclose(f);
        close(newfd);
    } // TODO oporavi se od send failed: Connection reset by peer

    close(sock);
    return 0;
}