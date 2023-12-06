#include <arpa/inet.h>

struct ip_port {
    char ip[INET_ADDRSTRLEN];
    char port[22];
};

struct MSG {
    char command;
    struct ip_port targets[20];
};