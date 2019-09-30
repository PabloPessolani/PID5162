all: muk

LIB= 	/usr/src/dvs/lib/config/libconfig.a \
		/usr/src/dvs/dvk-lib/stub_dvkipc.o \
		/usr/src/dvs/vos/muk/servers/m3ftp/libftp.a \
		/usr/src/dvs/vos/muk/servers/m3nweb/libnw.a \
		/usr/src/dvs/vos/muk/servers/is/libis.a \
		/usr/src/dvs/vos/muk/servers/fs/libfs.a \
		/usr/src/dvs/vos/muk/servers/pm/libpm.a \
		/usr/src/dvs/vos/muk/tasks/rdisk/librdisk.a \
		/usr/src/dvs/vos/muk/tasks/systask/libsystask.a \
		/usr/src/dvs/vos/muk/lib/mollib/libmollib.a \
		/usr/src/dvs/vos/muk/lib/syslib/libsyslib.a \
		/usr/src/dvs/vos/muk/lib/timers/libtimers.a \
		
AS=gcc -c
CC=gcc
CFLAGS=-Wall -c -I. -ggdb -g -c -fPIC -Wall -pthread 

%.o: %.c
	$(CC) $(CFLAGS) $*.c
	
muk: muk.o mukcfg.o 
	$(CC)  -pthread -o muk muk.o mukcfg.o $(LIB)
	
clean:
	rm -f *.o  muk 


