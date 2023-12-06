PROJECT = prog
SOURCE = prog.c
#HEADERS = proba.h

#CC = gcc

CFLAGS = -Wall -g
LDFLAGS =
OBJECTS = ${SOURCE:.c=.o}

$(PROJECT): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(PROJECT)

$(OBJECTS): $(HEADERS)

clean:
	-rm -f $(PROJECT) $(OBJECTS) *.core
