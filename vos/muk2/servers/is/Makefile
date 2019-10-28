all: libis.a

LIB=libis.a 


# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi
CFLAGS =  -static -g -c -fPIC -Wall -lpthread  -v -O0 -D_FILE_OFFSET_BITS=64

# OBJS = is.o table.o utility.o open.o read.o write.o mount.o path.o super.o link.o inode.o misc.o cache.o dmap.o device.o \
# 	../../stub_syscall.o

#Objetos a compilar
OBJS = is.o 
# dmp.o dmp_fs.o dmp_pm.o dm_kernel.o
		
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

	
$(LIB): $(OBJS) 
	ar rvc $(LIB) $(OBJS) 
	
clean:
	rm $(LIB)  *.o
