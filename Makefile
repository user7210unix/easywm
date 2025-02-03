CC = gcc
CFLAGS = -Wall -O2 -std=c99 -I/usr/include/freetype2
LDFLAGS = -lX11 -lXft -lfontconfig -lXinerama
SRC = easywm.c
BIN = easywm

all: $(BIN)

$(BIN): $(SRC) config.h
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

clean:
	rm -f $(BIN)

install: $(BIN)
	cp $(BIN) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(BIN)
