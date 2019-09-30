all: libfs.a

LIB=libfs.a 


# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi
CFLAGS =  -g -c -fPIC -Wall -pthread  -v -O0 -D_FILE_OFFSET_BITS=64

# OBJS = fs.o table.o utility.o open.o read.o write.o mount.o path.o super.o link.o inode.o misc.o cache.o dmap.o device.o \
# 	../../stub_syscall.o

#Objetos a compilar
 OBJS = fs.o table.o utility.o open.o read.o write.o filedes.o path.o protect.o \
		super.o link.o inode.o misc.o cache.o dmap.o device.o pipe.o stadir.o lock.o \
		time.o mount.o linux_util.o moldevcfg.o dump.o 
		
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

	
$(LIB): $(OBJS) 
	ar rvc $(LIB) $(OBJS)
	
clean:
	rm $(LIB)  *.o
