CFLAGS=$(shell pkg-config libevent python3 --cflags) -Wall
LIBS=$(shell pkg-config libevent python3 --libs)

ngru: ngru.c
	$(CC) -o ngru ngru.c -g $(CFLAGS) $(LIBS)

clean:
	rm -f ngru
	rm -f *.pyc
