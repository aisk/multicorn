CC=clang
CFLAGS=$(shell pkg-config libevent python --cflags) -Wall
LIBS=$(shell pkg-config libevent python --libs)

ngru: ngru.c
	$(CC) -o ngru ngru.c $(CFLAGS) $(LIBS)

clean:
	rm -f ngru
	rm -f *.pyc
