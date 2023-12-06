PROJECT = tftpserver
SOURCE = tftpserver.c
#HEADERS = message.h

CC = cc

CFLAGS = -Wall -g -pthread -fsanitize=address -g
OBJECTS = ${SOURCE:.c=.o}

TFTPSERV_C = tftpserver.c
TFTPSERV_O = ${TFTPSERV_C:.c=.o}

all: tftpserver

tftpserver: $(TFTPSERV_C) $(TFTPSERV_O)
	$(CC) $(CFLAGS) $(TFTPSERV_O) -o $@

clean:
	-rm -f $(PROJECT) $(OBJECTS) *.core

