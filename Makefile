CC = gcc
CFLAGS = "-Wall"
STATIC = -static -DSTATIC


all: sockjmp.c
	$(CC) $(CFLAGS) sockjmp.c -o sockjmp

debug: sockjmp.c
	$(CC) $(CFLAGS) -DDEBUG sockjmp.c -o sockjmp

static: sockjmp.c
	$(CC) $(CFLAGS) $(STATIC) -o sockjmp sockjmp.c

clean:
	rm sockjmp

install:
	cp sockjmp /usr/local/bin/
	chmod 755 /usr/local/bin/sockjmp

