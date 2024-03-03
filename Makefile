all: server subscriber

server: server.c helper.c
	gcc -g -Wall -o server server.c helper.c

subscriber: subscriber.c helper.c
	gcc -g -Wall -o subscriber subscriber.c helper.c -lm

clean:
	rm -rf subscriber server