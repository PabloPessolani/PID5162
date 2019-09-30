all: libconfig.so libconfig.a

# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .
 

# Options for development
CFLAGS =  -g -c -fPIC -Wall -pthread 

OBJS =  configfile.o 

libconfig.so: $(OBJS) 
	$(CC) -g -shared -pthread -o libconfig.so $(OBJS)  

libconfig.a: $(OBJS) 
	ar rvc -o libconfig.a $(OBJS)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)


clean:
	rm libconfig.so libconfig.a *.o

.PHONY: all clean 
.SECONDARY:

