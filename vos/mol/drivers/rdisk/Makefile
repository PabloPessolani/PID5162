all: rdisk 
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic
LZ4FLAGS = $(CFLAGS)  
LZ4FLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes


OBJS =  utility.o parse_config.o replicate.o rdisk.o \
		driver.o mastercopy.o md5_compute.o \
		slavecopy.o lz4_data_cd.o

LIBS = 	/usr/src/dvs/vos/mol/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/mol/lib/timers/libtimers.so \
		/usr/src/dvs/vos/mol/lib/mollib/libmollib.so \
		/usr/src/dvs/lib/config/libconfig.so \
		/usr/src/dvs/dvk-lib/stub_dvkcall.o \
		/usr/local/lib/libspread.a \
		/usr/local/lib/libspread-util.a 		

LZ4LIBS = 	/usr/src/dvs/lz4-1.8.2/lib/lz4frame.o \
			/usr/src/dvs/lz4-1.8.2/lib/lz4.o \
			/usr/src/dvs/lz4-1.8.2/lib/lz4hc.o \
			/usr/src/dvs/lz4-1.8.2/lib/xxhash.o				
		
MD5LIBS = 	/usr/src/dvs/md5-1.3.0/libmd5.a
		
rdisk: $(OBJS)
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic -o rdisk $(OBJS) $(LIBS) $(LZ4LIBS) $(MD5LIBS) -I$(INCLUDE)	
		
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall

clean:
	rm rdisk *.o
