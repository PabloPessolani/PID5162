#!/bin/bash
if [ $# -ne 2 ]
then 
 echo "usage: $0 <lcl_nodeid> <dcid>"
 exit 1 
fi
lcl=$1
if [ $1 -eq 0 ]
then
let rmtA=1
let rmtB=2
fi
if [ $1 -eq 1 ]
then
let rmtA=0
let rmtB=2
fi
if [ $1 -eq 2 ]
then
let rmtA=0
let rmtB=1
fi
echo "lcl=$lcl rmtA=$rmtA rmtB=$rmtB"
dcid=$2
echo "lcl_nodeid=$lcl dcid=$dcid" 
read  -p "Read to initilize TAP devices. Enter para continuar... "
./inittap.sh $lcl
echo > /var/log/kern.log
echo > /var/log/syslog
echo > /var/log/messages
dmesg -c > /dev/null
read  -p "Spread Enter para continuar... "
mkdir /var/run/spread
/usr/local/sbin/spread -c /etc/spread.conf > /dev/shm/spread.txt &
cd /usr/src/dvs/dvk-mod
mknod /dev/dvk c 33 0
dmesg -c > /dev/shm/dmesg.txt
insmod dvk.ko dvk_major=33 dvk_minor=0 dvk_nr_devs=1 dbglvl=16777215
dmesg -c >> /dev/shm/dmesg.txt
lsmod | grep dvk 
 read  -p "mount Enter para continuar... "
#cd /usr/src/dvs/dvs-apps/dvsd
#./dvsd $lcl 
part=(5 + $dcid)
echo "partition $part"
read  -p "mount Enter para continuar... "
#mount  /dev/sdb$part /usr/src/dvs/vos/rootfs/DC$dcid
cd /usr/src/dvs/dvk-tests
read  -p "local_nodeid=$lcl Enter para continuar... "
./test_dvs_init -n $lcl -D 16777215 -C TEST_CLUSTER
read  -p "DC$dcid Enter para continuar... "
# ./test_dc_init -d $dcid
cd /dev/shm
echo "# dc_init config file"     	>  /dev/shm/DC$dcid.cfg
echo "dc DC$dcid {"       			>> /dev/shm/DC$dcid.cfg
echo "dcid $dcid;"     				>> /dev/shm/DC$dcid.cfg
echo "nr_procs 221;"    			>> /dev/shm/DC$dcid.cfg
echo "nr_tasks 34;"    				>> /dev/shm/DC$dcid.cfg
echo "nr_sysprocs 64;"  			>> /dev/shm/DC$dcid.cfg
echo "nr_nodes 32;"    				>> /dev/shm/DC$dcid.cfg
echo "tracker \"NO\";"    			>> /dev/shm/DC$dcid.cfg
#echo "warn2proc 0;"     			>> DC$dcid.cfg
#echo "warnmsg 1;"     				>> DC$dcid.cfg
#echo "ip_addr \"192.168.10.10$dcid\";"	>> DC$dcid.cfg
echo "memory 512;"    				>> /dev/shm/DC$dcid.cfg
echo "image \"/usr/src/dvs/vos/images/debian$dcid.img\";"  	>> /dev/shm/DC$dcid.cfg
 echo "mount \"/usr/src/dvs/vos/rootfs/DC$dcid\";"  		>> /dev/shm/DC$dcid.cfg
echo "};"          					>> DC$dcid.cfg
/usr/src/dvs/dvs-apps/dc_init/dc_init /dev/shm/DC$dcid.cfg > /dev/shm/dc_init$dcid.out 2> /dev/shm/dc_init$dcid.err
dmesg -c >> /dev/shm/dmesg.txt
read  -p " TCP LZ4 BAT PROXY Enter para continuar... "
# read  -p "TCP PROXY Enter para continuar... "
#     PARA DESHABILITAR EL ALGORITMO DE NAGLE!! 
echo 1 > /proc/sys/net/ipv4/tcp_low_latency
echo 0 > /proc/sys/kernel/hung_task_timeout_secs
# tcp_keepalive_time: the interval between the last data packet sent (simple ACKs are not considered data) 
#           and the first keepalive probe; after the connection is marked to need keepalive, this counter is not used any further
# tcp_keepalive_intvl:  the interval between subsequential keepalive probes, regardless of what the connection has exchanged in the meantime
# tcp_keepalive_probes: the number of unacknowledged probes to send before considering the connection dead and notifying the application layer
#echo 60 > /proc/sys/net/ipv4/tcp_keepalive_time
#echo 30 > /proc/sys/net/ipv4/tcp_keepalive_intvl
# echo 3  > /proc/sys/net/ipv4/tcp_keepalive_probes
cd /usr/src/dvs/dvk-proxies
#./tcp_proxy node$rmtA $rmtA >/dev/shm/node$rmtA.txt 2>/dev/shm/error$rmtA.txt &
/usr/src/dvs/dvk-proxies/lz4tcp_proxy_bat -bBZP -n node$rmtA -i $rmtA > /dev/shm/node$rmtA.txt 2> /dev/shm/error$rmtA.txt &	
/usr/src/dvs/dvk-proxies/lz4tcp_proxy_bat -bBZP -n node$rmtB -i $rmtB > /dev/shm/node$rmtB.txt 2> /dev/shm/error$rmtB.txt &	
#read  -p "TIPC LZ4 BAT PROXY Enter para continuar... "
#tipc node set netid 4711
#tipc_addr="1.1.10$lcl"
#tipc node set addr $tipc_addr
#tipc bearer enable media eth dev eth0 
#read  -p "Enter para continuar... "
#/usr/src/dvs/dvk-proxies/tipc_proxy_bat -bBZ -n node$rmtA -i $rmtA > /dev/shm/node$rmtA.txt 2> /dev/shm/error$rmtA.txt &	
sleep 5
netstat  -nat
#tipc bearer list
#tipc nametable show
#tipc link list
read  -p "Enter para continuar... "
cat /proc/dvs/nodes
cat /proc/dvs/proxies/info
cat /proc/dvs/proxies/procs
read  -p "ADDNODE Enter para continuar... "
cd /usr/src/dvs/dvk-tests
cat /proc/dvs/DC$dcid/info
./test_add_node $dcid $rmtA
./test_add_node $dcid $rmtB
sleep 1
cat /proc/dvs/nodes
cat /proc/dvs/DC$dcid/info
cat /proc/dvs/DC$dcid/procs 
cd /dev/shm
echo "run # . /dev/shm/DC0.sh"
exit 



  
