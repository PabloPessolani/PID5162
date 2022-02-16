#!/bin/bash
if [ $# -ne 6 ]
then 
 echo "./client_ftp.sh <dcid>  <min_clt_ep> <max_clt_ep>  <clt_nodeid> <min_svr_ep> <max_svr_ep> "
 exit 1 
fi
######################## SERVERS ###########################
cd /usr/src/dvs/dvk-tests/
i=$5
end=$6
while [ $i -le $end ]; do
    echo "./test_rmtbind $1 $i $4 m3ftpd$i"
	./test_rmtbind $1 $i $4 m3ftpd$i
    i=$(($i+1))
done
cat /proc/dvs/DC0/procs
read -p "Press enter to continue...."
#
echo  "starting transfers..."
sleep 5
cd /usr/src/dvs/dvs-apps/m3ftp
. /dev/shm/DC0.sh
######################## CLIENTS ###########################
i=$2
end=$3
svrep=$5
while [ $i -le $end ]; do
    echo "./m3ftp -g $1 $i $svrep file-100M.dat file$i-100M.dat > gftp$i.out &"
	./m3ftp -g $1 $i $svrep file-100M.dat file$i-100M.dat > gftp$i.out &
    i=$(($i+1))
    svrep=$(($svrep+1))
done
cat /proc/dvs/DC0/procs
exit 


