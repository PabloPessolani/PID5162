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
	- Se copian los archivos de configuracion de MUK y de M3NWEB 
			cp /usr/src/dvs/vos/muk/m3nweb.cfg /dev/shm 
			cp /usr/src/dvs/vos/muk/muk.cfg /dev/shm 	
	- Se arranca el proxy TIPC 
	
Luego, a mano se debe ejecutar 
		# . /dev/shm/DC0.sh
		
Dependiendo si alguna de las tareas/servidores se ejecutan en forma interna o externa 
como por ejemplo RDISK/MOLFS, deberán arrancarse estos previamente.

En definitiva, en /dev/shm queda esto
	root@node0:/usr/src/dvs/dvk-tests# ls -l /dev/shm/
	total 100048
	-rw-r--r-- 1 root root       206 nov  3 11:25 DC0.cfg
	-rwx------ 1 root root        54 nov  3 11:25 DC0.sh
	-rw-r--r-- 1 root root         0 nov  3 11:25 dc_init0.err
	-rw-r--r-- 1 root root      3962 nov  3 11:25 dc_init0.out
	-rw-r--r-- 1 root root      2685 nov  3 11:25 dmesg.txt
	-rw-r--r-- 1 root root         0 nov  3 11:25 error1.txt
	-rw-r--r-- 1 root root        93 nov  3 11:25 m3nweb.cfg
	-rw-r--r-- 1 root root 102400000 nov  3 11:25 minixweb100.img
	-rw-r--r-- 1 root root       128 nov  3 11:25 molfs.cfg
	-rw-r--r-- 1 root root       247 nov  3 11:25 muk.cfg
	-rw-r--r-- 1 root root      3081 nov  3 11:25 node1.txt
	-rw-r--r-- 1 root root       169 nov  3 11:25 rdisk.cfg
	-rw-r--r-- 1 root root      8862 nov  3 11:25 spread.txt


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


Para arrancar muk 
	cd /usr/src/dvs/vos/muk 
	nsenter -p -t$DC0 ./muk muk.cfg
	
	Finaliza el arranque de MUK con:
		DEBUG 2:dvk_receive_T:876: endpoint=31438 timeout=10000
		DEBUG 2:dvk_send_T:847: ipc ret=76
		 system.c:main_systask:62:SYSTASK is waiting for requests
		DEBUG 2:dvk_receive_T:876: endpoint=31438 timeout=-1
		BOOT TIME: t_start=1572791296.89 t_stop=1572791299.04 t_total=2.15 [s]
		 muk.c:main:525:JOINING ALL THREADS -------------------------------------
	
Abrir otra sesion y comprobar los procesos bindeados al DVK 

root@node0:~# cat /proc/dvs/DC0/procs 
DC pnr -endp -lpid/vpid- nd flag misc -getf -sndt -wmig -prxy name
 0  -2    -2   701/3      0    8  420 31438 27342 27342 27342 systask        
 0   0     0   704/5      0    8    0 31438 27342 27342 27342 pm             
 0   1     1   705/6      0    8    0 31438 27342 27342 27342 fs             
 0   3     3   703/4      0    8    0 31438 27342 27342 27342 rdisk          
 0   8     8   706/7      0    0    0 27342 27342 27342 27342 is             
 0  14    14   709/10     0    8    0 31438 27342 27342 27342 m3ftp          
 0  22    22   708/9      0    0    0 27342 27342 27342 27342 nweb    
 
 
Abriendo un explorador con la URL http://192.168.1.100:8080/
Se abre el Web Information Server 
MUK Web Information Server .
sys_dc_dmp (F1)	sys_proc_dmp (F2)	sys_stats_dmp (F3)	rd_dev_dmp (F5)	pm_proc_dmp (F6)	fs_proc_dmp (F7)	fs_super_dmp (F9)

MUK Web Information Server .
sys_dc_dmp (F1)	sys_proc_dmp (F2)	sys_stats_dmp (F3)	rd_dev_dmp (F5)	pm_proc_dmp (F6)	fs_proc_dmp (F7)	fs_super_dmp (F9)

DC	pnr	-endp	-lpid	-vpid	nd	------flags-----	----misc---	-getf	-sndt	-wmig	-prxy	name
0	-2	-2	701	3	0	----------------	-----L----U	-NONE	-NONE	-NONE	-NONE	systask
0	0	0	704	5	0	--R-------------	-----------	-ANY-	-NONE	-NONE	-NONE	pm
0	1	1	705	6	0	--R-------------	-----------	-ANY-	-NONE	-NONE	-NONE	fs
0	3	3	703	4	0	--R-------------	-----------	-ANY-	-NONE	-NONE	-NONE	rdisk
0	8	8	706	7	0	--R-------------	-----------	systa	-NONE	-NONE	-NONE	is
0	14	14	709	10	0	--R-------------	-----------	-ANY-	-NONE	-NONE	-NONE	m3ftp
0	22	22	708	9	0	----------------	-----------	-NONE	-NONE	-NONE	-NONE	nweb
Process Status Flags:
A: NO_MAP- keeps unmapped forked child from running
S: SENDING- process blocked trying to send
R: RECEIVING- process blocked trying to receive
I: SIGNALED- set when new kernel signal arrives
P: SIG_PENDING- unready while signal being processed
T: P_STOP- set when process is being traced
V: NO_PRIV- keep forked system process from running
Y: NO_PRIORITY- process has been stopped
e: NO_ENDPOINT- process cannot send or receive messages
c: ONCOPY- A copy request is pending
m: MIGRATING- the process is waiting that it ends its migration
r: REMOTE- the process is running on a remote host
o: RMTOPER- a process descriptor is just used for a remote operation
g: WAITMIGR- a destination process is migrating
u: WAITUNBIND- a process is waiting another unbinding
b: WAITBIND- a process is waiting another binding
Process Miscelaneous Flags:
X: MIS_PROXY- The process is a proxy
C: MIS_CONNECTED- The proxy is connected
N: MIS_BIT_NOTIFY- A notify is pending
M: MIS_NEEDMIG- The proccess need to migrate
B: MIS_RMTBACKUP- The proccess is a remote process' backup
L: MIS_GRPLEADER- The proccess is the thread group leader
K: MIS_KTHREAD- The proccess is a KERNEL thread
R: MIS_REPLICATED- The ep is LOCAL but it is replicated on other node
k: MIS_KILLED- The process has been killed
W: MIS_WOKENUP- The process has been signaled
U: MIS_UNIKERNEL- The process is running inside a local UNIKERNEL




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

MUK tiene un tamaño de 
root@node0:/usr/src/dvs/vos/muk# ls -l muk
-rwxr-xr-x 1 root root 2195808 oct 26 22:22 muk

Haciendo TOP
Tasks: 111 total,   1 running, 110 sleeping,   0 stopped,   0 zombie
%Cpu(s):  0,0 us,  0,2 sy,  0,0 ni, 99,8 id,  0,0 wa,  0,0 hi,  0,0 si,  0,0 st
KiB Mem :  2066644 total,  1679680 free,    65660 used,   321304 buff/cache
KiB Swap:   522236 total,   522236 free,        0 used.  1705168 avail Mem 

  PID USER      PR  NI    VIRT    RES    SHR S  %CPU %MEM     TIME+ COMMAND                        
  661 root       1 -19  151596   4948    196 S   0,3  0,2   0:00.13 muk    
  
Para ver la utilizacion de memoria en ejecuciòn:
root@node0:~# pmap 661
		661:   ./muk muk.cfg
		08048000   1012K r-x-- muk
		08145000     12K rw--- muk
		08148000  77220K rw---   [ anon ]
		0cdfc000    136K rw---   [ anon ]
		b2a00000    136K rw---   [ anon ]
		b2a22000    888K -----   [ anon ]
		b2bff000      4K -----   [ anon ]
		b2c00000   8192K rw---   [ anon ]
		b3400000    148K rw---   [ anon ]
		b3425000    876K -----   [ anon ]
		b35ff000      4K -----   [ anon ]
		b3600000   8192K rw---   [ anon ]
		b3e00000    132K rw---   [ anon ]
		b3e21000    892K -----   [ anon ]
		b3ffe000      4K -----   [ anon ]
		b3fff000   8192K rw---   [ anon ]
		b47ff000      4K -----   [ anon ]
		b4800000   8192K rw---   [ anon ]
		b5000000    132K rw---   [ anon ]
		b5021000    892K -----   [ anon ]
		b51ff000      4K -----   [ anon ]
		b5200000   8192K rw---   [ anon ]
		b5a00000    132K rw---   [ anon ]
		b5a21000    892K -----   [ anon ]
		b5bff000      4K -----   [ anon ]
		b5c00000   8192K rw---   [ anon ]
		b6400000    132K rw---   [ anon ]
		b6421000    892K -----   [ anon ]
		b65ff000      4K -----   [ anon ]
		b6600000   8192K rw---   [ anon ]
		b6e00000    148K rw---   [ anon ]
		b6e25000    876K -----   [ anon ]
		b6f4e000    204K rw---   [ anon ]
		b6f81000    128K rw-s- procs
		b6fa1000      4K -----   [ anon ]
		b6fa2000   8192K rw---   [ anon ]
		b77a2000      8K r----   [ anon ]
		b77a4000      8K r-x--   [ anon ]
		bfd5e000    132K rw---   [ stack ]
		 total   151596K


  