all: libnw.so
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..

# Options for develofsent
# CFLAGS = -v -O3 -Wall -ansi

CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic
#LZ4FLAGS = $(CFLAGS)  
#LZ4FLAGS += -std=c99 -Wall -Wextra -Wundef -Wshadow -Wcast-align -Wstrict-prototypes

OBJS = m3nweb.o m3nwebcfg.o

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)
	
libnw.so: $(OBJS) 
	$(CC) -g -shared -pthread -o libnw.so $(OBJS)  $(LIBS)
	
clean:
	rm libnw.so  *.o
