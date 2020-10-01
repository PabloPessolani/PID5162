all: libtimers.a 

LIB=libtimers.a 

# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for development
CFLAGS =  -g -c -fPIC -Wall -pthread 
OBJS =  timers.o 

$(LIB): $(OBJS)
	ar rvc $(LIB) $(OBJS)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

clean:
	rm libtimers.a *.o

.PHONY: all clean 
.SECONDARY:





