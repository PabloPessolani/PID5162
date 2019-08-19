all: rhostfsd
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for development
# CFLAGS = -v -O3 -Wall -ansi

OBJS = rhostfs_svr.o rhostfs_cfg.o rhostfs_lib.o

LIBS = 	/usr/src/dvs/dvk-lib/libdvkipc.so \
		/usr/src/dvs/lib/config/libconfig.so 
		
rhostfsd: $(OBJS)    
	$(CC)  $(CFLAGS) -MMD -MP -ldl -pthread -o rhostfsd $(OBJS)  $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

clean:
	rm rhostfsd  *.o
