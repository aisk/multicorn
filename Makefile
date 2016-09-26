CC=clang
CFLAGS=$(shell pkg-config libevent python2 --cflags) -Wall
LIBS=$(shell pkg-config libevent python2 --libs)

ngru: ngru.c
	$(CC) -o ngru ngru.c $(CFLAGS) $(LIBS)

clean:
	rm -f ngru
	rm -f *.pyc
