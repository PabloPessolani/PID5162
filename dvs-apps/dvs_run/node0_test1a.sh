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
#/usr/bin/killall rsync 
rm -rf /var/run/rsyncd.pid
/usr/bin/rsync --daemon
#sleep 2
echo DC0=$DC0 > /usr/src/dvs/dvs-apps/dvs_run/node0_test1a.log 
ps -ef | grep rsync >> /usr/src/dvs/dvs-apps/dvs_run/node0_test1a.log 
cat /var/run/rsyncd.pid >> /usr/src/dvs/dvs-apps/dvs_run/node0_test1a.log
rm /usr/src/dvs/dvs-apps/dvs_run/*.dmtcp /usr/src/dvs/dvs-apps/dvs_run/*.dmtcp1
cd /usr/src/dvs/dvk-tests
./test_rmtbind 0 10 1 migr_server >> /usr/src/dvs/dvs-apps/dvs_run/node0_test1a.log
cd /usr/src/dvs/dvs-apps/dvs_run
nsenter -p -t$DC0 ./migr_client 0 11 10 4096 100 1 > /usr/src/dvs/dvs-apps/dvs_run/node0_test1_client.log &
exit


