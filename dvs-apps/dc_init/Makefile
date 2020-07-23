CC=gcc
BINS= 	dc_init
			
INCLUDE = ..
		
LIBS = 	/usr/local/lib/libspread.a \
		/usr/src/dvs/lib/config/libconfig.so \
		/usr/local/lib/libspread-util.a \
        /usr/src/dvs/dvk-lib/stub_dvkcall.o
#		/usr/src/dvs/dvk-lib/libdvkipc.so 

#LIBS = ../dvk-lib/libdvklib.so
CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic

all: $(BINS)

OBJS =  dc_read_config.o tracker.o dc_init.o

dc_init: $(OBJS)
	$(CC)  $(CFLAGS) -o dc_init $(OBJS) $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE) -Wall
	
clean:
	rm -f $(BINS)  $(OBJS)


