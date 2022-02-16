#!/bin/bash
if [ $# -ne 6 ]
then 
 echo "./server_ftpd.sh <dcid> <min_svr_ep> <max_svr_ep> <clt_nodeid> <min_clt_ep> <max_clt_ep> "
 exit 1 
fi
######################## CLIENTS ###########################
cd /usr/src/dvs/dvk-tests/
i=$5
end=$6
while [ $i -le $end ]; do
    echo "./test_rmtbind $1 $i $4 m3ftp$i"
	./test_rmtbind $1 $i $4 m3ftp$i 
    i=$(($i+1))
done
cat /proc/dvs/DC0/procs
read -p "Press enter to continue...."
echo  "starting servers..."
sleep 5
######################## SERVERS ###########################
cd /usr/src/dvs/dvs-apps/m3ftp
. /dev/shm/DC0.sh
i=$2
end=$3
while [ $i -le $end ]; do
    echo "./m3ftpd $1 $i > m3ftpd$i.out 2> m3ftpd$i.err &"
	./m3ftpd $1 $i > m3ftpd$i.out 2> m3ftpd$i.err &
    i=$(($i+1))
done
cat /proc/dvs/DC0/procs
exit 


