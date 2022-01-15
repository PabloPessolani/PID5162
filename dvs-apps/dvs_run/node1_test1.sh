#!/bin/bash
#	BEFORE:		NODE0: Client
#				NODE1: Server
#	AFTER:		NODE0: Client, Server
#				NODE1: Void 
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
echo DC0=$DC0 > node1_test1.log
echo memKB=$memKB >> node1_test1.log

/usr/src/lkl/dmtcp/bin/dmtcp_command -h node1 -p 1234 -k 

# Run in NODE1
# register the remote cliente endpoint in NODE0
cd /usr/src/dvs/dvk-tests
nsenter -p -t$DC0 ./test_rmtbind 0 11 0 migr_client >> node1_test1.log

cd /usr/src/dvs/dvs-apps/dvs_run
rm *.dmtcp *.dmtcp1

read -p "Start SERVER process in NODE1... press enter"
/usr/src/lkl/dmtcp/bin/dmtcp_launch --gzip -h node1 -p 1234 ./migr_server 0 10 $memKB > node1_test1_server.log  &
sleep 5s
pmap `pgrep migr_server` | grep "total" >> node1_test1.log
cat /proc/dvs/DC0/procs 	>> node1_test1.log
echo Run client in NODE0
sshpass -p 'root' ssh root@node0 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "/usr/src/dvs/dvs-apps/dvs_run/node0_test1a.sh"
read -p "Waiting 10 secs before migration start... press enter "
sleep 10s
echo "Migration Start"
startMigr=`date +%s%N`
# signal NODE0 DVK that server will migrate  
sshpass -p 'root' ssh root@node0 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "/usr/src/dvs/dvs-apps/dvs_run/dvs_migrate -s 0 10"
echo  "Client process was stopped waiting for server migration"
# signal NODE1 DVK that server will migrate
./dvs_migrate -s 0 10 		>> node1_test1.log
echo  "Server process was stopped for migration"
cat /proc/dvs/DC0/procs 	>> node1_test1.log
echo  "Take SERVER checkpoint"
/usr/src/lkl/dmtcp/bin/dmtcp_command -h node1 -i 0 -p 1234 -bc >> node1_test1.log
mv ckpt_*.dmtcp checkpoint_migr_server.dmtcp
echo  "SERVER Checkpoint finished"
startTranfer=`date +%s%N`
#sshpass -p "root" scp -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -r checkpoint_migr_server.dmtcp root@node0:/usr/src/dvs/dvs-apps/dvs_run
export RSYNC_PASSWORD=root
rsync -v --compress --password-file rsync.pass checkpoint_migr_server.dmtcp rsync://root@node0/dvs_run >> node1_test1.log
endTranfer=`date +%s%N`
# Run script in node 0
./dvs_migrate -c 0 10 0 
echo Run script in NODE0
sshpass -p 'root' ssh root@node0 -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR "/usr/src/dvs/dvs-apps/dvs_run/node0_test1.sh"
endMigr=`date +%s%N`
dmesg >> dmesg.log
cat /proc/dvs/DC0/procs >> node1_test1.log
echo startTranfer=$startTranfer >> node1_test1.log
echo endTranfer=$endTranfer >> node1_test1.log
echo startMigr=$startMigr 	>> node1_test1.log
echo endMigr=$endMigr >> node1_test1.log
echo Migration time was `expr $endMigr - $startMigr` nanoseconds >> node1_test1.log
echo "Migration time was `expr $endMigr - $startMigr` nanoseconds"
echo Tranfer time was `expr $endTranfer - $startTranfer` nanoseconds >> node1_test1.log
echo  "SERVER checkpoint image was transferred to NODE0" 
stat --printf="SERVER checkpoint image file size=%s\n" checkpoint_migr_server.dmtcp >> node1_test1.log