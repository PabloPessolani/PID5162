CC=gcc
BINS= 	dvs_run test_dvs_run
			
INCLUDE = ..
		
#LIBS = 	/usr/src/dvs/dvk-lib/libdvkipc.so 
LIBS = 	/usr/src/dvs/dvk-lib/stub_dvkcall.o

FLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic

all: $(BINS)

OBJS =  dvs_run.o 
OBJ2 =  test_dvs_run.o

dvs_run: $(OBJS) dvs_run.h
	$(CC)  $(CFLAGS) -o dvs_run $(OBJS) $(LIBS) -I$(INCLUDE)

test_dvs_run: $(OBJ2) dvs_run.h
	$(CC)  $(CFLAGS) -o test_dvs_run $(OBJ2) $(LIBS) -I$(INCLUDE)
	
clean:
	rm -f $(BINS)  $(OBJS) $(OBJ2)


