PROJECT = tcpserver tcpklijent
SOURCE = tcpserver.c tcpklijent.c
HEADERS = message.h

CC = cc

CFLAGS = -Wall -g
OBJECTS = ${SOURCE:.c=.o}

TCPSERV_C = tcpserver.c
TCPSERV_O = ${TCPSERV_C:.c=.o}

TCPKLI_C = tcpklijent.c
TCPKLI_O = ${TCPKLI_C:.c=.o}


all: tcpserver tcpklijent

tcpserver: $(TCPSERV_C) $(TCPSERV_O)
	$(CC) $(CFLAGS) $(TCPSERV_O) -o $@

tcpklijent: $(TCPKLI_C) $(TCPKLI_O)
	$(CC) $(CFLAGS) $(TCPKLI_O) -o $@

clean:
	-rm -f $(PROJECT) $(OBJECTS) *.core
