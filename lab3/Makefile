PROJECT = bot server CandC
SOURCE = bot.c server.c CandC.c
HEADERS = wrappers.h #message.h

CC = cc

CFLAGS = -Wall -g -pthread # -fsanitize=address # fsanitize causes problems on ubuntu vm
OBJECTS = ${SOURCE:.c=.o}

CNC_C = CandC.c
CNC_O = ${CNC_C:.c=.o}

BOT_C = bot.c
BOT_O = ${BOT_C:.c=.o}

SERVER_C = server.c
SERVER_O = ${SERVER_C:.c=.o}

all: bot server CandC

bot: $(BOT_C) $(BOT_O)
	$(CC) $(CFLAGS) $(BOT_O) -o $@

server: $(SERVER_C) $(SERVER_O)
	$(CC) $(CFLAGS) $(SERVER_O) -o $@

CandC: $(CNC_C) $(CNC_O)
	$(CC) $(CFLAGS) $(CNC_O) -o $@

clean:
	-rm -f $(PROJECT) $(OBJECTS) *.core
