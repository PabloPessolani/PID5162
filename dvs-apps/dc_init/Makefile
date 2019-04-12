CC=gcc
BINS= 	dc_init
			
INCLUDE = ..
LIBS = 	/usr/src/dvs/dvk-lib/stub_dvkcall.o \
		/usr/src/dvs/lib/config/libconfig.so 

#LIBS = ../dvk-lib/libdvklib.so
CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic

all: $(BINS)

OBJS =  dc_read_config.o 

dc_init: dc_init.c $(OBJS)
	$(CC)  $(CFLAGS) -o dc_init dc_init.c $(OBJS) $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall
	
clean:
	rm -f $(BINS)  $(OBJS)


