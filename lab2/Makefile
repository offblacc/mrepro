PROJECT = bot server
SOURCE = bot.c server.c
HEADERS = message.h wrappers.h

CC = cc

CFLAGS = -Wall -g
OBJECTS = ${SOURCE:.c=.o}

BOT_C = bot.c
BOT_O = ${BOT_C:.c=.o}

SERVER_C = server.c
SERVER_O = ${SERVER_C:.c=.o}

all: bot server

bot: $(BOT_C) $(BOT_O)
	$(CC) $(CFLAGS) $(BOT_O) -o $@

server: $(SERVER_C) $(SERVER_O)
	$(CC) $(CFLAGS) $(SERVER_O) -o $@

clean:
	-rm -f $(PROJECT) $(OBJECTS) *.core
