CC = gcc
CFLAGS = -Wall -g

test: test.o time_source.o time_server.o time_client.o
	$(CC) $(CFLAGS) -o $@ $^ -pthread

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: ../%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f test *.o
