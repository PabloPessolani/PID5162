all: libtap.a

LIB=libtap.a 


# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for development
# CFLAGS = -v -O3 -Wall -ansi
CFLAGS =  -g -c -fPIC -Wall  -lpthread -static 

OBJS =  tap.o  parse_config.o

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

$(LIB): $(OBJS) 
	ar rvc $(LIB) $(OBJS) 

clean:
	rm  $(LIB) *.o
