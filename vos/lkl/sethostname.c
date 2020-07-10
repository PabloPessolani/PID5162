#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

size_t uk_strlen(const char *__s)
{
	return(0);
}

size_t uk_gethostname(const char *__s, int c)
{
	return(0);
}

size_t uk_sethostname(const char *__s, int c)
{
	return(0);
}

size_t uk_printf(const char *__s, ...)
{
	return(0);
}


#define strlen			uk_strlen
#define gethostname		uk_gethostname
#define sethostname		uk_sethostname
#define printf			uk_printf

int _start()
{
	char lkl[10] = "lklhost";
	char host[10];
	
	int rcode, len;
	
	len = strlen(lkl);

	rcode = gethostname(host, 10);
	printf("current hostname=%s\n",host); 
	
    rcode = sethostname(lkl, len);
	
	rcode = gethostname(host, 10);
	printf("new hostname=%s\n",host);	
	
	 /* exit system call */
    asm("movl $1,%eax;"
        "xorl %ebx,%ebx;"
        "int  $0x80"
    );
}
   