all: libpm.so
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for development
CFLAGS =  -g -c -fPIC -Wall -pthread 

OBJS = pm.o table.o utility.o getset.o forkexit.o \
	timers.o signal.o misc.c time.c dump.c
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

	
libpm.so: $(OBJS) 
	$(CC) -g -shared -pthread -o libpm.so $(OBJS)  $(LIBS)

clean:
	rm libpm.so  *.o
