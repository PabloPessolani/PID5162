//traídos de rdisk.h, para dejar todas las constantes juntas

/*For m_dtab*/
#define OPER_NAME 0
#define OPER_OPEN 1
#define OPER_NOP 2
#define OPER_IOCTL 3
#define OPER_PREPARE 4
#define OPER_TRANSF 5
#define OPER_CLEAN 6
#define OPER_GEOM 7
#define OPER_SIG 8
#define OPER_ALARM 9
#define OPER_CANC 10
#define OPER_SEL 11

/* MULTICAST MESSAGE TYPES */
#define		STS_DISCONNECTED	-1
#define RDISK_MULTICAST		0x80	
#define MC_STATUS_INFO     (RDISK_MULTICAST + 1)
#define MC_SYNCHRONIZED    (RDISK_MULTICAST + 2)
#define RD_SYNCHRONIZED    (RDISK_MULTICAST + 3)

#define MC_RADAR_INFO 		0xDA

#define		STS_SYNCHRONIZED	0
#define	    STS_NEW				1
#define		STS_WAIT4PRIMARY	2
#define		STS_WAIT4SYNC		3
#define		STS_LEAVE			4

#define NO_PRIMARY			(-1)

#define REPLICATE_NO		0
#define REPLICATE_YES		1
#define DONOT_UPDATE		0

#define DONOT_COMPRESS		0
#define DO_COMPRESS			1
#define COMP            1
#define UNCOMP          0

#define RDISK_TIMEOUT_SEC	5
#define RDISK_TIMEOUT_MSEC	0

//fin traídas de rdisk.h

#define DONOT_DYNUPDATES	0
#define DO_DYNUPDATES		1

#define 	RMTNODE	1
#define DRV_NR 0
#define BKDRV_NR (DRV_NR + 1)			/*ver esto si lo hago acá, o directamente fuera*/

#define DEV_TRANS	DEV_WRITE * 2		/*transfer*/
#define DEV_TRANSR	DEV_TRANS +1		/*replay transfer*/

#define DEV_CFULL	DEV_TRANSR + 1		/*full copy*/
#define DEV_CFULLR	DEV_CFULL + 1		/*reply - full copy*/

#define DEV_CMD5	DEV_CFULLR + 1		/*md5 update*/
#define DEV_CMD5R	DEV_CMD5 + 1		/*replay md5*/

#define DEV_UFULL	DEV_CMD5R + 1		/*updates copy*/
#define DEV_UFULLR	DEV_UFULL + 1		/*replay updates copy*/

#define DEV_UMD5	DEV_UFULLR + 1		/*only differents*/
#define DEV_UMD5R	DEV_UMD5 + 1		/*replay only differents*/

#define DEV_EOF 	DEV_UMD5R + 1		/*end device */
#define DEV_EOFR 	DEV_EOF + 1			/*replay end device */

#define RD_DISK_EOF DEV_EOFR + 1 		/*end rdisk-node */
#define RD_DISK_ERR RD_DISK_EOF + 1		/* error */

