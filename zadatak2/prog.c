#include <err.h>
#include <getopt.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>

// TODO what if too many args ?3
/*
 * prog [-r] [-t|-u] [-x] [-h|-n] [-46] {hostname | IP_address} {servicename | port}
 * -t TCP (default)
 * -u UDP
 * -x hex (4 hex znamenke, 2 okteta)
 * -h host byte order (default)
 * -n network byte order
 * -4 IPv4 (default)
 * -6 IPv6
 * -r reverse lookup
 */

int main(int argc, char *argv[]) {
    int ch;
    int i;
    bool r = false, t = false, u = false, x = false, h = false, n = false, ip4 = false, ip6 = false;  // local var - have to init false, otherwise garbage

    // first, let's parse
    while ((ch = getopt(argc, argv, "rtuxhn46")) != -1) {
        switch (ch) {
            case 'r':
                r = true;
                break;
            case 't':
                t = true;
                break;
            case 'u':
                u = true;
                break;
            case 'x':
                x = true;
                break;
            case 'h':
                h = true;
                break;
            case 'n':
                n = true;
                break;
            case '4':
                ip4 = true;
                break;
            case '6':
                ip6 = true;
                break;
            default:
                break;
        }
    }

    // mutually exclusive: t|u, h|n
    if (t && u) printf("Mutually exclusive flags -t and -u.\n");
    if (h && n) printf("Mutually exclusive flags -h and -n.\n");
    if ((t && u) || (h && n)) return 1;

    i = optind;

    if (i + 2 != argc) {
        if (i == 0) {
            printf("Usage: prog [-r] [-t|-u] [-x] [-h|-n] [-46] {hostname|IP_adress} {servicename|port}\n");
        } else {
            printf("Wrong number of arguments!\n");
            return 2;
        }
    }

    char *arg1 = argv[i++];  // hostname / ip
    char *arg2 = argv[i++];  // servicename / port

    if (r) {  // ip -> hostname | port -> service
        if (ip4 && ip6) {
            printf("In reverse lookup only one, -4 or -6, flag is allowed");
            return 1;
        } if (ip6) {
            struct sockaddr_in6 sa6;
            sa6.sin6_family = AF_INET6;
            char host[NI_MAXHOST];
            char serv[NI_MAXSERV];
            if (inet_pton(AF_INET6, arg1, &(sa6.sin6_addr)) != 1) {
                errx(3, "%s is not a valid IP adress", arg1); // he flushes the stream so no newline is necessary in the string
            }
            if (x) {
                int tmp;
                sscanf(arg2, "%x", &tmp);
            }
            sa6.sin6_port = htons(atoi(arg2));
            int gre = getnameinfo((struct sockaddr *)&sa6, sizeof(struct sockaddr_in6), host, sizeof(host), serv, sizeof(serv), u ? NI_DGRAM : 0); // 0 if TCP, NI_DGRRAM if UDP flag is set
            if (gre) errx(1, "getnameinfo: %s", gai_strerror(gre));
            printf("%s (%s) %s\n", arg1, host, serv); // TODO if serv not found, dont print port num but error appropriate, yes?
        }
        else { // ip 4
            struct sockaddr_in sa;
            sa.sin_family = AF_INET;
            char host[NI_MAXHOST];
            char serv[NI_MAXSERV];
            if (inet_pton(AF_INET, arg1, &(sa.sin_addr)) != 1) {
                errx(3, "%s nije valjana IP adresa", arg1); // he flushes the stream so no newline is necessary in the string
            }
            sa.sin_port = htons(atoi(arg2));
            int gre = getnameinfo((struct sockaddr *)&sa, sizeof(struct sockaddr_in), host, sizeof(host), serv, sizeof(serv), u ? NI_DGRAM : 0); // 0 if TCP, NI_DGRRAM if UDP flag is set
            if (gre) errx(1, "getnameinfo: %s", gai_strerror(gre));
            printf("%s (%s) %s\n", arg1, host, serv); // TODO if serv not found, dont print port num but error appropriate, yes?
            }
    } else {  // hostname -> ip | service -> port
        char addrstr[100];
        if (ip6) {
            struct addrinfo hints, *res;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET6;
            hints.ai_flags |= AI_CANONNAME;
            if (u) {
                hints.ai_socktype = SOCK_DGRAM;
            } else {
                hints.ai_socktype = SOCK_STREAM;
            }
            int err = getaddrinfo(arg1, arg2, &hints, &res);
            if (err) errx(1, "%s", gai_strerror(err));
            while(res){    
                inet_ntop(
                    res->ai_family,
                    &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr,
                    addrstr, 100);
                int port;

                if (n) port = (int) ((struct sockaddr_in *) res->ai_addr)->sin_port;
                else port = (int) ntohs(((struct sockaddr_in *) res->ai_addr)->sin_port);

                printf("%s (%s) ", addrstr, res->ai_canonname);
                if (x) printf("%.4x\n", port);
                else printf("%d\n", port);
                res = res->ai_next;
        }} else {
            struct addrinfo hints, *res;

            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_flags |= AI_CANONNAME;
            if (u) {
                hints.ai_socktype = SOCK_DGRAM;
            } else {
                hints.ai_socktype = SOCK_STREAM;
            }
            int err = getaddrinfo(arg1, arg2, &hints, &res);
            if (err) errx(1, "%s", gai_strerror(err));
            while(res){    
                inet_ntop(
                    res->ai_family,
                    &((struct sockaddr_in *) res->ai_addr)->sin_addr,
                    addrstr, 100);
                int port;

                if (n) port = (int) ((struct sockaddr_in *) res->ai_addr)->sin_port;
                else port = (int) ntohs(((struct sockaddr_in *) res->ai_addr)->sin_port);

                printf("%s (%s) ", addrstr, res->ai_canonname);
                if (x) printf("%.4x\n", port);
                else printf("%d\n", port);
                res = res->ai_next;
            }
            freeaddrinfo(res);
        }
    }
    return 0;
}