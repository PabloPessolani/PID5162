all: dsp_proxy 
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

DSPOBJ =  dsp_proxy.o dsp_config.o 
 
#LIBS = ../../lib/syslib/libsyslib.so ../../lib/timers/libtimers.so

LIBS = 	/usr/src/dvs/dvk-lib/stub_dvkcall.o \
		/usr/local/lib/libspread.a \
		/usr/src/dvs/lib/config/libconfig.so \
		/usr/local/lib/libspread-util.a 
	
LZ4FLAGS = $(CFLAGS)  
LZ4FLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes

LZ4DFLAGS = $(DFLAGS)  
LZ4DFLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes


LZ4LIBS = $(LIBS) 
LZ4LIBS += 	 ../../lz4-1.8.2/lib/lz4frame.o \
			 ../../lz4-1.8.2/lib/lz4.o \
			 ../../lz4-1.8.2/lib/lz4hc.o \
			 ../../lz4-1.8.2/lib/xxhash.o
			 
dsp_proxy: $(DSPOBJ)   
	$(CC)  $(LZ4FLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o dsp_proxy $(DSPOBJ) $(LZ4LIBS) -I$(INCLUDE)	

.c.o:
	$(CC) $(LZ4FLAGS) -c -o $@ $< -I$(INCLUDE) -Wall

clean:
	rm dsp_proxy *.o
