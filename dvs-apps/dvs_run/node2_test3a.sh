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
#/usr/bin/killall rsync 
#rm -rf /var/run/rsyncd.pid
#/usr/bin/rsync --daemon
# sleep 2
echo DC0=$DC0 > /usr/src/dvs/dvs-apps/dvs_run/node2_test3a.log 
ps -ef | grep rsync >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3a.log 
cat /var/run/rsyncd.pid >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3a.log
rm /usr/src/dvs/dvs-apps/dvs_run/*.dmtcp

cd /usr/src/dvs/dvk-tests
./test_rmtbind 0 10 1 migr_server >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3a.log 
./test_rmtbind 0 11 0 migr_client >> /usr/src/dvs/dvs-apps/dvs_run/node2_test3a.log

cd /usr/src/dvs/dvs-apps/dvs_run
exit


