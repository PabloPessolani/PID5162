#!/bin/bash
#		Topology:
#							NODE 0		
#			tap0 -----br$dcidA--linux_routing----eth0 
#			tap1-------|	
#!/bin/bash
echo "#############################################"
echo  EJECUTAR EN CONSOLA PORQUE SE CAE SSH
echo "#############################################"
read  -p "Enter para continuar... "
if [ $# -ne 1 ]
then 
	echo "usage: $0 <dcid>"
	dcidA=0
else 
	dcidA=$1
fi	
let netid=($dcidA+1)
echo netid=$netid
let dcidB=($dcidA+1)
let dcidC=($dcidA+2)
echo  dcidA=$dcidA dcidB=$dcidB dcidC=$dcidC 
ipbr0="192.168.$netid.1$dcidA"
ipeth0="192.168.$netid.10$dcidA"
iptap0="192.168.$netid.10$dcidB"
iptap1="192.168.$netid.10$dcidC"
echo ipbr0=$ipbr0 iptap0=$iptap0 iptap1=$iptap1 
netmask="255.255.255.0"
mactap0="02:AA:BB:CC:0$dcidA:0$dcidB"
mactap1="02:AA:BB:CC:0$dcidA:0$dcidC"
echo netmask=$netmask mactap0=$mactap0 mactap1=$mactap1 
# enable routing between interfaces
echo 1 >  /proc/sys/net/ipv4/ip_forward
# Bridge configuration --------------------------------------------------------
read  -p "Configuring br$dcidA. Enter para continuar... "
tunctl -u root -t HTAP$dcidA
brctl addbr br$dcidA
ifconfig HTAP$dcidA 0.0.0.0 up
#ifconfig eth0 0.0.0.0 up
read  -p "ARRANCAR UML. Enter para continuar... "
brctl addif br$dcidA HTAP$dcidA
#brctl addif br$dcidA eth0
#ifconfig eth0 $ipeth0 netmask 255.255.255.0  up
ifconfig br$dcidA $ipbr0 netmask 255.255.255.0 up
route add -host $iptap0 dev br$dcidA
brctl show
brctl showmacs br$dcidA
exit
