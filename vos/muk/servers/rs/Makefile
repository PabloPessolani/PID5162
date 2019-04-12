all: librs.so
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for development
CFLAGS =  -g -c -fPIC -Wall -pthread 

OBJS = rs.o table.o utility.o getset.o forkexit.o \
	timers.o signal.o misc.c time.c 
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

	
librs.so: $(OBJS) 
	$(CC) -g -shared -pthread -o librs.so $(OBJS)  $(LIBS)

clean:
	rm librs.so  *.o
