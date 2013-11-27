CC=gcc
CFLAGS = -g

all: server client

server: server.o
	$(CC) -o server server.o $(LIBS)

client: client.o 
	$(CC) -o client client.o $(LIBS)

client.o: client.c gbnpacket.c

server.o: server.c gbnpacket.c

clean:
	rm -f client server client.o server.o
