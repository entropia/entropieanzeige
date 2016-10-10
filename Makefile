CC=gcc
LD=gcc

CFLAGS=-Wall -O2 -std=c99 -D_POSIX_C_SOURCE=199309L

LDFLAGS=-lpthread

pixelflut: main.o
	$(LD) $(LDFLAGS) -o pixelflut $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<
