all: rhostfs_svr
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for development
# CFLAGS = -v -O3 -Wall -ansi

OBJS = rhostfs_svr.o rhostfs_cfg.o rhostfs_lib.o

LIBS = 	/usr/src/dvs/dvk-lib/libdvkipc.so \
		/usr/src/dvs/lib/config/libconfig.so 
		
rhostfs_svr: $(OBJS)    
	$(CC)  $(CFLAGS) -MMD -MP -ldl -pthread -o rhostfs_svr $(OBJS)  $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

clean:
	rm rhostfs_svr  *.o
