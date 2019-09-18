all: fatFS
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
CFLAGS = -g -v -O0 -Wall
# CFLAGS = -v -O3 -Wall -ansi
# CFLAGS = -g -v -O0 -Wall -D_FILE_OFFSET_BITS=64 

#-MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic

#Objetos a compilar
OBJS = main.o table.o utility.o open.o read.o write.o filedes.o path.o protect.o \
		super.o link.o inode.o misc.o cache.o dmap.o device.o pipe.o stadir.o lock.o \
		time.o mount.o moldevcfg.o ff.o diskio.o \
 	../../stub_syscall.o

LIBS = ../../lib/syslib/libsyslib.so \
		../../lib/timers/libtimers.so \
		/usr/src/dvs/lib/config/libconfig.so \
		../../lib/mollib/libmollib.so \

fatFS: $(OBJS)    
	$(CC)  $(CFLAGS) -MMD -MP -ldl -pthread -o fatFS $(OBJS)  $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

clean:
	rm fatFS *.o