all: muk2
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for development
# CFLAGS = -v -O3 -Wall -ansi

OBJS = muk2.o 

#	/usr/src/dvs/vos/mol/lib/syslib/libsyslib.so \
#	/usr/src/dvs/vos/mol/lib/timers/libtimers.so \
#	/usr/src/dvs/vos/mol/lib/mollib/libmollib.so \	

LIBS = 
		
muk2: $(OBJS)    
	$(CC)  $(CFLAGS) -MMD -MP -ldl -pthread -o muk2 $(OBJS) $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

clean:
	rm muk2  *.o
