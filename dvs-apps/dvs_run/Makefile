CC=gcc
BINS= 	dvs_run test_dvs_run migr_server migr_client dvs_migrate
			
INCLUDE = ..
		
#LIBS = 	/usr/src/dvs/dvk-lib/libdvkipc.so 
LIBS = 	/usr/src/dvs/dvk-lib/stub_dvkcall.o

FLAGS = -MMD -MP -ldl -lm -lrt -lnsl -pthread -rdynamic

all: $(BINS)

OBJ1 =  dvs_run.o 
OBJ2 =  test_dvs_run.o
OBJ3 =  migr_server.o
OBJ4 =  migr_client.o
OBJ5 =  dvs_migrate.o

dvs_run: $(OBJ1) dvs_run.h
	$(CC)  $(CFLAGS) -o dvs_run $(OBJ1) $(LIBS) -I$(INCLUDE)

test_dvs_run: $(OBJ2) dvs_run.h
	$(CC)  $(CFLAGS) -o test_dvs_run $(OBJ2) $(LIBS) -I$(INCLUDE)

migr_server: $(OBJ3) dvs_run.h
	$(CC)  $(CFLAGS) -o migr_server $(OBJ3) $(LIBS) -I$(INCLUDE)

migr_client: $(OBJ4) dvs_run.h
	$(CC)  $(CFLAGS) -o migr_client $(OBJ4) $(LIBS) -I$(INCLUDE)

dvs_migrate: $(OBJ5) dvs_run.h
	$(CC)  $(CFLAGS) -o dvs_migrate $(OBJ5) $(LIBS) -I$(INCLUDE)
	
clean:
	rm -f $(BINS)  $(OBJ1) $(OBJ2) $(OBJ3) $(OBJ4) $(OBJ5)


