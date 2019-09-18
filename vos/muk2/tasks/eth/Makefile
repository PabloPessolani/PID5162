all: eth taptool rawtool rawftpd rawftp
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

OBJS =  eth.o 
OBJ1 =  taptool.o
OBJ2 =  rawtool.o
OBJ3 =  rawftpd.o
OBJ4 =  rawftp.o


LIBS = 	/usr/src/dvs/vos/mol/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/mol/lib/timers/libtimers.so \
		/usr/src/dvs/vos/mol/lib/mollib/libmollib.so \
		/usr/src/dvs/vos/mol/lib/config/libconfig.so \
		/usr/src/dvs/dvk-lib/stub_dvkcall.o 

eth: $(OBJS)   
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o eth $(OBJS) $(LIBS) -I$(INCLUDE)	

taptool: $(OBJ1)   
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o taptool $(OBJ1) 

rawtool: $(OBJ2)   
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o rawtool $(OBJ2) 

rawftpd: $(OBJ3)   
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o rawftpd $(OBJ3) 

rawftp: $(OBJ4)   
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o rawftp $(OBJ4) 
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall

clean:
	rm eth taptool rawtool rawftpd rawftp *.o
