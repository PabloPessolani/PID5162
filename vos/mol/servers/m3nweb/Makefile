all: m3nweb 
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic
#LZ4FLAGS = $(CFLAGS)  
#LZ4FLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes

OBJS = m3nweb.o m3nwebcfg.o
LIBS = 	/usr/src/dvs/vos/mol/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/mol/lib/timers/libtimers.so \
		/usr/src/dvs/vos/mol/lib/mollib/libmollib.so \
		/usr/src/dvs/lib/config/libconfig.so \
		/usr/src/dvs/dvk-lib/stub_dvkcall.o 
	
m3nweb: $(OBJS) 
	$(CC)  $(CFLAGS) -o m3nweb $(OBJS) $(LIBS) -I$(INCLUDE)	
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall

clean:
	rm m3nweb *.o
