CC=gcc
BINS= 	test_dvs_init test_dc_init test_bind test_tbind test_unbind vm_read \
		test_sendrcv test_sendrec test_ntfyrcv test_rr_rply  test_rmtbind \
		test_lclvcopy test_wait4bind test_add_node test_priv \
		test_wakeup test_proxy_bind test_node_up test_node_down test_lclvcopy3 \
		uml_test
		
	
INCLUDE = ..
#LIBS = ../dvk-lib/stub_dvkcall.o
LIBS = ../dvk-lib/libdvkipc.so
CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic

all: $(BINS)

vm_read: vm_read.c 
	$(CC)  $(CFLAGS) -o vm_read vm_read.c  $(LIBS) -I$(INCLUDE)
	
test_dvs_init: test_dvs_init.c 
	$(CC)  $(CFLAGS) -o test_dvs_init test_dvs_init.c  $(LIBS) -I$(INCLUDE)

test_dc_init: test_dc_init.c 
	$(CC)  $(CFLAGS) -o test_dc_init test_dc_init.c  $(LIBS) -I$(INCLUDE)
	
test_bind: test_bind.c 
	$(CC)  $(CFLAGS) -o test_bind test_bind.c  $(LIBS) -I$(INCLUDE)

test_rmtbind: test_rmtbind.c 
	$(CC)  $(CFLAGS) -o test_rmtbind test_rmtbind.c  $(LIBS) -I$(INCLUDE)
	
test_tbind: test_tbind.c 
	$(CC)  $(CFLAGS) -o test_tbind test_tbind.c  $(LIBS) -I$(INCLUDE)

test_unbind: test_unbind.c 
	$(CC)  $(CFLAGS) -o test_unbind test_unbind.c  $(LIBS) -I$(INCLUDE)

test_sendrcv: test_sendrcv.c 
	$(CC)  $(CFLAGS) -o test_sendrcv test_sendrcv.c  $(LIBS) -I$(INCLUDE)

test_sendrec: test_sendrec.c 
	$(CC)  $(CFLAGS) -o test_sendrec test_sendrec.c  $(LIBS) -I$(INCLUDE)

test_ntfyrcv: test_ntfyrcv.c 
	$(CC)  $(CFLAGS) -o test_ntfyrcv test_ntfyrcv.c  $(LIBS) -I$(INCLUDE)

test_rr_rply: test_rr_rply.c 
	$(CC)  $(CFLAGS) -o test_rr_rply test_rr_rply.c  $(LIBS) -I$(INCLUDE)

test_lclvcopy: test_lclvcopy.c 
	$(CC)  $(CFLAGS) -o test_lclvcopy test_lclvcopy.c  $(LIBS) -I$(INCLUDE)

test_wait4bind: test_wait4bind.c 
	$(CC)  $(CFLAGS) -o test_wait4bind test_wait4bind.c  $(LIBS) -I$(INCLUDE)

test_add_node: test_add_node.c 
	$(CC)  $(CFLAGS) -o test_add_node test_add_node.c  $(LIBS) -I$(INCLUDE)
	
test_priv: test_priv.c 
	$(CC)  $(CFLAGS) -o test_priv test_priv.c  $(LIBS) -I$(INCLUDE)

test_wakeup: test_wakeup.c 
	$(CC)  $(CFLAGS) -o test_wakeup test_wakeup.c  $(LIBS) -I$(INCLUDE)

test_proxy_bind: test_proxy_bind.c 
	$(CC)  $(CFLAGS) -o test_proxy_bind test_proxy_bind.c  $(LIBS) -I$(INCLUDE)

test_node_up: test_node_up.c 
	$(CC)  $(CFLAGS) -o test_node_up test_node_up.c  $(LIBS) -I$(INCLUDE)

test_node_down: test_node_down.c 
	$(CC)  $(CFLAGS) -o test_node_down test_node_down.c  $(LIBS) -I$(INCLUDE)	

test_lclvcopy3: test_lclvcopy3.c 
	$(CC)  $(CFLAGS) -o test_lclvcopy3 test_lclvcopy3.c  $(LIBS) -I$(INCLUDE)	
	
uml_test: uml_test.c 
	$(CC)  $(CFLAGS) -o uml_test uml_test.c  $(LIBS) -I$(INCLUDE)
	
clean:
	rm -f $(BINS)


