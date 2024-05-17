all: main fs.o

main: fs.o disk.o
	gcc -o web -g $^ webserv.c

fs: disk.o fs.o
	gcc -o $@ -g $^

disk.o:
	gcc -o disk.o -g -c disk.c
