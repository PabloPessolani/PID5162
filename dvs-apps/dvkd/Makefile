all: dd_monitor dd_agent dd_proxy
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

DDMOBJ =  dd_monitor.o 
DDAOBJ =  dd_agent.o
DDPOBJ =  dd_proxy.o
 
#LIBS = ../../lib/syslib/libsyslib.so ../../lib/timers/libtimers.so

LIBS = 	/usr/src/dvs/dvk-lib/stub_dvkcall.o \
		/usr/local/lib/libspread.a \
		/usr/src/dvs/lib/config/libconfig.so \
		/usr/local/lib/libspread-util.a 
	
dd_monitor: $(DDMOBJ)  dd_common.h dd_monitor.h glo_monitor.h
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o dd_monitor $(DDMOBJ) $(LIBS) -I$(INCLUDE)	

dd_agent: $(DDAOBJ)   dd_common.h dd_agent.h glo_agent.h
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o dd_agent $(DDAOBJ) $(LIBS) -I$(INCLUDE)	

dd_proxy: $(DDPOBJ)   dd_common.h dd_proxy.h glo_proxy.h
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o dd_proxy $(DDPOBJ) $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall

clean:
	rm dd_monitor dd_agent dd_proxy *.o
