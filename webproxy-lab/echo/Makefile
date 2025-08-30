CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -lpthread

SRCDIR = .
TARGETS = client server

all: $(TARGETS)

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

client: client.c csapp.o
	$(CC) $(CFLAGS) -o client csapp.o client.c $(LDFLAGS)
	
server: server.c csapp.o
	$(CC) $(CFLAGS) -o server csapp.o server.c $(LDFLAGS)

clean:
	rm -rf $(TARGETS) csapp.o
