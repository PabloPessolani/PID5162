
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>		/* open */
#include <unistd.h>		/* exit */
#include <errno.h>		/* errno */
#include <sys/ioctl.h>		/* ioctl */

#define DVS_USERSPACE	1

#include "../include/com/dvs_config.h"
#include "../include/com/config.h"
#include "../include/com/com.h"
#include "../include/com/const.h"
#include "../include/com/cmd.h"
#include "../include/com/proc_sts.h"
#include "../include/com/proc_usr.h"
#include "../include/com/proxy_sts.h"
#include "../include/com/proxy_usr.h"
#include "../include/com/dc_usr.h"
#include "../include/com/node_usr.h"
#include "../include/com/priv_usr.h"
#include "../include/com/dvs_usr.h"
#include "../include/com/dvk_calls.h"
#include "../include/com/dvk_ioctl.h"
#include "../include/com/dvs_errno.h"

main()
{
	int fd, rcode;

	fd = open(DVK_FILE_NAME, 0);
	if (fd < 0) {
		printf("Can't open device file: %s errno=%d\n", DVK_FILE_NAME, errno);
		exit(-1);
	}

	rcode = ioctl(fd, DVK_IOCTDVSEND);
	printf("ioctl DVK_IOCTDVSEND=%X rcode=%d\n", DVK_IOCTDVSEND, rcode);
	
	close(fd);
}
