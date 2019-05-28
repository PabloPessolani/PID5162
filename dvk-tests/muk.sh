#!/bin/bash
if [ $# -ne 3 ]
then 
 echo "usage: $0 <lcl_nodeid> <dcid> <block_size>"
 echo "block_sizes 1024 4096 8192 16384 32768"
 exit 1 
fi
let lcl=$1
let rmt=(1 - $lcl)
let dcid=$2
# check block sizes  --------------------------------------------
match=0
for b in 1024 4096 8192 16384 32768 
do
	if [ $b -eq $3 ]
	then
		let match=1
	fi 
done
if [ $match -eq 0 ]
then 
 echo "usage: $0 <lcl_nodeid> <dcid> <block_size>"
 echo "block_sizes 1024 4096 8192 16384 32768"
 exit 1  
fi  
#---------------------------------------------------------------
echo "lcl_nodeid=$lcl dcid=$dcid block_size=$3" 
echo "device MY_RDISK_IMG { " >  /dev/shm/molfs.cfg
echo "	major			3;  " >>  /dev/shm/molfs.cfg
echo "	minor			0; 	" >>  /dev/shm/molfs.cfg
echo "	type			RDISK_IMG;" >>  /dev/shm/molfs.cfg
echo "	volatile		NO;	" >>  /dev/shm/molfs.cfg
echo "	root_dev		YES; " >>  /dev/shm/molfs.cfg
echo "	buffer_size		$3; " >>  /dev/shm/molfs.cfg
echo "}; " >>  /dev/shm/molfs.cfg
cat /dev/shm/molfs.cfg
read  -p "Enter para continuar... "
#---------------------------------------------------------------
echo "device MINIX_FILE_IMG {" >  /dev/shm/rdisk.cfg
echo "	major			3; " >>  /dev/shm/rdisk.cfg
echo "	minor			0;" >>  /dev/shm/rdisk.cfg
echo "	type			FILE_IMAGE; " >>  /dev/shm/rdisk.cfg
echo "	image_file 		 \"/dev/shm/minixweb100.img\"; " >>  /dev/shm/rdisk.cfg
echo "	buffer			$3; " >>  /dev/shm/rdisk.cfg
echo "	volatile		NO; " >>  /dev/shm/rdisk.cfg
echo "	replicated		NO; " >>  /dev/shm/rdisk.cfg
echo "}; " >>  /dev/shm/rdisk.cfg
cat /dev/shm/rdisk.cfg
read  -p "Enter para continuar... "
#---------------------------------------------------------------
dmesg -c > /dev/null
read  -p "Spread Enter para continuar... "
mkdir /var/run/spread
/usr/local/sbin/spread -c /etc/spread.conf > /dev/shm/spread.txt &
cd /usr/src/dvs/dvk-mod
mknod /dev/dvk c 33 0
dmesg -c > /dev/shm/dmesg.txt
insmod dvk.ko dvk_major=33 dvk_minor=0 dvk_nr_devs=1 
dmesg -c >> /dev/shm/dmesg.txt
#cd /usr/src/dvs/dvs-apps/dvsd
#./dvsd $lcl 
part=(5 + $dcid)
echo "partition $part"
read  -p "mount Enter para continuar... "
#mount  /dev/sdb$part /usr/src/dvs/vos/rootfs/DC$dcid
cd /usr/src/dvs/dvk-tests
read  -p "local_nodeid=$lcl Enter para continuar... "
./test_dvs_init -n $lcl -D 16777215
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
cat /dev/shm/DC$dcid.cfg
read  -p "Enter para continuar... "
#---------------------------------------------------------------
/usr/src/dvs/dvs-apps/dc_init/dc_init /dev/shm/DC$dcid.cfg > /dev/shm/dc_init$dcid.out 2> /dev/shm/dc_init$dcid.err
dmesg -c >> /dev/shm/dmesg.txt
#read  -p "TCP PROXY Enter para continuar... "
#     PARA DESHABILITAR EL ALGORITMO DE NAGLE!! 
echo 1 > /proc/sys/net/ipv4/tcp_low_latency
echo 0 > /proc/sys/kernel/hung_task_timeout_secs
cd /usr/src/dvs/dvk-proxies
# read  -p "TCP PROXY Enter para continuar... "
#./sp_proxy_bat node$rmt $rmt >node$rmt.txt 2>error$rmt.txt &
# ./tcp_proxy_bat node$rmt $rmt >node$rmt.txt 2>error$rmt.txt &
read  -p "TIPC BAT PROXY Enter para continuar... "
tipc node set netid 4711
tipc_addr="1.1.10$lcl"
tipc node set addr $tipc_addr
tipc bearer enable media eth dev eth0 
read  -p "Enter para continuar... "
/usr/src/dvs/dvk-proxies/tipc_proxy_bat -bBZ -n node$rmt -i $rmt > /dev/shm/node$rmt.txt 2> /dev/shm/error$rmt.txt &	
sleep 5
tipc bearer list
tipc nametable show
tipc link list
read  -p "Enter para continuar... "
cat /proc/dvs/nodes
cat /proc/dvs/proxies/info
cat /proc/dvs/proxies/procs
read  -p "ADDNODE Enter para continuar... "
cd /usr/src/dvs/dvk-tests
cat /proc/dvs/DC$dcid/info
./test_add_node $dcid $rmt
sleep 1
cat /proc/dvs/nodes
cat /proc/dvs/DC$dcid/info
cat /proc/dvs/DC$dcid/procs 
cd /dev/shm
echo "file copy to /dev/shm"
cp /usr/src/dvs/vos/muk/m3nweb.cfg /dev/shm 
cp /usr/src/dvs/vos/muk/muk.cfg /dev/shm 
cp /usr/src/dvs/vos/images/minix3-$3.img /dev/shm/minixweb100.img
echo "run # . /dev/shm/DC0.sh"
exit 



  
