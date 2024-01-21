CC = gcc
CFLAGS = -Wall
LDFLAGS = -lpulse -lfftw3 -lraylib -lm

mspirit: main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f mspirit

run: mspirit
	./mspirit

install: mspirit
	cp mspirit /usr/local/bin

all: mspirit