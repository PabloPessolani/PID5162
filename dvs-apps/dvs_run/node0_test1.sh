#!/bin/bash 
#	BEFORE:		NODE0: Client
#				NODE1: Server
#	AFTER:		NODE0: Client, Server
#				NODE1: Void 
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# This script was invoked from node1_test1.sh running in NODE1 
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# necesito setear la variable entorno DC0 con el PID del proceso que creo el namespace llamado dc_init
. /dev/shm/DC0.sh    
echo DC0=$DC0 > /usr/src/dvs/dvs-apps/dvs_run/node0_test1.log 
cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node0_test1.log
cd /usr/src/dvs/dvs-apps/dvs_run/
chown root:root checkpoint_migr_server.dmtcp
startMigr=`date +%s%N` 
bash -c "/usr/local/bin/dmtcp_restart checkpoint_migr_server.dmtcp > node0_test1_server.log &"
cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node0_test1.log
#./dvs_migrate -s 0 10
#cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node0_test1.log
./dvs_migrate -c 0 10 
# cat /proc/dvs/DC0/procs >> /usr/src/dvs/dvs-apps/dvs_run/node0_test1.log
endMigr=`date +%s%N`  
dmesg > dmesg.txt
echo startMigr=$startMigr >> /usr/src/dvs/dvs-apps/dvs_run/node0_test1.log
echo endMigr=$endMigr >> /usr/src/dvs/dvs-apps/dvs_run/node0_test1.log
echo Migration time was `expr $endMigr - $startMigr` nanoseconds >> node0_test1.log
cat /proc/dvs/DC0/procs >> node0_test1.log
exit


