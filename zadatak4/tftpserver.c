#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <getopt.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "wrappers.h"

// TFTP opcodes
#define RRQ 0x1
#define WRQ 0x2
#define DATA 0x3
#define ACK 0x4
#define ERROR 0x5

#define LOGMSGLEN 100 /* Maximum log message length - change if necessary */

#define PORT_LEN 22
#define TFTP_DIR "/tftpboot/"

void tftpify_filename(char **filename);
void lf_to_crlf(char **str, size_t str_len);
void tolowercase(char *str);
void *handle_connection(void *arg);
void send_error(int sock, char *ip, char *port, char *message);

bool dem;  // daemon mode
char *ident;

struct ip_port {
    char ip[INET_ADDRSTRLEN];
    char port[PORT_LEN];
};

/**
 * The thread handling the entire connection with a client.
*/
void *handle_connection(void *arg) {
    char *packet_ip_port = (char *)arg;
    int opcode;
    memcpy(&opcode, packet_ip_port, 2);
    opcode = ntohs(opcode);

    char *cli_port = malloc(PORT_LEN);
    memcpy(cli_port, packet_ip_port + 516 + INET_ADDRSTRLEN, PORT_LEN);

    char *ip = malloc(INET_ADDRSTRLEN);
    memset(ip, 0, INET_ADDRSTRLEN);
    memcpy(ip, packet_ip_port + 516, INET_ADDRSTRLEN);

    int sockfd = Socket(AF_INET, SOCK_DGRAM, 0);

    if (opcode != RRQ) {
        if (dem) {
            openlog(ident, LOG_INFO, LOG_FTP);
            syslog(LOG_INFO, "TFTP ERROR kôd proizvoljni tekst");
            closelog();
        } else {
            fprintf(stderr, "TFTP ERROR Opcode is not RRQ, sending error\n");
        }

        send_error(sockfd, ip, cli_port, "Illegal TFTP operation; not (yet?) supported or invalid TFTP opcode");
        free(cli_port);
        free(ip);
        return NULL;
    }

    int filename_len = strlen(packet_ip_port + 2);
    char *filename = malloc(filename_len + 1);
    memset(filename, 0, filename_len + 1);
    memcpy(filename, packet_ip_port + 2, filename_len + 1);

    char *mode;
    tolowercase(packet_ip_port + 2 + filename_len + 1);
    if (strcmp(packet_ip_port + 2 + filename_len + 1, "octet") == 0) {
        mode = "octet";
    } else if (strcmp(packet_ip_port + 2 + filename_len + 1, "netascii") == 0) {
        mode = "netascii";
    } else {
        if (dem) {
            openlog(ident, LOG_INFO, LOG_FTP);
            syslog(LOG_INFO, "TFTP ERROR Mode is not octet or netascii\n");
            closelog();
        } else {
            fprintf(stderr, "TFTP ERROR Mode is not octet or netascii\n");
        }
        send_error(sockfd, ip, cli_port, "Unsupported mode");
        free(arg);
        free(filename);
        free(cli_port);
        free(ip);
        return NULL;
    }

    free(arg);

    if (dem) {
        openlog(ident, LOG_INFO, LOG_FTP);
        syslog(LOG_INFO, "%s:%s->%s", ip, cli_port, filename);
        closelog();
    } else {
        printf("%s:%s->%s\n", ip, cli_port, filename);
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints = (struct addrinfo){
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
        .ai_flags = AI_PASSIVE,
    };
    Getaddrinfo(NULL, "0", &hints, &res);
    Bind(sockfd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    tftpify_filename(&filename);

    FILE *file = fopen(filename, "r");
    if (access(filename, F_OK) == -1) {
        if (dem) {
            openlog(ident, LOG_INFO, LOG_FTP);
            syslog(LOG_INFO, "TFTP ERROR File does not exist");
            closelog();
        } else {
            fprintf(stderr, "TFTP ERROR File does not exist\n");
        }
        send_error(sockfd, ip, cli_port, "File does not exist");
        return NULL;
    }

    struct addrinfo *server_res;
    Getaddrinfo(ip, cli_port, &hints, &server_res);

    // setting up for the incoming loop where we send data
    char *buff = malloc(512);
    unsigned block = 1;
    bool eof = false;

    fd_set master;
    fd_set read_fds;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &master);
    // struct timeval tv = {.tv_sec = 1, .tv_usec = 0};  // RTO
    for (;;) {
        if (eof) {
            printf("EOF\n");
            break;
        }

        memset(buff, 0, 512);

        int bytes_read = 0;
        do {
            bytes_read += fread(buff + bytes_read, 1, 512 - bytes_read, file);
        } while (bytes_read < 512 && !(eof = feof(file)));

        if (strcmp(mode, "netascii") == 0) {
            int lf_count = 0;
            for (int i = 0; i < bytes_read; i++) {
                if (buff[i] == '\n') {
                    lf_count++;
                }
            }
            lf_to_crlf(&buff, bytes_read);
            fseek(file, -lf_count, SEEK_CUR);  // accomodate for the bytes cut off by adding \r
            bytes_read = bytes_read + lf_count > 512 ? 512 : bytes_read + lf_count;
        }

        char *data = malloc(4 + bytes_read);
        data[0] = 0;
        data[1] = DATA;
        data[2] = block >> 8;  // first byte of block
        data[3] = block;       // second byte of block

        memcpy(data + 4, buff, bytes_read);

        char *ack = malloc(4);
        bool got_ack = false;
        for (int i = 0; i < 4; i++) {
            if (got_ack) {
                break;
            }
            Sendto(sockfd, data, 4 + bytes_read, 0, server_res->ai_addr, server_res->ai_addrlen);
            read_fds = master;
            Select(sockfd + 1, &read_fds, NULL, NULL, NULL);
            if (FD_ISSET(sockfd, &read_fds)) {
                struct sockaddr *their_addr = malloc(sizeof(struct sockaddr));
                socklen_t addr_len = sizeof(their_addr);
                memset(ack, 0, 4);
                Recvfrom(sockfd, ack, 4, 0, their_addr, &addr_len);
                struct sockaddr_in *their_addr_in = (struct sockaddr_in *)their_addr;

                char *their_ip = malloc(INET_ADDRSTRLEN);
                char *their_port = malloc(PORT_LEN);
                Inet_ntop(AF_INET, &(((struct sockaddr_in *)their_addr)->sin_addr), their_ip, INET_ADDRSTRLEN);

                int port = ntohs(their_addr_in->sin_port);
                snprintf(their_port, PORT_LEN, "%d", port);

                if (atoi(their_port) != atoi(cli_port)) {
                    if (dem) {
                        openlog(ident, LOG_INFO, LOG_FTP);
                        syslog(LOG_INFO, "TFTP ERROR Port number is not the same");
                        closelog();
                    } else {
                        fprintf(stderr, "TFTP ERROR Port number is not the same\n");
                    }
                    send_error(sockfd, their_ip, their_port, "I don't know you");
                }

                opcode = 0;
                memcpy(&opcode, ack, 2);
                opcode = ntohs(opcode);

                if (opcode != ACK) {
                    printf("Opcode is not ACK\n");
                    send_error(sockfd, their_ip, their_port, "Wrong opcode, expected ACK");
                    free(their_ip);
                    free(their_port);
                    free(their_addr);
                    free(data);
                    free(ack);
                    break;
                }
                unsigned ack_data_block = 0;
                memcpy(&ack_data_block, ack + 2, 2);
                ack_data_block = ntohs(ack_data_block);
                if (ack_data_block != block) {
                    send_error(sockfd, their_ip, their_port, "Wrong ACK block number");
                    free(their_ip);
                    free(their_port);
                    free(their_addr);
                    free(data);
                    free(ack);
                    break;
                }
                got_ack = true;
                free(their_ip);
                free(their_port);
                free(their_addr);
            }
        }
        if (!got_ack) {
            printf("Did not get ACK\n");
            send_error(sockfd, ip, cli_port, "Did not get ACK");
            free(data);
            free(ack);
            break;
        }
        free(data);
        free(ack);

        block++;
        block %= 65536;
    }

    fclose(file);
    free(buff);
    free(filename);
    free(cli_port);
    free(ip);
    freeaddrinfo(server_res);
    return NULL;
}

/**
 * Modifies the filename so that it makes sure it is prefixed with TFTP_DIR.
 * If it is already prefixed, it does nothing. Otherwise, adds the prefix,
 * as all the files TFTP server is allowed to serve are in the TFTP_DIR directory.
 * @param filename The filename to modify.
 */
void tftpify_filename(char **filename) {
    int len = strlen(*filename);
    if (strncmp(*filename, TFTP_DIR, strlen(TFTP_DIR)) == 0) {
        return;
    }

    char *old_filename = malloc(len + 1);
    memcpy(old_filename, *filename, len + 1);

    *filename = realloc(*filename, strlen(TFTP_DIR) + len + 1);
    memset(*filename, 0, strlen(TFTP_DIR) + len + 1);
    if (*filename == NULL) {
        return;  // TODO error handling; this is fatal but should not happen
    }

    memcpy(*filename, TFTP_DIR, strlen(TFTP_DIR));
    memcpy(*filename + strlen(TFTP_DIR), old_filename, len + 1);

    free(old_filename);
}

/**
 * Converts all the \n characters in the string to \r\n. Assumes LF style
 * line endings, as this is a Unix implementation.
 * @param str The string to convert.
 */
void lf_to_crlf(char **str, size_t str_len) {
    size_t len = str_len;
    size_t i, j;
    char *new_str = malloc(2 * len + 2);

    for (i = 0, j = 0; i < len; i++) {
        if ((*str)[i] == '\n') {
            new_str[j++] = '\r';
        }
        new_str[j++] = (*str)[i];
    }

    new_str[j] = '\0';
    free(*str);
    *str = new_str;
}

/**
 * Sends the message to the destination, adding error opcode and
 * a (random; constant in this case) error code.
 * @param sock The socket to send from
 * @param ip Destination adress
 * @param port Destination port
 * @param message The message to send
 */
void send_error(int sock, char *ip, char *port, char *message) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints = (struct addrinfo){
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
        .ai_flags = AI_PASSIVE,
    };
    char *sending = malloc(strlen(message) * sizeof(char) + 5);
    memset(sending, 0, strlen(message) + 5);
    sending[1] = 5;
    sending[3] = 1;
    memcpy(sending + 4, message, strlen(message));
    Getaddrinfo(ip, port, &hints, &res);
    Sendto(sock, sending, strlen(message) * sizeof(char) + 5, 0, res->ai_addr, res->ai_addrlen);
    free(sending);
}

/**
 * Converts a string to lowercase. Reads until the first null byte.
 * @param str The string to convert.
 * @return The converted string.
 */
void tolowercase(char *str) {
    for (int i = 0; i < strlen(str); i++) {
        str[i] = tolower(str[i]);
    }
}

int main(int argc, char **argv) {
    int ch;
    dem = false;
    while ((ch = getopt(argc, argv, "d")) != -1) {
        switch (ch) {
            case 'd':
                dem = true;
                break;
            default:
                errx(1, "Usage: tftpserver [-d] port_name_or_number");
                break;
        }
    }
    if (optind != argc - 1) {
        errx(1, "Usage: tftpserver [-d] port_name_or_number");
    }

    FILE *logfile = NULL;

    char *port = argv[optind];

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints = (struct addrinfo){
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
        .ai_flags = AI_PASSIVE,
    };
    Getaddrinfo(NULL, port, &hints, &res);
    int sockfd = Socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    Bind(sockfd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    struct sockaddr_in client;
    socklen_t clientlen = sizeof(client);

    if (dem) {
        printf("Running as daemon\n");
        if (daemon(0, 0) == -1) {
            errx(1, "daemon");
        }
        ident = malloc(LOGMSGLEN);
        memset(ident, 0, LOGMSGLEN);
        snprintf(ident, LOGMSGLEN, "ls53099:MrePro tftpserver[%d]", getpid());
    }

    while (true) {
        char buf[516];
        memset(buf, 0, 516);
        struct sockaddr_in client;
        int nrecv = Recvfrom(sockfd, buf, 516, 0, (struct sockaddr *)&client, &clientlen);
        char *ip = inet_ntoa(client.sin_addr);
        char *port = malloc(PORT_LEN);
        sprintf(port, "%d", ntohs(client.sin_port));

        char *sending = malloc(516 + INET_ADDRSTRLEN + PORT_LEN);  // is freed in each individual thread handling the connection
        memset(sending, 0, 516 + INET_ADDRSTRLEN + PORT_LEN);

        memcpy(sending, buf, nrecv);
        memcpy(sending + 516, ip, strlen(ip));
        memcpy(sending + 516 + INET_ADDRSTRLEN, port, strlen(port));
        pthread_t tid = (pthread_t)malloc(sizeof(pthread_t));
        Pthread_create(&tid, NULL, handle_connection, sending);
        free(port);
    }

    close(sockfd);
    if (dem) {
        closelog();
        fclose(logfile);
    }
    return 0;
}
