CC=gcc
BINS= 	loop_r-s_server loop_sr_client loop_copy_server loop_copy_client \
		copy_client copy_server uml_server uml_client 

# loop_clt_vcopy3 \
#		loop_clt_vcopy3a loop_clt_vcopy3b loop_clt_vcopy4 \
#		loop_svr_vcopy1 loop_svr_vcopy2 loop_svr_vcopy3 \
#		loop_svr_vcopy3a loop_svr_vcopy3b loop_svr_vcopy4		
	
INCLUDE = ..
#LIBS = ../../dvk-lib/stub_dvkcall.o
LIBS = ../../dvk-lib/libdvkipc.so
LIBUML = ../../dvk-lib/libuserdvk.so
CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic
		
all: $(BINS)
	
loop_r-s_server: loop_r-s_server.c 
	$(CC)  $(CFLAGS)  -o loop_r-s_server loop_r-s_server.c $(LIBS) -I$(INCLUDE)

loop_sr_client: loop_sr_client.c 
	$(CC)  $(CFLAGS)  -o loop_sr_client loop_sr_client.c $(LIBS) -I$(INCLUDE)

loop_copy_server: loop_copy_server.c 
	$(CC)  $(CFLAGS)  -o loop_copy_server loop_copy_server.c $(LIBS) -I$(INCLUDE)

loop_copy_client: loop_copy_client.c 
	$(CC)  $(CFLAGS)  -o loop_copy_client loop_copy_client.c $(LIBS) -I$(INCLUDE)

copy_server: copy_server.c 
	$(CC)  $(CFLAGS)  -o copy_server copy_server.c $(LIBS) -I$(INCLUDE)

copy_client: copy_client.c 
	$(CC)  $(CFLAGS)  -o copy_client copy_client.c $(LIBS) -I$(INCLUDE)

uml_server: copy_server.c 
	$(CC)  $(CFLAGS)  -o uml_server copy_server.c $(LIBUML) -I$(INCLUDE)

uml_client: copy_client.c 
	$(CC)  $(CFLAGS)  -o uml_client copy_client.c $(LIBUML) -I$(INCLUDE)
	
clean:
	rm -f $(BINS)
