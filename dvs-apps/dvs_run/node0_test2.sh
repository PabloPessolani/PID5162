#!/bin/bash 
#	BEFORE:		NODE0: Void
#				NODE1: Client, Server
#	AFTER:		NODE0: Server
#				NODE1: Client
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# This script was invoked from node1_test2.sh running in NODE1 
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# necesito setear la variable entorno DC0 con el PID del proceso que creo el namespace llamado dc_init
. /dev/shm/DC0.sh    
echo DC0=$DC0 > /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log 

# le dice al DVK del NODE0 que tanto el cliente como el servidor estan ejecutando en NODE1
cd /usr/src/dvs/dvk-tests/
nsenter -p -t$DC0 ./test_rmtbind 0 10 1 migr_server >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log 
nsenter -p -t$DC0 ./test_rmtbind 0 11 1 migr_client >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log
cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log

# le dice al DVK del NODE0 que el server esta por migrar
startMigr=`date +%s%N`
echo startMigr=$startMigr >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log
cd /usr/src/dvs/dvs-apps/dvs_run
./dvs_migrate -s 0 10 >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log
cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log

# se hace el restart del server en el entorno del DC0 que internamente le notifica de la migracion exitosa al DVK local
# nsenter -p -t$DC0 
stat --printf="SERVER checkpoint image file size=%s\n" checkpoint_migr_server.dmtcp >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log
bash -c "/usr/local/bin/dmtcp_restart checkpoint_migr_server.dmtcp > /usr/src/dvs/dvs-apps/dvs_run/node0_test2_server.log &"
endMigr=`date +%s%N`
echo endMigr=$endMigr >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log
echo Migration time was `expr $endMigr - $startMigr` nanoseconds >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log
cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node0_test2.log
exit


