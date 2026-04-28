CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: server client

server: shell_server.c shell_common.h
	$(CC) $(CFLAGS) -o server shell_server.c

client: shell_client.c shell_common.h
	$(CC) $(CFLAGS) -o client shell_client.c

clean:
	rm -f server client
