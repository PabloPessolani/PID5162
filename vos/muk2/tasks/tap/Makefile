all: tap 
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic
LZ4FLAGS = $(CFLAGS)  
LZ4FLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes


OBJS =  tap.o

LIBS = 	/usr/src/dvs/vos/mol/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/mol/lib/timers/libtimers.so \
		/usr/src/dvs/vos/mol/lib/mollib/libmollib.so \
		/usr/src/dvs/vos/mol/lib/config/libconfig.so \
		/usr/src/dvs/dvk-lib/stub_dvkcall.o 
	
		
tap: $(OBJS)
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic -o tap $(OBJS) $(LIBS) -I$(INCLUDE)	
		
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall

clean:
	rm tap *.o
