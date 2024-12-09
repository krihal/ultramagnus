C=gcc
CFLAGS=-g -Wall -lpthread

CLIENT_SRC = client.c
SERVER_SRC = server.c dict.c
TEST_SRC = test.c dict.c

CLIENT_OBJ = $(CLIENT_SRC:%.c=%.o)
SERVER_OBJ = $(SERVER_SRC:%.c=%.o)
TEST_OBJ = $(TEST_SRC:%.c=%.o)

default: client server test

client: $(CLIENT_OBJ)
	$(CC) -Wall $(CFLAGS) -o $@ -lc $^

server: $(SERVER_OBJ)
	$(CC) -Wall $(CFLAGS) -o $@ -lc $^

test: $(TEST_OBJ)
	$(CC) -Wall $(CFLAGS) -o $@ -lc $^
clean:
	rm -f *.o client server
