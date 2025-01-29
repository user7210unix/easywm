CC = gcc
CFLAGS = -Wall -O2 -std=c99
LDFLAGS = -lX11

all: easywm

easywm: easywm.c config.h
	$(CC) $(CFLAGS) easywm.c -o easywm $(LDFLAGS)

clean:
	rm -f easywm

