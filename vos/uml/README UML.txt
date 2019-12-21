USER MODE LINUX como Virtual Operating System
===============================================

El objetivo es que UML pueda utilizar las facilidades ofrecidas por el DVS.
Puede operar de la siguiente forma:
	- El driver RDISK y TAP (no implementado aun) pueden acceder a servidores externos (locales o remotos) utilizando M3-IPC
		Tanto RDISK como TAP son en realidad proxies de lo drivers reales.
	- Se creo rhostfs ( a partir de hostfs HOST Filesystem, que puede acceder al filesystem del HOST) que 
		permite montar a modo de NFS un filesystem remoto.
	- Se habilitó el proxy TCP por lo que se puede considerar como un nodo.
	
INCORPORACION DE RDISK COMO DRIVER DE UML
=========================================

Esto funciona de la siguiente forma 
   -----------------                    -----------
   |  UML DRIVER   |     M3-IPC         | RDISK   |
   |     RDISK     |--------------------| SERVER  |
   |     CLIENT    |                    | ep=3    |
   -----------------                    -----------

PARA HABILITAR LA COMPILACION DE UML_RDISK (mendiante la constante CONFIG_UML_RDISK)
Se modifico el archivo Kconfig.char 

		menu "UML Character Devices"

		config UML_RDISK 
			bool "UML Replicated Disk proxy"
			default y
			help
			This options enable RDISK proxy to the host RDISK.
			
			
		config UML_DVK 
			bool "UML Distributed Virtualization Kernel (DVK) pseudo character device"
			default y
			help
			This options enable DVK pseudo device driver as a pass-through to the host-DVK.
			

ATENCION!!! compilar el kernel del HOST con la opcion CONFIG_DVKIPC deshabilitada  !!!!!!!!!!!!!!!!!!!!			
			
EJECUCION EN SESION 1
		root@node0:/#cd /usr/src/dvs/dvk-tests
		root@node0:/usr/src/dvs/dvk-tests# ./tests.sh 0 0
	luego 
		root@node0:/usr/src/dvs/dvk-tests# cd /usr/src/dvs/vos/mol/drivers/rdisk
		root@node0:/usr/src/dvs/vos/mol/drivers/rdisk# ./rdisk.sh 0
			DC pnr -endp -lpid/vpid- nd flag misc -getf -sndt -wmig -prxy name
			0   3     3   785/785    0    8   20 31438 27342 27342 27342 rdisk   
		
EJECUCION EN SESION 2

		
		
