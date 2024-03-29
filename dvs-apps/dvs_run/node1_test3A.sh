#!/bin/bash
#	BEFORE:		NODE0: Client
#				NODE1: Server
#				NODE2: Void
#	AFTER:		NODE0: Client
#				NODE1: Void 
#				NODE2: Server
#### Run this script in NODE1
if [ $# -eq 0 ]
then 
	let memKB=64 
fi
if [ $# -eq 1 ]
then 
	let memKB=$1 
fi
if [ $memKB -eq 0 ]
then 
	let memKB=64
fi
. /dev/shm/DC0.sh    
echo DC0=$DC0 > node1_test3A.log
echo memKB=$memKB >> node1_test3A.log

/usr/src/lkl/dmtcp/bin/dmtcp_command -h node1 -p 1234 -k 

# register the remote cliente endpoint in NODE0
cd /usr/src/dvs/dvk-tests
./test_rmtbind 0 11 0 migr_client >> node1_test3A.log

cd /usr/src/dvs/dvs-apps/dvs_run
rm *.dmtcp

read -p "Start SERVER process in NODE1... press enter"
/usr/src/lkl/dmtcp/bin/dmtcp_launch --no-gzip -h node1 -p 1234 ./migr_server 0 10 $memKB > node1_test3_server.log  &
sleep 5s
pmap `pgrep migr_server` | grep "total" >> node1_test3A.log
cat /proc/dvs/DC0/procs 	>> node1_test3A.log
read -p "Run client in NODE0... press enter"
sshpass -p 'root' ssh root@node0 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "/usr/src/dvs/dvs-apps/dvs_run/node0_test3a.sh"
read -p "Set environment in NODE2 ... press enter"
sshpass -p 'root' ssh root@node2 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "/usr/src/dvs/dvs-apps/dvs_run/node2_test3a.sh"

echo  "Take FIRST SERVER checkpoint"
/usr/src/lkl/dmtcp/bin/dmtcp_command -h node1 -i 0 -p 1234 -bc >> node1_test3A.log 
mv ckpt_*.dmtcp checkpoint_migr_server.dmtcp1
chmod 600 checkpoint_migr_server.dmtcp1
echo  "FIRST SERVER Checkpoint finished"
startTranfer1=`date +%s%N`
export RSYNC_PASSWORD=root
rsync -v --compress --password-file rsync.pass checkpoint_migr_server.dmtcp1 rsync://root@node2/dvs_run >> node1_test3A.log 
endTranfer1=`date +%s%N`
stat --printf="SERVER FIRST checkpoint image file size=%s\n" checkpoint_migr_server.dmtcp1 >> node1_test3A.log

read -p "Waiting 10 secs before migration start... press enter"
sleep 10s

echo "Migration Start"
startMigr=`date +%s%N`
# signal NODE0 DVK that server will migrate  
sshpass -p 'root' ssh root@node0 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "/usr/src/dvs/dvs-apps/dvs_run/dvs_migrate -s 0 10"
echo  "Client process was stopped waiting for server migration"

# signal NODE1 DVK that server will migrate
./dvs_migrate -s 0 10 		>> node1_test3A.log
echo  "Server process was stopped for migration"
cat /proc/dvs/DC0/procs 	>> node1_test3A.log

echo  "Take SECOND SERVER checkpoint"
/usr/src/lkl/dmtcp/bin/dmtcp_command -h node1 -i 0 -p 1234 -bc >> node1_test3A.log
mv ckpt_*.dmtcp checkpoint_migr_server.dmtcp
bsdiff checkpoint_migr_server.dmtcp1 checkpoint_migr_server.dmtcp dmtcp.diff    
echo  "SECOND SERVER Checkpoint finished"

startTranfer=`date +%s%N`
# sshpass -p "root" scp -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -r checkpoint_migr_server.dmtcp root@node2:/usr/src/dvs/dvs-apps/dvs_run
export RSYNC_PASSWORD=root
rsync -v --compress --password-file rsync.pass dmtcp.diff rsync://root@node2/dvs_run >> node1_test3A.log
endTranfer=`date +%s%N`

# Run script in node 2
echo Run script in NODE2
sshpass -p 'root' ssh root@node2 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "/usr/src/dvs/dvs-apps/dvs_run/node2_test3B.sh" 
# signal NODE0 DVK that server has migrated  
echo Migration Commit in NODE0
sshpass -p 'root' ssh root@node0 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "/usr/src/dvs/dvs-apps/dvs_run/dvs_migrate -c 0 10 2" &
echo  "Client process was restarted "

# signal NODE1 DVK that server has migrated  
./dvs_migrate -c 0 10 2 
endMigr=`date +%s%N`
# dmesg >> dmesg.log
cat /proc/dvs/DC0/procs >> node1_test3A.log
echo startTranfer1=$startTranfer1 >> node1_test3A.log
echo endTranfer1=$endTranfer1 >> node1_test3A.log
echo startTranfer=$startTranfer >> node1_test3A.log
echo endTranfer=$endTranfer >> node1_test3A.log
echo startMigr=$startMigr 	>> node1_test3A.log
echo endMigr=$endMigr >> node1_test3A.log
echo Migration time was `expr $endMigr - $startMigr` nanoseconds >> node1_test3A.log
echo "Migration time was `expr $endMigr - $startMigr` nanoseconds"
echo FIRST Tranfer time was `expr $endTranfer1 - $startTranfer1` nanoseconds >> node1_test3A.log
echo SECOND Tranfer time was `expr $endTranfer - $startTranfer` nanoseconds >> node1_test3A.log
echo  "SERVER checkpoint image was transferred to NODE0" 
stat --printf="SERVER SECOND checkpoint image file size=%s\n" checkpoint_migr_server.dmtcp >> node1_test3A.log
