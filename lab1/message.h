#include <arpa/inet.h>

struct MSG {
    char command;
    char IP1[INET_ADDRSTRLEN];
    char PORT1[22];

    char IP2[INET_ADDRSTRLEN];
    char PORT2[22];

    char IP3[INET_ADDRSTRLEN];
    char PORT3[22];

    char IP4[INET_ADDRSTRLEN];
    char PORT4[22];

    char IP5[INET_ADDRSTRLEN];
    char PORT5[22];

    char IP6[INET_ADDRSTRLEN];
    char PORT6[22];

    char IP7[INET_ADDRSTRLEN];
    char PORT7[22];

    char IP8[INET_ADDRSTRLEN];
    char PORT8[22];

    char IP9[INET_ADDRSTRLEN];
    char PORT9[22];

    char IP10[INET_ADDRSTRLEN];
    char PORT10[22];

    char IP11[INET_ADDRSTRLEN];
    char PORT11[22];

    char IP12[INET_ADDRSTRLEN];
    char PORT12[22];

    char IP13[INET_ADDRSTRLEN];
    char PORT13[22];

    char IP14[INET_ADDRSTRLEN];
    char PORT14[22];

    char IP15[INET_ADDRSTRLEN];
    char PORT15[22];

    char IP16[INET_ADDRSTRLEN];
    char PORT16[22];

    char IP17[INET_ADDRSTRLEN];
    char PORT17[22];

    char IP18[INET_ADDRSTRLEN];
    char PORT18[22];

    char IP19[INET_ADDRSTRLEN];
    char PORT19[22];

    char IP20[INET_ADDRSTRLEN];
    char PORT20[22];
};

struct ip_port {
    char IP[INET_ADDRSTRLEN];
    char PORT[22];
};