all: libpm.a

LIB=libpm.a 

# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for development
CFLAGS =  -static -g -c -fPIC -Wall -lpthread 

OBJS = pm.o table.o utility.o getset.o forkexit.o \
	timers.o signal.o misc.o time.o dump.o
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

	
$(LIB): $(OBJS) 
	ar rvc $(LIB) $(OBJS)

clean:
	rm $(LIB)  *.o
