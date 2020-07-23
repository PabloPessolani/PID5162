CC=gcc
BINS= loop_dvk_ipc1 loop_dvk_ipc2 loop_dvk_ipc3 loop_dvk_ipc4 \
	loop_sr_mpeer loop_rr_mpeer loop_dvk_copy1

all: $(BINS)
		
INCLUDE = ..
LIBS = ../dvk-lib/stub_dvkcall.o
#LIBS = ../dvk-lib/libdvkipc.so

CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic
	
loop_dvk_ipc1: loop_dvk_ipc1.c 
	$(CC) $(FLAGS) -o loop_dvk_ipc1 loop_dvk_ipc1.c  $(LIBS) -I$(INCLUDE)

loop_dvk_ipc2: loop_dvk_ipc2.c 
	$(CC) $(FLAGS) -o loop_dvk_ipc2 loop_dvk_ipc2.c  $(LIBS) -I$(INCLUDE)

loop_dvk_ipc3: loop_dvk_ipc3.c 
	$(CC) $(FLAGS) -o loop_dvk_ipc3 loop_dvk_ipc3.c  $(LIBS) -I$(INCLUDE)

loop_dvk_ipc4: loop_dvk_ipc4.c 
	$(CC) $(FLAGS) -o loop_dvk_ipc4 loop_dvk_ipc4.c  $(LIBS) -I$(INCLUDE)

loop_sr_mpeer: loop_sr_mpeer.c 
	$(CC) $(FLAGS) -o loop_sr_mpeer loop_sr_mpeer.c  $(LIBS) -I$(INCLUDE)

loop_rr_mpeer: loop_rr_mpeer.c 
	$(CC) $(FLAGS) -o loop_rr_mpeer loop_rr_mpeer.c  $(LIBS) -I$(INCLUDE)

loop_dvk_copy1: loop_dvk_copy1.c 
	$(CC) $(FLAGS) -o loop_dvk_copy1 loop_dvk_copy1.c  $(LIBS) -I$(INCLUDE)
	
clean:
	rm -f $(BINS)
