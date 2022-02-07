all: m3ftpd m3ftp mm3ftpd
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

OBJ1 =  m3ftpd.o 
OBJ2 =  m3ftp.o 
OBJ3 =  mm3ftpd.o 

LIBS = 	/usr/src/dvs/vos/mol/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/mol/lib/timers/libtimers.so \
		/usr/src/dvs/vos/mol/lib/mollib/libmollib.so \
		/usr/src/dvs/dvk-lib/stub_dvkcall.o 

m3ftpd: $(OBJ1)  m3ftp.h 
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o m3ftpd $(OBJ1) $(LIBS) -I$(INCLUDE)	

m3ftp: $(OBJ2)  m3ftp.h 
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o m3ftp $(OBJ2) $(LIBS) -I$(INCLUDE)

mm3ftpd: $(OBJ3)  m3ftp.h 
	$(CC)  $(CFLAGS) -MMD -MP -ldl -lm -lrt -lnsl  -pthread -rdynamic -o mm3ftpd $(OBJ3) $(LIBS) -I$(INCLUDE)	
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall

clean:
	rm m3ftpd m3ftp mm3ftpd *.o
