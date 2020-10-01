all: ph_static 

		
AS=gcc -c
CC=gcc
CFLAGS=-Wall -c -I. -ggdb -g -c -fPIC -Wall -lpthread -static

%.o: %.c
	$(CC) $(CFLAGS) $*.c
	
ph_static: ph_static.o 
	$(CC)  -static -o ph_static ph_static.o -lpthread
	
clean:
	rm -f *.o  ph_static 


