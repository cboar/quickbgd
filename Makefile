CC=gcc
CFLAGS=-O2 -Wall
LDFLAGS=-Wl,--no-as-needed -lrt

all: quickbgd quickbg

quickbgd:
	$(CC) $(CFLAGS) $(LDFLAGS) -lX11 -lImlib2 -lXinerama -o quickbgd quickbgd.c

quickbg:
	$(CC) $(CFLAGS) $(LDFLAGS) -o quickbg quickbg.c

clean:
	rm -f quickbg quickbgd
