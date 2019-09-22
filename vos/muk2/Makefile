all: muk2

LIB= 	/usr/src/dvs/lib/config/libconfig.so \
		/usr/src/dvs/dvk-lib/stub_dvkipc.o \
		/usr/src/dvs/vos/muk2/tasks/systask/libsystask.so \
		/usr/src/dvs/vos/muk2/lib/timers/libtimers.so \
		/usr/src/dvs/vos/muk2/tasks/rdisk/librdisk.so \
		/usr/src/dvs/vos/muk2/servers/pm/libpm.so \
		/usr/src/dvs/vos/muk2/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/muk2/lib/mollib/libmollib.so \
		/usr/src/dvs/vos/muk2/lib/libtask/libtask.a

LIBS2 = /usr/src/dvs/dvk-lib/stub_dvkcall.o \
		/usr/src/dvs/vos/muk2/lib/timers/libtimers.so \
		/usr/src/dvs/vos/muk2/servers/m3nweb/libnw.so \
		/usr/src/dvs/vos/muk2/servers/m3ftp/libftp.so \
		/usr/src/dvs/vos/muk2/servers/is/libis.so \
		/usr/src/dvs/vos/muk2/servers/fs/libfs.so \
		/usr/src/dvs/vos/muk2/tasks/rdisk/librdisk.so \
		/usr/src/dvs/lib/config/libconfig.so \
		/usr/src/dvs/vos/muk2/servers/pm/libpm.so \
		/usr/src/dvs/vos/muk2/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/muk2/lib/mollib/libmollib.so \
		/usr/src/dvs/vos/muk2/tasks/systask/libsystask.so 		
		
AS=gcc -c
CC=gcc
CFLAGS=-Wall -c -I. -ggdb -g -c -fPIC -Wall -pthread 

%.o: %.c
	$(CC) $(CFLAGS) $*.c
	
muk2: muk2.o mukcfg.o $(LIB)
	$(CC) -o muk2 muk2.o mukcfg.o $(LIB)
	
clean:
	rm -f *.o  muk2 


