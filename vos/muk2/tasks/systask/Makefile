all: libsystask.a

LIB=libsystask.a 

# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for development
# CFLAGS = -v -O3 -Wall -ansi
CFLAGS =  -g -c -fPIC -Wall -pthread 

OBJS =  system.o clock.o  \
		st_unused.o st_times.o st_privctl.o st_nice.o \
		st_getinfo.o st_fork.o st_exit.o st_copy.o st_killed.o  \
		st_setalarm.o st_bindproc.o st_memset.o \
		st_vcopy.o st_dump.o

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

	
$(LIB): $(OBJS) 
	ar rvc $(LIB) $(OBJS) 

clean:
	rm  $(LIB) *.o
