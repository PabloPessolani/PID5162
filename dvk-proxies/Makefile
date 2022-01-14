BINS= tcp_proxy_bat tcp_proxy tipc_proxy lz4tipc_proxy_bat lz4tcp_proxy_bat multi_proxy
#      lz4tcp_th_proxy lz4tipc_proxy  sp_proxy_bat
		
# Which compiler
CC = gcc
C++ = g++

# Where are include files kept
INCLUDE = ..
LIBS = 	/usr/src/dvs/lib/config/libconfig.so \
		../dvk-lib/stub_dvkcall.o
#		../dvk-lib/libdvklib.so \
		
CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic -Wall
DFLAGS = -g -c -fPIC -Wall -pthread

LZ4FLAGS = $(CFLAGS)  
LZ4FLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes

LZ4DFLAGS = $(DFLAGS)  
LZ4DFLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes


LZ4LIBS = $(LIBS) 
LZ4LIBS += 	 ../lz4-1.8.2/lib/lz4frame.o \
			 ../lz4-1.8.2/lib/lz4.o \
			 ../lz4-1.8.2/lib/lz4hc.o \
			 ../lz4-1.8.2/lib/xxhash.o		

SPFLAGS = $(CFLAGS)
SPLIBS  = $(LIBS)
SPLIBS  += 	/usr/local/lib/libspread.a \
			/usr/local/lib/libspread-util.a 			 
	
MPXOBJ =  multi_proxy.o multi_config.o 
	
# Options for development
# CFLAGS = -v -O3 -Wall -ansi

all: $(BINS)
	
multi_proxy: $(MPXOBJ)   
	$(CC)   $(LZ4FLAGS) -o multi_proxy $(MPXOBJ) $(LZ4LIBS) -I$(INCLUDE)	
	
multi_proxy.o: multi_proxy.c multi_proxy.h
	$(CC)   $(LZ4DFLAGS)  -o multi_proxy.o multi_proxy.c $(LZ4LIBS) -I$(INCLUDE)	

multi_config.o: multi_config.c
	$(CC)   $(DFLAGS) -o multi_config.o multi_config.c $(LZ4LIBS) -I$(INCLUDE)
	
tcp_proxy_bat: tcp_proxy_bat.c 
	$(CC)  $(CFLAGS)  -o tcp_proxy_bat tcp_proxy_bat.c $(LIBS) 	-I$(INCLUDE)	

lz4tcp_proxy_bat: lz4tcp_proxy_bat.c 
	$(CC)  $(CFLAGS)  -o lz4tcp_proxy_bat lz4tcp_proxy_bat.c $(LZ4LIBS) -I$(INCLUDE)
	
lz4tipc_proxy_bat: lz4tipc_proxy_bat.c 
	$(CC)  $(CFLAGS)  -o lz4tipc_proxy_bat lz4tipc_proxy_bat.c $(LZ4LIBS) -I$(INCLUDE)	
	
tcp_proxy: tcp_proxy.c 
	$(CC)  $(CFLAGS)  -o tcp_proxy tcp_proxy.c $(LIBS) 	-I$(INCLUDE)	
	
tipc_proxy: tipc_proxy.c 
	$(CC)  $(CFLAGS)  -o tipc_proxy tipc_proxy.c $(LIBS) 	-I$(INCLUDE)	

lz4tcp_th_proxy: lz4tcp_th_proxy.c 
	$(CC)  $(LZ4FLAGS)  -o lz4tcp_th_proxy lz4tcp_th_proxy.c $(LZ4LIBS) -I$(INCLUDE)	 
	
lz4tipc_proxy: lz4tipc_proxy.c 
	$(CC)  $(LZ4FLAGS)  -o lz4tipc_proxy lz4tipc_proxy.c $(LZ4LIBS)  -I$(INCLUDE)	

sp_proxy_bat: sp_proxy_bat.c 
	$(CC)  $(SPFLAGS)  -o sp_proxy_bat sp_proxy_bat.c $(SPLIBS)  -I$(INCLUDE)	
	
clean:
	rm $(BINS) *.o
