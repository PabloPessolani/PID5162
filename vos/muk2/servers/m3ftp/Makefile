all: libftp.a m3ftp 

LIB=libftp.a 

LIB2= 	/usr/src/dvs/dvk-lib/stub_dvkipc.o \
		/usr/src/dvs/vos/muk2/lib/mollib/libmollib.a \
		/usr/src/dvs/vos/muk2/lib/syslib/libsyslib.a \
		/usr/src/dvs/vos/muk2/lib/timers/libtimers.a 
		
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi
CFLAGS =  -g -c -fPIC -Wall -pthread 
#Objetos a compilar
OBJS = m3ftpd.o 
#OBJ2 =  m3ftp.o 
	
m3ftp: m3ftp.o $(LIB2)
	$(CC) -o m3ftp m3ftp.o $(LIB2)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall
	
$(LIB): $(OBJS) 
	ar rvc $(LIB) $(OBJS)  	

clean:
	rm $(LIB)  *.o