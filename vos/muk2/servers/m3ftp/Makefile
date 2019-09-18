all: libftp.so m3ftp
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi
CFLAGS =  -g -c -fPIC -Wall -pthread  -v -O0 -D_FILE_OFFSET_BITS=64

# OBJS = is.o table.o utility.o open.o read.o write.o mount.o path.o super.o link.o inode.o misc.o cache.o dmap.o device.o \
# 	../../stub_syscall.o

#Objetos a compilar
OBJS = m3ftpd.o 
OBJ2 =  m3ftp.o 


LIBS = 	/usr/src/dvs/vos/muk/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/muk/lib/timers/libtimers.so \
		/usr/src/dvs/vos/muk/lib/mollib/libmollib.so \
		/usr/src/dvs/dvk-lib/stub_dvkcall.o 
	
m3ftp: $(OBJ2)  m3ftp.h 
	$(CC)  -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o m3ftp $(OBJ2) $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall
	
libftp.so: $(OBJS) 
	$(CC) -g -shared -pthread -o libftp.so $(OBJS)  	

clean:
	rm libftp.so m3ftp *.o