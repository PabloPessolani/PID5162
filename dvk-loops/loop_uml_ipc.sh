#!/bin/bash
if [ $# -ne 1 ]
then 
	echo "usage: $0 <loops>"
	echo $#
	exit 1 
fi
rm loop_dvk_ipc*.txt
echo "Test send-receive vs receive-send" 
for i in {1..10}
do
    echo "loop_dvk_ipc1 $i"
    /usr/src/dvs/dvk-loops/loop_dvk_ipc1 $1 | grep Throuhput  >> loop_dvk_ipc1.txt
sleep 1
done
echo "Test sendrec vs receive-send" 
for i in {1..10}
do
    echo "loop_dvk_ipc2 $i"
    /usr/src/dvs/dvk-loops/loop_dvk_ipc2 $1 | grep Throuhput >> loop_dvk_ipc2.txt
sleep 1
done
echo "Test notify-receive vs receive-notify" 
for i in {1..10}
do
    echo "loop_dvk_ipc3 $i"
    /usr/src/dvs/dvk-loops/loop_dvk_ipc3 $1	| grep Throuhput >> loop_dvk_ipc3.txt
sleep 1
dmesg -c >> dmesg_ntf.txt
done
dmesg -c > dmesg.txt
echo "Test sendrec vs rcvrqst-reply" 
for i in {1..10}
do
    echo "loop_dvk_ipc4 $i"
    /usr/src/dvs/dvk-loops/loop_dvk_ipc4 $1	| grep Throuhput >> loop_dvk_ipc4.txt
sleep 1
done

