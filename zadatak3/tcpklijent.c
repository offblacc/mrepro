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
#define DEFAULT_IP "localhost"

int main(int argc, char **argv) {
    // tcpklijent [-s server] [-p port] [-c] filename
    int ch;
    char server[50], port[20];
    strcpy(server, DEFAULT_IP);
    strcpy(port, DEFAULT_PORT);
    char *filename = NULL;
    unsigned offset = 0;
    bool c;  // TODO what if connect fails - err msg fix

    while ((ch = getopt(argc, argv, "s:p:c")) != -1) {
        switch (ch) {
            case 's':
                strcpy(server, optarg);
                break;
            case 'p':
                strcpy(port, optarg);
                break;
            case 'c':
                c = true;
                break;
            default:
                errx(1, "Usage: tcpklijent [-s server] [-p port] [-c] filename");
        }
    }

    int i = optind;
    if (i >= argc) errx(1, "Usage: tcpklijent [-s server] [-p port] [-c] filename");
    filename = argv[i];

    bool file_exists = access(filename, F_OK) != -1;
    if (!c && file_exists) {
        fprintf(stderr, "File %s already exists, see -c flag to continue\n", filename);
        exit(1);
    } else if (c) {
        if (!file_exists) {
            offset = (unsigned)0;
        } else if (access(filename, W_OK) == -1) {
            fprintf(stderr, "Cannot write to file %s\n", filename);
            exit(1);
        } else {  // find offset
            FILE *f = fopen(filename, "r");
            fseek(f, 0, SEEK_END);
            offset = (unsigned)ftell(f);
            fclose(f);
        }
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // ==== CREATING CLIENT SOCKET ====
    int sockme;
    sockme = Socket(AF_INET, SOCK_STREAM, 0);

    // ==== CREATE SERVER SOCKET ====
    int sockserv;
    sockserv = Socket(AF_INET, SOCK_STREAM, 0);

    // ==== CONNECT TO SERVER ====
    Getaddrinfo(server, port, &hints, &res);
    Connect(sockserv, res->ai_addr, res->ai_addrlen);
    printf("Connected to server %s:%s\n", server, port);

    // ==== SEND REQUEST TO SERVER ====
    Send(sockserv, &offset, sizeof(offset), 0);
    Send(sockserv, filename, strlen(filename) + 1, 0);
    printf("Sent request to server\n");

    // ==== RECEIVE RESPONSE FROM SERVER ====
    struct response resp;
    bool first_package = true;
    FILE *f = NULL;
    while (true) {
        memset(&resp, 0, sizeof(resp));
        int nrecvd;
        if (first_package) {
            nrecvd = Recv(sockserv, &resp, sizeof(resp), 0);
        } else {
            nrecvd = Recv(sockserv, &resp.data, sizeof(resp.data), 0);
        }
        if (nrecvd == 0) break;
        if (first_package) {
            switch (resp.status_code) {
                case NOSUCHFILE:
                    fprintf(stderr, "File %s does not exist on server\n", filename);
                    break;
                case NOACCESS:
                    fprintf(stderr, "File %s cannot be accessed on server\n", filename);
                    break;
                case UNDEFINED:
                    fprintf(stderr, "Undefined error occurred on server\n");
                    break;
            }
            if (resp.status_code != 0) {
                close(sockme);
                close(sockserv);
                exit(resp.status_code);
            }
        }

        FILE *f = fopen(filename, "ab");
        if (f == NULL) errx(1, "fopen");

        int written = Fwrite(resp.data, 1, first_package ? nrecvd - sizeof(resp.status_code) : nrecvd, f);
        fflush(f);
        ftruncate(fileno(f), ftell(f));
        fclose(f);
        first_package = false;
    }
    printf("Got to the end\n");
    if (f != NULL) fclose(f);
    close(sockme);
    close(sockserv);
    return 0;
}
