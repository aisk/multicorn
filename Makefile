CFLAGS=$(shell pkg-config libevent python3 --cflags) -Wall
LIBS=$(shell pkg-config libevent python3 --libs)

sonia: sonia.c
	$(CC) -o sonia sonia.c -g $(CFLAGS) $(LIBS)

clean:
	rm -f sonia
	rm -f *.pyc
