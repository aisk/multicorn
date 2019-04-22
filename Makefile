CFLAGS=$(shell pkg-config libevent python3 --cflags) -Wall
LIBS=$(shell pkg-config libevent python3 --libs)

sonia: sonia.c http_message.c
	$(CC) -o sonia $^ -g $(CFLAGS) $(LIBS)

clean:
	rm -f sonia
	rm -f *.pyc
