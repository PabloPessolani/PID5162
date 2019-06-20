all: stub_dvkcall.o stub_umldvk.o stub_userdvk.o libdvklib.so libumldvk.so libuserdvk.so

# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = .
 

################### FOR STANDAR DVK PROCESSESS #############################
OBJS1 =  stub_dvkcall.o   
CFLAGS1 =  -g -c -fPIC -Wall -pthread -D CONFIG_DVS_DVK -U CONFIG_UML_DVK -U CONFIG_UML_USER

$(OBJS1): stub_dvkcall.c ../include/com/stub_dvkcall.h 
	$(CC) $(CFLAGS1) -g -shared -pthread -o  $(OBJS1) stub_dvkcall.c

libdvklib.so: $(OBJS1) 
	$(CC) -g -shared -pthread -o libdvklib.so $(OBJS1)  $(LIBS)
	
	
################### FOR UML KERNEL PROCESSESS #############################
OBJS2 =  stub_umldvk.o   
CFLAGS2 =  -g -c -fPIC -Wall -pthread  -U CONFIG_UML_USER

$(OBJS2): stub_dvkcall.c ../include/com/stub_dvkcall.h 
	$(CC) $(CFLAGS2)  -g -shared -pthread -o $(OBJS2) stub_dvkcall.c

libumldvk.so: $(OBJS2) 
	$(CC) -g -shared -pthread -o libumldvk.so $(OBJS2)  $(LIBS)	
	
################### FOR UML USER PROCESSESS #############################
OBJS3 =  stub_userdvk.o   
CFLAGS3 =  -g -c -fPIC -Wall -pthread -D CONFIG_DVS_DVK -D CONFIG_UML_USER

$(OBJS3): stub_dvkcall.c ../include/com/stub_dvkcall.h 
	$(CC) $(CFLAGS3)  -g -shared -pthread -o $(OBJS3) stub_dvkcall.c

libuserdvk.so: $(OBJS3) 
	$(CC) -g -shared -pthread -o libuserdvk.so $(OBJS3)  $(LIBS)
	

#.c.o:
#$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)


	
clean:
	rm  *.o *.so

.PHONY: all clean 
.SECONDARY:

