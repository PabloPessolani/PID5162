#!/bin/bash
#	BEFORE:		NODE0: Void
#				NODE1: Client, Server
#	AFTER:		NODE0: Server
#				NODE1: Client
#### Run this script in NODE1
. /dev/shm/DC0.sh    
echo DC0=$DC0 > /usr/src/dvs/dvs-apps/dvs_run/node1_test2.log 
/usr/src/lkl/dmtcp/bin/dmtcp_command -h node1 -p 1234 -k 

# Run in node 1
cd /usr/src/dvs/dvs-apps/dvs_run
rm *.dmtcp
echo  "Start SERVER process in NODE1"
/usr/src/lkl/dmtcp/bin/dmtcp_launch -h node1 -p 1234 ./migr_server > node1_test2_server.log 0 10 &
sleep 5s
echo  "Start CLIENT process in NODE1"
nsenter -p -t$DC0 ./migr_client 0 11 10 4096 100 1 > node1_test2_client.log & 
echo "Waiting 10 secs before migration start..."
sleep 10s
cat /proc/dvs/DC0/procs >> .log
echo "Migration Start"
startMigr=`date +%s%N`
echo startMigr=$startMigr >> node1_test2.log
./dvs_migrate -s 0 10 >> node1_test2.log
cat /proc/dvs/DC0/procs >> node1_test2.log
echo  "Client process was stopped waiting for server migration"
#echo >> node1_test2.log 
#date >> node1_test2.log
#echo >> node1_test2.log
echo  "Take SERVER checkpoint"
/usr/src/lkl/dmtcp/bin/dmtcp_command -h node1 -i 0 -p 1234 -bc >> node1_test2.log
cat /proc/dvs/DC0/procs >> node1_test2.log
mv ckpt_*.dmtcp checkpoint_migr_server.dmtcp
echo  "SERVER Checkpoint finished"
startTranfer=`date +%s%N`
echo startTranfer=$startTranfer >> node1_test2.log
sshpass -p "root" scp -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -r checkpoint_migr_server.dmtcp root@node0:/usr/src/dvs/dvs-apps/dvs_run
endTranfer=`date +%s%N`
echo endTranfer=$endTranfer >> node1_test2.log
# Run script in node 0
echo Run script in NODE0
sshpass -p 'root' ssh root@node0 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "/usr/src/dvs/dvs-apps/dvs_run/node0_test2.sh"
cat /proc/dvs/DC0/procs >> node1_test2.log
#dmesg >> dmesg.log
# echo sleeping 30 secs
# sleep 30
#sync
#sync
#echo "Successful SERVER migration, warn local DVK" 
./dvs_migrate -c 0 10 0 
endMigr=`date +%s%N`
# dmesg >> dmesg.log
cat /proc/dvs/DC0/procs >> node1_test2.log
echo endMigr=$endMigr >> node1_test2.log
echo Migration time was `expr $endMigr - $startMigr` nanoseconds >> node1_test2.log
echo "Migration time was `expr $endMigr - $startMigr` nanoseconds"
echo Tranfer time was `expr $endTranfer - $startTranfer` nanoseconds >> node1_test2.log
echo  "SERVER checkpoint image was transferred to NODE0" 
stat --printf="SERVER checkpoint image file size=%s\n" checkpoint_migr_server.dmtcp >> node1_test2.log
