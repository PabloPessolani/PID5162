MINIX AS A UNIKERNEL (MUK) CON THREADS 
==================================================

El objetivo es construir un Unikernel conformado por todos los servidores y tareas de  MINIX(MoL)
A diferencia de MoL  MUK es un Unikernel en donde cada server/tarea se ejecuta en un thread Linux, 
perteneciente al proceso MUK

En MUK se compilan todos los fuentes utilizando y creando librerias estáticas.

Cuando MUK inicia, arranca cada una de las tareas/servers como threads y luego queda esperando
por la finalizacion de todas.
Se puede seleccionar que tareas y servidores arranca en funcion de las constantes 
definidas en glo.h
#define 	ENABLE_SYSTASK	1
#define 	ENABLE_PM  		1
#define 	ENABLE_RDISK  	1
#define 	ENABLE_FS	  	1
#define 	ENABLE_IS	  	1
#define		ENABLE_NW		1
#define		ENABLE_FTP		1


=)=============================================


EJECUCION
=========

Se utiliza muk.sh para:
	- arrancar el DVS
	- arrancar el DC0 
	- Crear los archivos de configuracion de 
			- RDISK (rdisk.cfg)
			- MOLFS (molfs.cfg)
			- DC (DC0.cfg)
		Todos estos archivos se dejan en /dev/shm
	- se copia la version de imagen de disco elegida segun el tamaño de bloque 
		en /dev/shm/minixweb100.img
	- Se copian los archivos de configuracion de MUK2 y de M3NWEB 
			cp /usr/src/dvs/vos/muk/m3nweb.cfg /dev/shm 
			cp /usr/src/dvs/vos/muk/muk.cfg /dev/shm 	
	- Se arranca el proxy TIPC 
	
Luego, a mano se debe ejecutar 
		# . /dev/shm/DC0.sh
		
Dependiendo si alguna de las tareas/servidores se ejecutan en forma interna o externa 
como por ejemplo RDISK/MOLFS, deberán arrancarse estos previamente.

En definitiva, en /dev/shm queda esto
		root@node0:/usr/src/dvs/dvk-tests# ls -l /dev/shm
		total 100048
		-rw-r--r-- 1 root root       206 oct 16 00:44 DC0.cfg
		-rwx------ 1 root root        55 oct 16 00:44 DC0.sh
		-rw-r--r-- 1 root root         0 oct 16 00:44 dc_init0.err
		-rw-r--r-- 1 root root      3970 oct 16 00:44 dc_init0.out
		-rw-r--r-- 1 root root      2695 oct 16 00:44 dmesg.txt
		-rw-r--r-- 1 root root         0 oct 16 00:44 error1.txt
		-rw-r--r-- 1 root root        93 oct 16 00:45 m3nweb.cfg
		-rw-r--r-- 1 root root 102400000 oct 16 00:45 minixweb100.img
		-rw-r--r-- 1 root root       128 oct 16 00:44 molfs.cfg
		-rw-r--r-- 1 root root       247 oct 16 00:45 muk.cfg
		-rw-r--r-- 1 root root      3190 oct 16 00:45 node1.txt
		-rw-r--r-- 1 root root       169 oct 16 00:44 rdisk.cfg
		-rw-r--r-- 1 root root      8862 oct 16 00:44 spread.txt

El contenido del archivo minixweb100.img es este 
		root@node0:/usr/src/dvs/dvk-tests# cat /usr/src/dvs/vos/images/minix3-4096.txt
		total 63548
		-rw------- 1 root root      793 may 25 21:45 debug.h
		-rw-r--r-- 1 root root 10485760 may 25 21:09 file10M.txt
		-rw-r--r-- 1 root root 52428800 may 25 21:09 file50M.txt
		-rw------- 1 root root   103630 may 25 21:45 index.html
		-rw-r--r-- 1 root root  1377610 may 25 21:45 index.mht
		-rw------- 1 root root      984 may 25 21:45 macros.h
		-rw-r--r-- 1 root root   100000 may 25 21:46 muk.txt
		-rw-r--r-- 1 root root   320742 may 25 21:46 test.tar.gz
		-rw-r--r-- 1 root root   142097 may 25 21:46 test.zip


Para arrancar muk2 
	cd /usr/src/dvs/vos/muk2 
	./muk2.sh 
	
Abrir otra sesion y comprobar los procesos bindeados al DVK 

root@node0:~# cat /proc/dvs/DC0/procs 
DC pnr -endp -lpid/vpid- nd flag misc -getf -sndt -wmig -prxy name
 0  -2    -2  3336/3      0    0   20 27342 27342 27342 27342 muk2           
 0   0     0  3338/5      0    0    0 27342 27342 27342 27342 muk2           
 0   1     1  3340/6      0    0    0 27342 27342 27342 27342 muk2           
 0   3     3  3337/4      0    0    0 27342 27342 27342 27342 muk2           
 0   8     8  3341/7      0    0    0 27342 27342 27342 27342 muk2           
 0  14    14  3344/9      0    8    0 31438 27342 27342 27342 muk2           
 0  22    22  3342/8      0    0    0 27342 27342 27342 27342 muk2 

Abriendo un explorador con la URL http://192.168.1.100:8080/
Se abre el Web Information Server 
MUK Web Information Server .
sys_dc_dmp (F1)	sys_proc_dmp (F2)	sys_stats_dmp (F3)	rd_dev_dmp (F5)	pm_proc_dmp (F6)	fs_proc_dmp (F7)	fs_super_dmp (F9)

Abriendo un explorador con la URL http://192.168.1.100:8081/
se abre una pagina web de demo de M3NWEB 


Para hacer una transferencia de archivo via web 
Abrir una sesion de windows 
	C:\PAP\tmp>wget http://192.168.1.100:8081/file10M.txt
	SYSTEM_WGETRC = c:/progra~1/wget/etc/wgetrc
	syswgetrc = C:\Program Files (x86)\GnuWin32/etc/wgetrc
	--2019-10-26 12:17:41--  http://192.168.1.100:8081/file10M.txt
	Connecting to 192.168.1.100:8081... conectado.
	Petición HTTP enviada, esperando respuesta... 200 OK
	Longitud: 10485760 (10M) [text/txt]
	Saving to: `file10M.txt'

	100%[======================================>] 10.485.760   130K/s   in 83s

	2019-10-26 12:19:04 (123 KB/s) - `file10M.txt' saved [10485760/10485760]

Para hacer  una transferencia utilizando M3FTP 
Abrir otra sesion, a mano se debe ejecutar 
		# . /dev/shm/DC0.sh
	Para hacer un GET 
		# nsenter -p -t $DC0 ./m3ftp -g 0 file10M.txt file10M.out
			t_start=1572103222.67 t_stop=1572103507.55 t_total=284.88
			total_bytes = 10485760
			Throuhput = 36807.618143 [bytes/s]

Enviando kill -SIGQUIT <muk2_PID>
Se imprime la lista de tareas 

	task list:
		11  m3ftp                sleep
		 2  muk_tout_task        sleep
		 3  systask              sleep
		 4s fdtask               poll (running)
		 5  rdisk                sleep
		 6  pm                   sleep
		 7  fs                   sleep
		 8  is                   sleep
		 9                       fdwait for read
		10  nweb                 fdwait for read

MUK2 tiene un tamaño de 
root@node0:/usr/src/dvs/vos/muk2# ls -l muk2
-rwxr-xr-x 1 root root 2368388 oct 14 22:30 muk2

Para ver la utilizacion de memoria en ejecuciòn:
root@node0:/usr/src/dvs/vos/muk2# pmap 648
		648:   ./muk2 /dev/shm/muk.cfg
		08048000   1060K r-x-- muk2
		08151000     12K rw--- muk2
		08154000  77240K rw---   [ anon ]
		0cfc3000    780K rw---   [ anon ]
		b3e9e000      4K -----   [ anon ]
		b3e9f000   8192K rw---   [ anon ]
		b469f000      4K -----   [ anon ]
		b46a0000   8192K rw---   [ anon ]
		b4ea0000      4K -----   [ anon ]
		b4ea1000   8192K rw---   [ anon ]
		b56a1000      4K -----   [ anon ]
		b56a2000   8396K rw---   [ anon ]
		b5ed5000      4K -----   [ anon ]
		b5ed6000   8192K rw---   [ anon ]
		b66d6000      4K -----   [ anon ]
		b66d7000   8192K rw---   [ anon ]
		b6ed7000      4K -----   [ anon ]
		b6ed8000   8192K rw---   [ anon ]
		b7719000      8K r----   [ anon ]
		b771b000      8K r-x--   [ anon ]
		bfbd5000    132K rw---   [ stack ]
		 total   136816K

root@node0:/usr/src/dvs/vos/muk2# top
top - 12:44:54 up 6 min,  1 user,  load average: 0,01, 0,25, 0,18
Tasks: 106 total,   1 running, 105 sleeping,   0 stopped,   0 zombie
%Cpu(s):  0,1 us,  0,1 sy,  0,0 ni, 99,8 id,  0,0 wa,  0,0 hi,  0,0 si,  0,0 st
KiB Mem :  2066196 total,  1680724 free,    64932 used,   320540 buff/cache
KiB Swap:   522236 total,   522236 free,        0 used.  1707960 avail Mem 

  PID USER      PR  NI    VIRT    RES    SHR S  %CPU %MEM     TIME+ COMMAND                  
  648 root      20   0  136816   6396    968 S   0,3  0,3   0:00.29 muk2
  
  

==================================================================================
/usr/src/dvs/dvk-tests/muk.sh 

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



  


