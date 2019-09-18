all: inet test_inet
# Which compiler
CC = gcc

# Where are include files kept
INCLUDE = ..
g = generic
m = minix3

# Options for develoinetent
# CFLAGS = -v -O3 -Wall -ansi

OBJS = inet.o sha2.o mq.o buf.o clock.o qp.o sr.o inet_config.o mnx_eth.o \
		$g/rand256.o $g/event.o $g/eth.o $g/arp.o $g/psip.o $g/ip.o \
		$g/tcp.o $g/udp.o $g/io.o  $g/ip_ioctl.o $g/ip_ps.o $g/icmp.o \
		$g/ip_eth.o $g/ipr.o $g/ip_write.o $g/ip_read.o $g/tcp_recv.o \
		$g/tcp_send.o $g/tcp_lib.o $g/ip_lib.o $m/queryparam.o $m/oneC_sum.o 

LIBS = 	/usr/src/dvs/vos/mol/lib/syslib/libsyslib.so \
		/usr/src/dvs/vos/mol/lib/timers/libtimers.so \
		/usr/src/dvs/vos/mol/lib/mollib/libmollib.so \
		/usr/src/dvs/dvk-lib/stub_dvkcall.o 
		
inet: $(OBJS)    
	$(CC)  $(CFLAGS) -MMD -MP -ldl -pthread -o inet $(OBJS) $(LIBS) -I$(INCLUDE)
	
test_inet: test_inet.c    
	$(CC)  $(CFLAGS) -MMD -MP -ldl -pthread -o test_inet  test_inet.c $m/oneC_sum.o $(LIBS) -I$(INCLUDE)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $< -I$(INCLUDE)

clean:
	rm inet test_inet $(OBJS)
