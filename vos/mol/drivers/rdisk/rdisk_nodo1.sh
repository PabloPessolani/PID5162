#!/bin/bash
if [ $# -ne 1 ]
then 
	echo "usage: $0 <lcl_nodeid>"
	exit 1 
fi
lcl=$1
vmid=0
let rmt=(1 - $lcl)
echo "lcl=$lcl rmt=$rmt" 
read  -p "Enter para continuar... "
dmesg -c > /dev/null
X86DIR="/sys/kernel/debug/x86"
if [ ! -d $X86DIR ]; then
	echo mounting debugfs...
	mount -t debugfs none /sys/kernel/debug
else 
	echo debugfs is already mounted
fi
mount | grep debug
read  -p "Spread Enter para continuar... "
/usr/local/sbin/spread  > spread.txt &			
cd /home/MoL_Module/mol-module
insmod ./mol_replace.ko
lsmod | grep mol
cd /home/MoL_Module/mol-ipc
read  -p "DRVS Enter para continuar... "
/home/MoL_Module/mol-ipc/tests/test_drvs_end
/home/MoL_Module/mol-ipc/tests/test_drvs_init -n $lcl -D 16777215
read  -p "VM$vmid Enter para continuar... "
/home/MoL_Module/mol-ipc/tests/test_vm_init -v $vmid -P 0 -m 1
read  -p "TCP PROXY Enter para continuar... "
#    	PARA DESHABILITAR EL ALGORITMO DE NAGLE!! 
echo 1 > /proc/sys/net/ipv4/tcp_low_latency
cd /home/MoL_Module/mol-ipc/proxy
./tcp_proxy node$rmt $rmt >node$rmt.txt 2>error$rmt.txt &
sleep 5
read  -p "Enter para continuar... "
cat /proc/drvs/nodes
cat /proc/drvs/proxies/info
cat /proc/drvs/proxies/procs
read  -p "ADDNODE Enter para continuar... "
cat /proc/drvs/VM$vmid/info
/home/MoL_Module/mol-ipc/tests/test_add_node $vmid $rmt
##############################################
################## SYSTASK  #####################
read  -p "SYSTASK Enter para continuar... "
dmesg -c  > /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cd /home/MoL_Module/mol-ipc/tasks/systask
./systask -v $vmid  > systask$lcl.txt 2> st_err$lcl.txt &
sleep 2
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cat /proc/drvs/VM$vmid/procs
################# START RDISK  #################
read  -p "RDISK Enter para continuar... "
cd /home/MoL_Module/mol-ipc/tasks/rdisk
#./rdisk -r<replicate> -[f<full_update>|d<diff_updates>] -D<dyn_updates> -z<compress> -c <config file>
./rdisk -rdDc rdisk3.cfg > rdisk$lcl.txt 2> rdisk_err$lcl.txt &
sleep 2
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cat /proc/drvs/VM$vmid/procs
################## NODE 0 #####################
if [ $lcl -eq 0 ]; then 
################## PM NODE0 #####################
read  -p "PM Enter para continuar... "
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cd /home/MoL_Module/mol-ipc/servers/pm
./pm $vmid > pm$lcl.txt 2> pm_err$lcl.txt &
sleep 2
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
################## RS NODE0 #####################
read  -p "RS Enter para continuar... "
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
cd /home/MoL_Module/mol-ipc/servers/rs
./rs $vmid > rs$lcl.txt 2> rs_err$lcl.txt &
sleep 2
dmesg -c  >> /home/MoL_Module/mol-ipc/dmesg$lcl.txt
#######################################
fi
exit

