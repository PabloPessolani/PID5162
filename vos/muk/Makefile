all: muk
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for development
# CFLAGS = -v -O3 -Wall -ansi

OBJS = muk.o mukcfg.o

#	/usr/src/dvs/vos/mol/lib/syslib/libsyslib.so \
#	/usr/src/dvs/vos/mol/lib/timers/libtimers.so \
#	/usr/src/dvs/vos/mol/lib/mollib/libmollib.so \	

LIBS = 	/usr/src/dvs/dvk-lib/stub_dvkcall.o \
		/usr/src/dvs/vos/mol/lib/timers/libtimers.so \
		/usr/src/dvs/vos/muk/servers/m3nweb/libnw.so \
		/usr/src/dvs/vos/muk/servers/m3ftp/libftp.so \
		/usr/src/dvs/vos/muk/servers/is/libis.so \
		/usr/src/dvs/vos/muk/servers/fs/libfs.so \
		/usr/src/dvs/vos/muk/tasks/rdisk/librdisk.so \
		/usr/src/dvs/lib/config/libconfig.so \
		/usr/src/dvs/vos/muk/servers/pm/libpm.so \
		/usr/src/dvs/vos/muk/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/muk/lib/mollib/libmollib.so \
		/usr/src/dvs/vos/muk/tasks/systask/libsystask.so 
		
muk: $(OBJS)    
	$(CC)  $(CFLAGS) -MMD -MP -ldl -pthread -o muk $(OBJS)  $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

clean:
	rm muk  *.o
