all: muk2

LIB= 	/usr/src/dvs/lib/config/libconfig.a \
		/usr/src/dvs/dvk-lib/stub_dvkipc.o \
		/usr/src/dvs/vos/muk2/servers/m3ftp/libftp.a \
		/usr/src/dvs/vos/muk2/servers/m3nweb/libnw.a \
		/usr/src/dvs/vos/muk2/servers/is/libis.a \
		/usr/src/dvs/vos/muk2/servers/fs/libfs.a \
		/usr/src/dvs/vos/muk2/servers/pm/libpm.a \
		/usr/src/dvs/vos/muk2/tasks/rdisk/librdisk.a \
		/usr/src/dvs/vos/muk2/tasks/systask/libsystask.a \
		/usr/src/dvs/vos/muk2/lib/mollib/libmollib.a \
		/usr/src/dvs/vos/muk2/lib/syslib/libsyslib.a \
		/usr/src/dvs/vos/muk2/lib/timers/libtimers.a \
		/usr/src/dvs/vos/muk2/lib/libtask/libtask.a
		
AS=gcc -c
CC=gcc
CFLAGS=-Wall -c -I. -ggdb -g -c -fPIC -Wall -pthread 

%.o: %.c
	$(CC) $(CFLAGS) $*.c
	
muk2: muk2.o mukcfg.o 
	$(CC)  -pthread -o muk2 muk2.o mukcfg.o $(LIB)
	
clean:
	rm -f *.o  muk2 


