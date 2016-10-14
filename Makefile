CC=gcc
LD=gcc

CFLAGS=-Wall -O2 -std=c99 -D_POSIX_C_SOURCE=200112L -D_DEFAULT_SOURCE -g

LDFLAGS=-lpthread

pixelflut: pixelflut_g
	strip $< -o $@

pixelflut_g: main.o
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f pixelflut pixelflut_g *.o
