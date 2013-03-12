CC=clang
CFLAGS=-Wall
LIBS=-lpython2.7 -levent

build:
	$(CC) -o ngru ngru.c $(LIBS)

clean:
	rm -f ngru
	rm -f *.pyc
