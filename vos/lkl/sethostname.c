#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv[])
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
	exit(0);
}
   