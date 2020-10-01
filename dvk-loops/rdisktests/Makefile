CC=gcc
BINS=01_test_devopen 02_test_devwrite 03_test_devread \

		
all: $(BINS)
		
INCLUDE = ..
OBJS = /home/MoL_Module/mol-ipc/stub_syscall.o \
	   	   
LIBS = 	/home/MoL_Module/mol-ipc/lib/syslib/libsyslib.so \
		/home/MoL_Module/mol-ipc/lib/timers/libtimers.so \
		/home/MoL_Module/mol-ipc/lib/mollib/libmollib.so 
CFLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic	

01_test_devopen: 01_test_devopen.c 
	$(CC) $(FLAGS) -o 01_test_devopen 01_test_devopen.c $(OBJS) $(LIBS) -I$(INCLUDE)

02_test_devwrite: 02_test_devwrite.c 
	$(CC) $(FLAGS) -o 02_test_devwrite 02_test_devwrite.c $(OBJS) $(LIBS) -I$(INCLUDE)
	
03_test_devread: 03_test_devread.c 
	$(CC) $(FLAGS) -o 03_test_devread 03_test_devread.c $(OBJS) $(LIBS) -I$(INCLUDE)	
	
clean:
	rm -f $(BINS)