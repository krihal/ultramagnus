C=gcc
CFLAGS=-g -Wall -lpthread

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

client: client.o
	$(CC) -o client client.o

clean:
	rm -f *.o client
