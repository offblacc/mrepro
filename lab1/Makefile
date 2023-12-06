PROJECT = bot UDP_server
SOURCE = bot.c UDP_server.c
HEADERS = message.h wrappers.h

CC = cc

CFLAGS = -Wall -g
OBJECTS = ${SOURCE:.c=.o}

BOT_C = bot.c
BOT_O = ${BOT_C:.c=.o}

UDP_SERVER_C = UDP_server.c
UDP_SERVER_O = ${UDP_SERVER_C:.c=.o}


all: bot UDP_server

bot: $(BOT_C) $(BOT_O)
	$(CC) $(CFLAGS) $(BOT_O) -o $@

UDP_server: $(UDP_SERVER_C) $(UDP_SERVER_O)
	$(CC) $(CFLAGS) $(UDP_SERVER_O) -o $@

clean:
	-rm -f $(PROJECT) $(OBJECTS) *.core
