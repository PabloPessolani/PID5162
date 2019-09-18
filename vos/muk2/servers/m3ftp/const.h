// REQUESTS
#define FTP_NONE	0
#define FTP_GET		1
#define FTP_PUT		2
#define FTP_NEXT	3
#define FTP_CANCEL  4

#define FTPOPER 	m_type	// request type 
#define FTPPATH 	m1_p1 	// pathname address
#define FTPPLEN  	m1_i1 	// lenght of pathname 
#define FTPDATA 	m1_p2 	// DATA  address
#define FTPDLEN  	m1_i2 	// lenght of DATA 