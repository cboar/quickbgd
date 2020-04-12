CC?=gcc
CFLAGS?=-g -O2 -Wall
LDFLAGS?=-Wl,--no-as-needed -lX11 -lImlib2 -lXinerama -lrt


all: quickbgd

quickbgd: quickbgd.o

clean:
	rm -f *.o quickbgd
