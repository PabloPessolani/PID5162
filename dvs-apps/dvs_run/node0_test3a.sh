#!/bin/bash
#	BEFORE:		NODE0: Client
#				NODE1: Server
#				NODE2: Void
#	AFTER:		NODE0: Client
#				NODE1: Void 
#				NODE2: Server
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# This script was invoked from node1_test3.sh running in NODE1 
#!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
. /dev/shm/DC0.sh    
echo DC0=$DC0 > /usr/src/dvs/dvs-apps/dvs_run/node0_test3a.log 
rm /usr/src/dvs/dvs-apps/dvs_run/*.dmtcp /usr/src/dvs/dvs-apps/dvs_run/*.dmtcp1

cd /usr/src/dvs/dvk-tests
./test_rmtbind 0 10 1 migr_server >> /usr/src/dvs/dvs-apps/dvs_run/node0_test3a.log

read -p "arrancar el cliente press enter"
cd /usr/src/dvs/dvs-apps/dvs_run/
nsenter -p -t$DC0 ./migr_client 0 11 10 4096 100 1 > node0_client_test3a.log 2> node0_client_test3a.err  &
exit


