CC = gcc
CFLAGS = "-Wall"

all: sockjmp.c
	$(CC) $(CFLAGS) sockjmp.c -o sockjmp

debug: sockjmp.c
	$(CC) $(CFLAGS) -DDEBUG sockjmp.c -o sockjmp

clean:
	rm sockjmp
