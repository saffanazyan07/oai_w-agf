CC = gcc
CFLAGS = -Wall -g -O2

test_manual: test_manual.o time_source.o time_server.o time_client.o
	$(CC) $(CFLAGS) -o $@ $^ -pthread

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: ../%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f test_manual *.o
