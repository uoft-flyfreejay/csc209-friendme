PORT=56916
CFLAGS = -DPORT=$(PORT) -g -Wall -std=gnu99 -Werror

all: friend_server friendme

friend_server: friend_server.o friends.o
	gcc ${CFLAGS} -o $@ $^

friendme: friendme.o friends.o
	gcc ${CFLAGS} -o $@ $^

%.o: %.c friends.h
	gcc ${CFLAGS} -c $<

clean:
	rm -f *.o friend_server friendme friends