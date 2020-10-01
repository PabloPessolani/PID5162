all: libnw.a

LIB=libnw.a 

# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

CFLAGS =  -g -c -fPIC -Wall -pthread 

#LZ4FLAGS = $(CFLAGS)  
#LZ4FLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes

OBJS = m3nweb.o m3nwebcfg.o

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)
	
$(LIB): $(OBJS) 
	ar rvc $(LIB) $(OBJS)  
	
clean:
	rm $(LIB)  *.o
