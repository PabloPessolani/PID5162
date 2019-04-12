all: librdisk.so

# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for development
# CFLAGS = -v -O3 -Wall -ansi
CFLAGS =  -g -c -fPIC -Wall -pthread 


OBJS =  rdisk.o utility.o parse_config.o driver.o 

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

librdisk.so: $(OBJS) 
	$(CC) -g -shared -pthread -o librdisk.so $(OBJS) 

clean:
	rm  librdisk.so *.o
