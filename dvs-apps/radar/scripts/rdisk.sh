#!/bin/bash
max=3
if [ $# -ne 2 ]
then 
 echo "usage: $0 <lcl_nodeid> <dcid>"
 exit 1 
fi
lcl=$1
dcid=$2
rmt1=$(( ($lcl + 1 ) % 3))
rmt2=$(( ($lcl + 2 ) % 3))
echo "lcl=$lcl"
echo "rmt1=$rmt1"
echo "rmt2=$rmt2"
echo "dcid=$dcid" 
read  -p "Enter para continuar... "
dmesg -c > /dev/null
read  -p "Spread Enter para continuar... "
mkdir /var/run/spread
/usr/local/sbin/spread -c /etc/spread.conf > spread.txt &
cd /usr/src/dvs/dvk-mod
mknod /dev/dvk c 33 0
dmesg -c > /usr/src/dvs/dvk-tests/dmesg.txt
insmod dvk.ko dvk_major=33 dvk_minor=0 dvk_nr_devs=1 
dmesg -c > /usr/src/dvs/dvk-tests/dmesg.txt
#cd /usr/src/dvs/dvs-apps/dvsd
#./dvsd $lcl 
part=(5 + $dcid)
echo "partition $part"
read  -p "mount Enter para continuar... "
mount  /dev/sdb$part /usr/src/dvs/vos/rootfs/DC$dcid
cd /usr/src/dvs/dvk-tests
read  -p "local_nodeid=$lcl Enter para continuar... "
./test_dvs_init -n $lcl -D 16777215
read  -p "DC$dcid Enter para continuar... "
cd /usr/src/dvs/dvs-apps/dc_init
echo "# dc_init config file"     	>  DC$dcid.cfg
echo "dc DC$dcid {"       			>> DC$dcid.cfg
echo "dcid $dcid;"     				>> DC$dcid.cfg
echo "nr_procs 221;"    			>> DC$dcid.cfg
echo "nr_tasks 34;"    				>> DC$dcid.cfg
echo "nr_sysprocs 64;"  			>> DC$dcid.cfg
echo "nr_nodes 32;"    				>> DC$dcid.cfg
echo "warn2proc 0;"     			>> DC$dcid.cfg
echo "warnmsg 1;"     				>> DC$dcid.cfg
echo "ip_addr \"192.168.1.10$lcl\";"	>> DC$dcid.cfg
echo "memory 512;"    				>> DC$dcid.cfg
echo "image \"/usr/src/dvs/vos/images/debian$dcid.img\";"  	>> DC$dcid.cfg
echo "mount \"/usr/src/dvs/vos/rootfs/DC$dcid\";"  			>> DC$dcid.cfg
echo "};"          					>> DC$dcid.cfg
./dc_init DC$dcid.cfg 
dmesg -c >> /usr/src/dvs/dvk-tests/dmesg.txt
#read  -p "TCP PROXY Enter para continuar... "
#     PARA DESHABILITAR EL ALGORITMO DE NAGLE!! 
echo 1 > /proc/sys/net/ipv4/tcp_low_latency
echo 0 > /proc/sys/kernel/hung_task_timeout_secs
cd /usr/src/dvs/dvk-proxies
read  -p "TCP PROXY Enter para continuar... "
./tcp_proxy node$rmt1 $rmt1 >node$rmt1.txt 2>error$rmt1.txt &
./tcp_proxy node$rmt2 $rmt2 >node$rmt2.txt 2>error$rmt2.txt &
sleep 5
read  -p "Enter para continuar... "
cat /proc/dvs/nodes
cat /proc/dvs/proxies/info
cat /proc/dvs/proxies/procs
read  -p "ADDNODE Enter para continuar... "
cd /usr/src/dvs/dvk-tests
cat /proc/dvs/DC$dcid/info
./test_add_node $dcid $rmt1
./test_add_node $dcid $rmt2
sleep 1
cat /proc/dvs/nodes
cat /proc/dvs/DC$dcid/info
cd /usr/src/dvs/dvs-apps/dc_init
echo "ATENCION!! ejecutar en . ./DC$dcid.sh"
exit
