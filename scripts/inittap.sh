#!/bin/bash
#		Topology:
#							NODE 0		
#			tap0 -----br$dcidA--linux_routing----eth0 
#			tap1-------|	
#!/bin/bash
if [ $# -ne 1 ]
then 
	echo "usage: $0 <dcid>"
	dcidA=0
else 
	dcidA=$1
fi	
let dcidB=($dcidA+1)
echo  dcidA=$dcidA dcidB=$dcidB
ipbr0="192.168.2.2$dcidA"
iptap0="192.168.2.20$dcidA"
iptap1="192.168.2.20$dcidB"
echo ipbr0=$ipbr0 iptap0=$iptap0 iptap1=$iptap1 
netmask="255.255.255.0"
mactap0="02:AA:BB:CC:DD:0$dcidA"
mactap1="02:AA:BB:CC:DD:0$dcidB"
echo netmask=$netmask mactap0=$mactap0 mactap1=$mactap1 
# enable routing between interfaces
echo 1 >  /proc/sys/net/ipv4/ip_forward
# Bridge configuration --------------------------------------------------------
read  -p "Configuring br$dcidA. Enter para continuar... "
brctl addbr br$dcidA
ifconfig br$dcidA $ipbr0 netmask $netmask 
ip link set dev br$dcidA up 
# TAP0 configuration --------------------------------------------------------
read  -p "Configuring tap0. Enter para continuar... "
mknod /dev/tap0 c 36 $[ 0 + 16 ]
chmod 666 /dev/tap0
ip tuntap add dev tap0 mode tap
ip link set dev tap0 address $mactap0
ip link set dev tap0 up 
brctl addif br$dcidA tap0
ifconfig tap0 $iptap0 netmask $netmask
ifconfig tap0 | grep addr
# TAP1 configuration --------------------------------------------------------
read  -p "Configuring tap1. Enter para continuar... "
mknod /dev/tap1 c 36 $[ 1 + 16 ]
chmod 666 /dev/tap1
ip tuntap add dev tap1 mode tap
ip link set dev tap1 address $mactap1
ip link set dev tap1 up 
brctl addif br$dcidA tap1
ifconfig tap1  $iptap1 netmask  $netmask
ifconfig tap1 | grep addr
# Link ETH0 to BRIDGE
#read  -p "Conecting eth0 to br$dcidA. Enter para continuar... "
#brctl addif br$dcidA eth0
brctl show
brctl showmacs br$dcidA
exit
