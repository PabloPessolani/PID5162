all: m3ftpd m3ftp 
# Which compiler
CC = gcc

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi
CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic

OBJ1 =  m3ftpd.o 
OBJ2 =  m3ftp.o 

#LIBS = ../../../dvk-lib/stub_dvkcall.o
LIBS = ../../../dvk-lib/libdvkipc.so

m3ftpd: $(OBJ1)  m3ftp.h 
	$(CC)  $(CFLAGS) -o m3ftpd $(OBJ1) $(LIBS) 	

m3ftp: $(OBJ2)  m3ftp.h 
	$(CC)  $(CFLAGS) -o m3ftp $(OBJ2) $(LIBS) 
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall

clean:
	rm m3ftpd m3ftp *.o
