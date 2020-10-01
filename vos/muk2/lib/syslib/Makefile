all: libsyslib.a 

LIB=libsyslib.a 

# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .
 

# Options for development
CFLAGS =  -static -g -c -fPIC -Wall -lpthread 

OBJS =  taskcall.o sys_times.o sys_setalarm.o sys_fork.o sys_exit.o \
	sys_privctl.o sys_getinfo.o sys_bindproc.o sys_vircopy.o sys_getuptime.o \
	sys_memset.o sys_migrproc.o sys_rexec.o sys_setpname.o sys_virvcopy.o 
	
$(LIB): $(OBJS) 
	ar rvc $(LIB) $(OBJS)  

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)


clean:
	rm $(LIB) *.o

.PHONY: all clean 
.SECONDARY:

