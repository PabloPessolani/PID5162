## Topología de Red
La virtualización se realizó con vmware, la configuración de red usada se presenta a continuación:

	RED 192.168.1.0/24
	PC(NAT) 192.168.1.1
		|_node0 192.168.1.100
		|_node1 192.168.1.101
		|_node2 192.168.1.102
		|_ . . .
		|_nodeXX 192.168.1.1XX


## Poniéndolo en vmware

	sudo vmware-netcfg

En el NAT completar
subnet IP = 192.168.1.0
subnet mask  = 255.255.255.0

## Configurando cada nodo

En cada nodo editar el archivo /etc/network/interfaces con el siguiente comando:

	nano /etc/network/interfaces

Para que contenga lo siguiente:

	source /etc/network/interfaces.d/*
	auto lo
	iface lo inet loopback
		address 192.168.1.1XX
		netmask 255.255.255.0
		network 192.168.1.0
		broadcast 192.168.1.255
		gateway 192.168.1.2

donde XX corresponde al numero de nodo (ej: 00, 01, 02, etc.)

Y el archivo /etc/hostname debe contener el valor

	nodeXX

donde XX corresponde al numero de nodo (ej: node1, node2, etc.)

## Formato requerido en archivo de configuración radar
En nuestro caso el servicio de rdisk arranca con endpoint 3, y usamos el DC 0, por lo tanto el archivo de configuracion contiene:

	service RDISK0 {
		replica		RPB
		dcid  		0;
		endpoint 	3;
		group		"RDISK";
	};

(replica: tipo de funcionamiento, RPB o FSM; dcid: identificador de DC; endpoint: endpoint por defecto a usar por el servicio a monitorear
 group: identificador de grupo de SPREAD)

## Inicialización del DVS para la prueba del proyecto RADAR

En la carpeta scripts del proyecto hay dos archivos .sh que realizan el proceso de inicialización por defecto del DVS, él inicio del contenedor distribuido y 
el proceso de rdisk con el que se probó la aplicación. Los archivos se llaman rdisk_radar.sh y rdisk.sh. A continuación se explicará en detalle que hace cada uno.

En el servidor primario (que en este ejemplo corre en el nodo0), corriendo rdisk_radar.sh se realiza el inicio y configuracion del DVS para luego levantar el servicio de rdisk.
Para hacerlo ejecutar lo siguiente:

	cd /usr/src/dvs/scripts
	./rdisk_radar.sh 0

El argumento pasado al script representa el nodo donde este se esta ejecutando. 

NOTA: Al ejecutar la ultima linea, se puede solicitar numerosas veces que se presione ENTER para continuar, esto es para que se puedan
	visualizar los mensajes de control/depuracion mostrados. En cierto momento, se imprimira por pantalla el texto "sleep looping", ahi
	debe volverse a presionar ENTER aunque no se indique para que el proceso siga.

En el nodo 1 repetimos la operación para así levantar el servicio de rdisk nuevamente, pero esta vez, el nodo1 representa el servidor de backup.
AHORA SE DEBE PASAR COMO ARGUMENTO 1:

	cd /usr/src/dvs/scripts
	./rdisk_radar.sh 1

En el nodo donde corre el radar (en este caso nodo2), se inicia también el DVS y se une al contenedor distribuido DC0 (por defecto). Esto se 
logra con el script rdisk.sh con argumentos 2 (nodo donde se ejecuta el script) y 0 (DC ID) respectivamente.

	cd /usr/src/dvs/scripts
	./rdisk.sh 2 0

NOTA: Al ejecutar la ultima linea, se puede solicitar numerosas veces que se presione ENTER para continuar, esto es para que se puedan
	visualizar los mensajes de control/depuracion mostrados. En cierto momento, se imprimira por pantalla el texto "sleep looping", ahi
	debe volverse a presionar ENTER aunque no se indique para que el proceso siga.

Luego, en el nodo2 se inicia el servicio de radar con su archivo compilado:

	cd /usr/src/dvs/dvs-apps/radar
	./radar radar.cfg

Como argumento se pasa el nombre del archivo de configuracion por defecto a utilizar por radar explicado anteriormente.
	
Desde el nodo2 (donde corre radar) se puede ver el estado del proceso remoto utilizado en este ejemplo (rdisk). Para lograrlo, abrir una segunda
terminal en el mismo nodo mediante la combinacion CTRL+ALT+F2 (esto puede evitarse ejecutando el servicio en segundo plano) y luego ejecutar:

	cat /proc/dvs/DC0/procs 

La salida tiene que ser por el estilo:

	DC pnr -endp -lpid/vpid- nd flag misc -getf -sndt -wmig -prxy name
	0   3     3    -1/-1     0  1000    0 27342 27342 27342 27342 RDISK0 
	
El proceso remoto fue identificado con el número 3 y podemos ver que se asignó un valor de flag 1000 que representa que dicho proceso es remoto y alcanzable
(activo). En la columna “nd” se nos informa que nodo oficia de servidor primario.
En caso de una desconexión eventual de la placa de red del nodo0, el mismo comando debería cambiar su salida, mostrando que ahora el primario se ejecuta sobre el nodo1
si es que existe un servidor backup corriendo ahi. En caso contrario se observara un valor de flag 1800 dado que el servicio no es mas alcanzable (con estado desconocido).

## Pruebas de reconexion y redireccion
Como se menciono antes, para poner a prueba la capacidad de radar de determinar si el servidor primario esta caido o identificar y ubicar un servidor backup, 
en distintos escenarios uno simplemente deberia deshabilitar las placas de red de los nodos respectivos desde el gestor de maquinas virtuales que este 
siendo utilizado (si es que se realizan las pruebas de esta forma), en este caso VmWare. Ademas se puede variar las configuraciones, por ejemplo iniciando un
servidor backup o no.

## Prueba con Cliente de RDISK en NODE2
En esta prueba se iniciara un cliente de rdisk en node2 para probar la capacidad de funcionamiento del mismo luego 
de algun cambio en la ubicacion del servidor primario mientras se envia informacion (mensajes) hacia el servidor.

Topologia

	NODE0: RDISK PRIMARIO
	NODE1: RDISK BACKUP
	NODE2: RDISK_CLIENT + RADAR
 
Ejecutar los comandos explicados previamente para iniciar tanto el DVS como el servicio de RDISK en nodos 0 y 1.

En NODE2, luego de ejecutar los comandos correspondientes previamente explicados para iniciar el DVS y el servicio de RADAR,
uno puede iniciar un cliente del servicio rdisk en ejecutando los siguientes comandos (posiblemente en una nueva terminal):
	
	cd 
	cd ../usr/src/dvs/dvs-apps/radar 
 	./test_radar 0 3 70

donde 0 es el identificador de DC, 3 es el numero de endpoint y 70 representa el numero de endpoint asignado al proceso cliente de rdisk. Este cliente enviará 
30 mensajes, uno por segundo, hacia el servidor RDISK primario actual.

Si ejecutamos el siguiente comando en NODE0 (cuando comienza el envio de mensajes):

	cat /proc/dvs/DC0/procs 

Veremos el endpoint del cliente bindeado automaticamente al nodo correcto en NODE0:

	DC pnr -endp -lpid/vpid- nd flag misc -getf -sndt -wmig -prxy name
	0   3     3   641/4      0    8   20  31438 27342 27342 27342 rdisk          
	0  70    70   -1/-1      2  1000   0  27342 27342 27342 27342 rclient 
	
Si ahora detenemos el proceso de RDISK en NODE0, NODE1 deberia
pasar a oficiar ahora de PRIMARIO. Para hacerlo ejecutamos lo siguiente en NODO0:

. /usr/src/dvs/dvs-apps/dc_init/DC0.sh		(NOTAR EL PUNTO Y EL ESPACIO AL INICIO DEL COMANDO)
nsenter -p -u -F -t$DC0 pkill rdisk

Ejecutando "cat /proc/dvs/DC0/procs" en NODE1 vemos:
	DC pnr -endp -lpid/vpid- nd flag misc -getf -sndt -wmig -prxy name
	0   3     3   633/4      1    8   20 31438 27342 27342 27342 rdisk
	0  70    70    -1/-1     2  1000   0 27342 27342 27342 27342 rclient  

Y, con el mismo comando en NODE2:
	DC pnr -endp -lpid/vpid- nd flag misc -getf -sndt -wmig -prxy name
	0   3     3    -1/-1     1 1000    0 27342 27342 27342 27342 RDISK0

Si todo funciona correctamente, el envio de mensajes desde el NODE2 no sera interrumpido (solo se notará un pequeño retardo) al pasar de usar el servidor 
NODE0 como primario a usar a su antiguo backup en NODE1 como nuevo primario.