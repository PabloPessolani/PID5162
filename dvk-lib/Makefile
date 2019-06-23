all: stub_dvkcall.o libdvklib.so libuserdvk.so

# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .
 
OBJS =  stub_dvkcall.o   

# Options for development
CFLAGS =  -g -c -fPIC -Wall -pthread  -U CONFIG_UML_USER -U CONFIG_UML_DVK

stub_dvkcall.o: stub_dvkcall.c ../include/com/stub_dvkcall.h 
	$(CC) -g -shared -pthread -o stub_dvkcall.o  stub_dvkcall.c

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

libdvklib.so: $(OBJS) 
	$(CC) -g -shared -pthread -o libdvklib.so $(OBJS)  $(LIBS)
	

################### FOR UML USER PROCESSESS #############################
OBJS3 =  stub_userdvk.o   
CFLAGS3 =  -g -c -fPIC -Wall -pthread -D CONFIG_UML_USER -U CONFIG_UML_DVK

$(OBJS3): stub_dvkcall.c ../include/com/stub_dvkcall.h 
	$(CC) $(CFLAGS3) -pthread -o $(OBJS3) stub_dvkcall.c

libuserdvk.so: $(OBJS3) 
	$(CC) -g -shared -pthread -o libuserdvk.so $(OBJS3)  $(LIBS)
	
clean:
	rm  *.o *.so

.PHONY: all clean 
.SECONDARY:
