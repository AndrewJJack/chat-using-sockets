all: clean client server

clean:
	rm -f *.log
	rm -f server
	rm -f client

client: client.c
	gcc -o client client.c

server: server.c
	gcc -o server server.c
