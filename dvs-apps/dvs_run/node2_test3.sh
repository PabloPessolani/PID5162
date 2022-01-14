#!/bin/bash
#	BEFORE:		NODE0: Client
#				NODE1: Server
#				NODE2: Void
#	AFTER:		NODE0: Client
#				NODE1: Void 
#				NODE2: Server
#### Run this script in NODE1
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# This script was invoked from node1_test3.sh running in NODE1 
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# necesito setear la variable entorno DC0 con el PID del proceso que creo el namespace llamado dc_init
. /dev/shm/DC0.sh    
echo DC0=$DC0 > /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log 
cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log

# le dice al DVK del NODE0 que el server esta por migrar
cd /usr/src/dvs/dvk-tests/
startMigr=`date +%s%N`
echo startMigr=$startMigr >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log
cd /usr/src/dvs/dvs-apps/dvs_run
./dvs_migrate -s 0 10 >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log
cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log

# se hace el restart del server en el entorno del DC0 que internamente le notifica de la migracion exitosa al DVK local
# nsenter -p -t$DC0 
chown root:root checkpoint_migr_server.dmtcp
stat --printf="SERVER checkpoint image file size=%s\n" checkpoint_migr_server.dmtcp >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log
bash -c "/usr/local/bin/dmtcp_restart checkpoint_migr_server.dmtcp > /usr/src/dvs/dvs-apps/dvs_run/node2_test3_server.log &"
#./dvs_migrate -c 0 10 >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log
endMigr=`date +%s%N`
echo endMigr=$endMigr >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log
echo Migration time was `expr $endMigr - $startMigr` nanoseconds >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log
cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3.log
exit


