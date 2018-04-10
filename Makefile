all: criserver
chatserver: criserver.c criserver.h
	gcc -o criserver -Wall -I/usr/local/include -L/usr/local/lib criserver.c

