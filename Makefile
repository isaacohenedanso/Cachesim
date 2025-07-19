CC=gcc
CFLAGS=-lm -Wall -o

all run: cachesim.c
	$(CC) $(CFLAGS) cachesim cachesim.c
clean:
	rm -rf cachesim

