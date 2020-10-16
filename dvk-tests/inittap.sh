#!/bin/bash
#		Topology:
#							NODE 0		
#			tap0 -----br0--linux_routing----eth0 
#			tap1-------|	
#!/bin/bash
if [ $# -ne 1 ]
then 
	echo "usage: $0 <lcl_nodeid>"
	lcl=0
else 
	lcl=$1
fi	
let rmt=(1 - $lcl)
echo  lcl=$lcl rmt=$rmt
ipbr0="192.168.1.20"
iptap0="192.168.1.20$lcl"
iptap1="192.168.1.21$lcl"
echo ipbr0=$ipbr0 iptap0=$iptap0 iptap1=$iptap1 
netmask="255.255.255.0"
mactap0="02:AA:BB:CC:DD:0$lcl"
mactap1="02:AA:BB:CC:DD:0$rmt"
echo netmask=$netmask mactap0=$mactap0 mactap1=$mactap1 
# enable routing between interfaces
echo 1 >  /proc/sys/net/ipv4/ip_forward
# Bridge configuration --------------------------------------------------------
read  -p "Configuring br0. Enter para continuar... "
brctl addbr br0
ifconfig br0 $ipbr0 netmask $netmask 
ip link set dev br0 up 
# TAP0 configuration --------------------------------------------------------
read  -p "Configuring tap0. Enter para continuar... "
mknod /dev/tap0 c 36 $[ 0 + 16 ]
chmod 666 /dev/tap0
ip tuntap add dev tap0 mode tap
ip link set dev tap0 address $mactap0
ip link set dev tap0 up 
brctl addif br0 tap0
ifconfig tap0 $iptap0 netmask $netmask
ifconfig tap0 | grep addr
# TAP1 configuration --------------------------------------------------------
read  -p "Configuring tap1. Enter para continuar... "
mknod /dev/tap1 c 36 $[ 1 + 16 ]
chmod 666 /dev/tap1
ip tuntap add dev tap1 mode tap
ip link set dev tap1 address $mactap1
ip link set dev tap1 up 
brctl addif br0 tap1
ifconfig tap1  $iptap1 netmask  $netmask
ifconfig tap1 | grep addr
# Link ETH0 to BRIDGE
#read  -p "Conecting eth0 to br0. Enter para continuar... "
#brctl addif br0 eth0
brctl show
brctl showmacs br0
exit
