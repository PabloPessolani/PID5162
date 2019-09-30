all: libftp.a 

LIB=libftp.a 

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
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall
	
$(LIB): $(OBJS) 
	ar rvc $(LIB) $(OBJS)  	

clean:
	rm $(LIB)  *.o