CC=gcc
CFLAGS=-Wall -Wextra -O2 -pthread
LIBS=-lzmq

all:
	$(CC) $(CFLAGS) server.c -o server $(LIBS)
	$(CC) $(CFLAGS) client.c -o client $(LIBS)

clean:
	rm -f server client
