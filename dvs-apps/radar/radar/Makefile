all: radar test_radar
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

OBJS =  radar.o radar_cfg.o
OBJ2 =  test_radar.o 

#LIBS = ../../lib/syslib/libsyslib.so ../../lib/timers/libtimers.so

LIBS = 	/usr/src/dvs/dvk-lib/stub_dvkcall.o \
		/usr/local/lib/libspread.a \
		/usr/src/dvs/lib/config/libconfig.so \
		/usr/local/lib/libspread-util.a 
	
radar: $(OBJS)   
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o radar $(OBJS) $(LIBS) -I$(INCLUDE)	

test_radar: $(OBJ2)   
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o test_radar $(OBJ2) $(LIBS) -I$(INCLUDE)	
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall

clean:
	rm radar test_radar radar_cfg *.o
