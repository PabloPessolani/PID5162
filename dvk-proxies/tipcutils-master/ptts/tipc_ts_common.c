/* ------------------------------------------------------------------------
 *
 * tipc_ts_common.c
 *
 * Short description: Portable TIPC Test Suite -- common client+server routines
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2006,2008,2010 Wind River Systems
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the names of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * ------------------------------------------------------------------------
 */


#include "tipc_ts.h" /* must use " for rtp projects */




/**
 * nameList - 	list of all test names, must be in sync with:
 * 		serverList[] in tipc_ts_server.c and
 * 		clientList[] in tipc_ts_client.c and
 * 		TS_NUM in tipc_ts.h
 */

TSTESTNAME nameList[] = {
	{ts_doAllTests, "do all sanity"},
	{ts_dgram, "connectionless (SOCK_DGRAM)"},
	{ts_rdm, "connectionless (SOCK_RDM)"},
	{ts_conn_stream, "connection (SOCK_STREAM)"},
	{ts_conn_seqpacket, "connection (SOCK_SEQPACKET)"},
	{ts_shutdown_stream, "connection shutdown (SOCK_STREAM)"},
	{ts_shutdown_seqpacket, "connection shutdown (SOCK_SEQPACKET)"},
	{ts_messageLimits, "message size limits (SOCK_RDM)"},
	{ts_importance, "TIPC_IMPORTANCE"},
	{ts_socketOptions, "socket options"},
	{ts_connection_anc, "Connection Ancillary Data (SOCK_SEQPACKET)"},
	{ts_connectionless_anc, "Connectionless Ancillary Data (SOCK_RDM)"},
	{ts_multicast, "Multicast (RDM)"},
	{ts_stream, "fragmented message (SOCK_STREAM)"},
	{ts_bigStream, "message > 66000 bytes (SOCK_STREAM)"},
	{ts_sendto, "sendto - recvfrom (SOCK_RDM)"},
#if DO_BLAST
	{ts_blast_rdm, "blaster/blastee (SOCK_RDM)"},
	{ts_blast_seqpacket, "blaster/blastee (SOCK_SEQPACKET)"},
	{ts_blast_stream, "blaster/blastee (SOCK_STREAM)"},
#endif
	{ts_lastSanityTest, ""},
	{ts_stress_rdm,"continuous SOCK_RDM"},
	{ts_lastStressTest, ""}
};

#define NAMELIST_SIZE  sizeof(nameList)/sizeof(nameList[0])

/**
 * testName - returns the name of the test from the nameList structure
 */
char * testName
(
        int test	 /* number of the test */
)
{
	int testIndex;  /* loop index to traverse the nameList */

	for (testIndex =0; testIndex < NAMELIST_SIZE; testIndex++) {
		if (nameList[testIndex].testNum == test)
			return(nameList[testIndex].name);
	}
	return("");
}

/**
 * failTest - indicates the completion of an unsuccessful test
 */

void failTest
(
        char *reason  /* string indicating why the failure occured */
)
{
	char failString[150]; /* string for the failure message */

	if (reason == NULL)
		reason = "no reason for failure given";
	sprintf (failString,"TEST FAILED %s errno = %d", reason, errno);
	perror(failString);	   /* append the system error codes to the error */

	killme(1);  /* kill the process */
}

/**
 * makeArray - create test vector with a specified pattern of numbers
 */

void makeArray
(
        char *theArray,	  /* pointer to the array to fill in */
        int size,	  	  /* size of the array */
        int start,	  	  /* where the message starts */
        int end		      /* where the message ends */
)
{
	int curNum;       /* current index to the message array */
	char errCheck;    /* error checksum */
	int i;            /* loop index to access the array */

	/* Validate arguments */

	if ((start < 0) || (start > 255) || (start > end)) {
		dbg1 ("Using default data start (0)\n");
		start = 0;
	}
	if ((end < 0) || (end > 255)) {
		dbg1 ("Using default data end (255)\n");
		end = 255;
	}

	/* Generate short (or null) message & return if no room for header */

	if (size < 7) {
		for (i = 0; i < size; i++)
			theArray[i] = (char)i;
		return;
	}

	/* Create message descriptor portion of header */

	curNum = size;
	for (i = 0; i < 4; i++) {
		theArray[i] = (char)(curNum & 0xFF);
		curNum >>= 8;
	}
	theArray[4] = (char)start;
	theArray[5] = (char)end;

	/* Create header error check */

	errCheck = 0;
	for (i = 0; i <= 5; i++)
		errCheck ^= (char)theArray[i];
	theArray[6] = (errCheck ^ 0xFF);

	/* Fill rest of array with specified data pattern */

	curNum = start;
	for (i = 7; i < size; i++) {
		theArray[i] = (char)curNum;
		curNum++;
		if (curNum > end)
			curNum = start;
	}
}

/**
 * checkArray - validates the integrity of a test vector
 *
 * RETURNS: # of errors found
 */

int checkArray
(
        char *theArray,	  /* the message to check */
        int arraySize	  /* the size of the message */
)
{
	int theSize;     /* the size of the message to validate */
	int start;       /* index to where the message starts */
	int end;         /* index to where the message ends */
	char errCheck;   /* used to validate the checksum */
	int i;           /* loop index to look at the message  */
	int errCount;    /* a count of errors found */
	int curNum;      /* current index into the message */

	/* Check short (or null) message & return */

	if (arraySize < 7) {
		errCount = 0;
		for (i = 0; i < arraySize; i++) {
			if ((unsigned char)theArray[i] != i)
				errCount++;
		}
		if (errCount > 0)
			dbg1 ("Not a valid %d byte message\n", arraySize);
		return errCount;
	}

	/* Validate header */

	errCheck = 0;
	for (i = 0; i < 6; i++)
		errCheck ^= theArray[i];
	errCheck ^= 0xFF;
	if ((errCheck ^ theArray[6]) & 0xFF) {
		printf ("Header checksum error\n");
		for (i = 0; i <= 6; i++)
			dbg1 ("Header[%d]=0x%0x\n", i, theArray[i]);
		dbg1 ("Computed checksum=0x%0x\n", errCheck);
		return 1;
	}

	/* Validate message size */

	theSize = 0;
	for (i = 4; i > 0; ) {
		theSize <<= 8;
		theSize |= (unsigned char)theArray[--i];
	}
	if (theSize != arraySize) {
		printf ("Message size mismatch\n");
		return 1;
	}

	start = (unsigned char)theArray[4];
	end = (unsigned char)theArray[5];

	/* Validate message data */

	errCount = 0;
	curNum = start;
	for (i = 7; i < theSize; i++) {
		if (curNum != (unsigned char)theArray[i]) {
			if (++errCount == 1)
				dbg1 ("First data mismatch at offset %d\n", i);
		}
		curNum++;
		if (curNum > end)
			curNum = start;
	}
	return errCount;
}


/**
 * acceptSocketTIPC - accepts a socket connection (ignores peer address)
 */

int acceptSocketTIPC
(
        int sockfd_L		/* socket to use */
)
{
	struct sockaddr_tipc addr;    /* socket address structure not filled in */
	socklen_t addrlen;            /* length of the address */
	int sockfd_N;                 /* returned socket */

	addrlen = sizeof (addr);
	sockfd_N = accept (sockfd_L, (struct sockaddr *)&addr, &addrlen);
	if (sockfd_N < 0)
		err("accept() error");
	return sockfd_N;
}

/**
 * listenSocketTIPC - allows socket to listen for connections sets max queue length to 5
 */

void listenSocketTIPC
(
        int sockfd_S  /* socket to use */
)
{
	if (listen (sockfd_S, 5) <0)
		err("listen() error");
}

/**
 * sendtoSocketBuffTIPC - sends supplied messages via a connectionless socket
 *
 * "sendErrorTarget" specifies expected number of send() errors;
 * if < 0, then any number of errors is OK
 */

void sendtoSocketBuffTIPC
(
        int sockfd,				/* socket to be used */
        struct sockaddr_tipc *addr,	/* address of the socket */
        char *msgArea,				/* pointer to the message to be sent */
        int numTimes,				/* number of times to send the message */
        int msgSize,				/* size of the message to be sent */
        int sendErrorTarget			/* number of expected errors */
)
{
	int sendErrorCount;		 /* local count of the errors */
	int i = 0;				 /* loop variable */
	int res;				 /* return code */
	char failStr[100]; 	 	 /* string for the failure message */

	if ((numTimes == 0) || (msgSize == 0))
		return;

	sendErrorCount = 0;

	while (i < numTimes) {

		res = sendto (sockfd, msgArea, msgSize, 0,
		              (struct sockaddr *)addr, sizeof (*addr));
		dbg1("sent a message %d\n",i);

		if (res != msgSize)
			sendErrorCount++;

		i++;
	}

	if ((sendErrorTarget >= 0) && (sendErrorCount != sendErrorTarget)) {
		sprintf(failStr,"sendto() errors %d expected %d Message Size = %d",sendErrorCount, sendErrorTarget,msgSize);
		err(failStr);
	}


}

/**
 * sendtoSocketTIPC - sends messages via a connectionless socket
 *
 * "sendErrorTarget" specifies expected number of send() errors;
 * if < 0, then any number of errors is OK
 */

void sendtoSocketTIPC
(
        int sockfd,				/* socket to be used */
        struct sockaddr_tipc *addr,	/* address of the socket */
        int numTimes,				/* number of times to send the message */
        int msgSize,				/* size of the message to be sent */
        int sendErrorTarget			/* number of expected errors */
)
{
	char *msgArea;	  /* local pointer to the message */

	if ((numTimes == 0) || (msgSize == 0))
		return;

	msgArea = (char *)malloc (msgSize);
	if (msgArea == NULL)
		err("unable to allocate send buffer");

	makeArray (msgArea, msgSize, 0, 255);
	sendtoSocketBuffTIPC (sockfd, addr, msgArea, numTimes, msgSize,
	                      sendErrorTarget);
	free (msgArea);
}

/**
 * anc_data_type - utility to get the anc type as a string
 */

void anc_data_type
(
        char * str,  /* string with type of ancillary data */
        int type     /* anc type */
)
{
	switch (type) {
	case TIPC_DESTNAME:
		strcpy (str, "TIPC_DESTNAME");
		break;
	case TIPC_ERRINFO:
		strcpy (str, "TIPC_ERRINFO");
		break;
	case TIPC_RETDATA:
		strcpy (str, "TIPC_RETDATA");
		break;
	default:
		strcpy (str, "UNKNOWN!!");
		break;
	}
}

/**
 * setOption - set the TIPC Socket option
 */

void setOption
(
        int sockfd,	   /* socket to use */
        int opt,	   /* option to set */
        int value	   /* value to set */
)
{
	char failString[50]; /* string for failure return code */

	if (setsockopt (sockfd, SOL_TIPC, opt,
	                (char*)&value, sizeof (value))) {
		sprintf(failString,"unable to set option %d to %d", opt, value);
		err(failString);
	}
}

/**
 * getOption - get the TIPC Socket option
 */

void getOption
(
        int sockfd,    /* socket to use */
        int opt,       /* option to get */
        int *value     /* returned value */
)
{
	char failString[50];  /* string for failure return code */
	socklen_t size;       /* size of the socket option value */

	size = sizeof(*value);

	if (getsockopt (sockfd, SOL_TIPC, opt, (char*)value, &size)) {
		sprintf(failString,"unable to get option %d", opt);
		err(failString);
	}
}


/**
 * tipc_printaddr - print out a TIPC socket address in readable form
 *
 * note will only print when debug enabled
 */

void tipc_printaddr
(
        struct sockaddr_tipc *addr     /* pointer to TIPC address structure */
)
{
	dbg1 ("%s:",
	       (addr->family == AF_TIPC) ? "AF_TIPC" : "invalid family");
	switch (addr->addrtype) {
	case TIPC_ADDR_ID:
		dbg1 ("ID=<%d.%d.%d>,%d",
		       tipc_zone (addr->addr.id.node),
		       tipc_cluster (addr->addr.id.node),
		       tipc_node (addr->addr.id.node),
		       addr->addr.id.ref);
		break;
	case TIPC_ADDR_NAME:
		dbg1 ("NAME=(%d,%d),%d",
		       addr->addr.name.name.type, addr->addr.name.name.instance,
		       addr->addr.name.domain);
		break;
	case TIPC_ADDR_NAMESEQ:
		/* case TIPC_ADDR_MCAST:  same value as TIPC_ADDR_NAMESEQ */
		dbg1 ("NAMESEQ=(%d,%d,%d)",
		       addr->addr.nameseq.type, addr->addr.nameseq.lower,
		       addr->addr.nameseq.upper);
		break;
	default:
		printf ("invalid type");
		break;
	}

	/* don't bother printing scope, since it is only used by bind() */
}

/**
 *
 * setServerAddr - sets address of regression test server
 *
 */

void setServerAddr
(
        struct sockaddr_tipc *addr  /* pointer to address structure */
)
{
	addr->family = AF_TIPC;
	addr->addrtype = TIPC_ADDR_NAME;
	addr->scope = TIPC_ZONE_SCOPE;
	addr->addr.name.name.type = TS_TEST_TYPE;
	addr->addr.name.name.instance = TS_TEST_INST;
	addr->addr.name.domain = 0;
}

/**
 *
 * setServerAddrTo - sets address to a specific value
 *
 */

void setServerAddrTo
(
        struct sockaddr_tipc *addr,   /* pointer to address structure */
        int infoType,		      /* address type */
        int info1,		      /* address info 1 */
        int info2,		      /* address info 2 */
        int info3		      /* address info 3 */
)
{
	addr->family = AF_TIPC;
	addr->scope = TIPC_ZONE_SCOPE;
	addr->addrtype = infoType;

	switch (infoType) {
	case TIPC_ADDR_NAME:
		addr->addr.name.name.type = info1;
		addr->addr.name.name.instance = info2;
		addr->addr.name.domain = info3;
		break;
	case TIPC_ADDR_NAMESEQ:
		/* case TIPC_ADDR_MCAST:  same value as TIPC_ADDR_NAMESEQ */
		addr->addr.nameseq.type = info1;
		addr->addr.nameseq.lower = info2;
		addr->addr.nameseq.upper = info3;
		break;
	case TIPC_ADDR_ID:
		addr->addr.id.node = info1;
		addr->addr.id.ref = info2;
		break;
	default:
		err("Invalid address type used in test");
		break;
	}
}

/**
 * createSocketTIPC - create a TIPC socket
 */

int createSocketTIPC
(
        int type     /* socket type to create SOCK_STREAM/SOCK_SEQPACKET/SOCK_DGRAM/SOCK_RDM */
)
{
	int sockfd_N; /* socket created */

	sockfd_N = socket (AF_TIPC, type, 0);
	if (sockfd_N < 0)
		err("socket() error");

	return sockfd_N;
}

/**
 * bindSocketTIPC - binds TIPC name to a socket
 */

void bindSocketTIPC
(
        int sockfd_S,		       /* socket to use */
        struct sockaddr_tipc *addr     /* address to use */
)
{
	if (bind (sockfd_S, (struct sockaddr *)addr, sizeof (*addr)) < 0) {
		tipc_printaddr(addr);
		err("bind() error");
	}

}

#if 0		 /* not implemented yet */
/**
 * acceptPeerSocketTIPC - accepts a socket connection (captures peer address)
 */


LOCALS int acceptPeerSocketTIPC
(
        int sockfd_L,			 /* socket to accept on */
        struct sockaddr_tipc *peerAddr,	 /* pointer to peer address */
        int *peerAddrLen		 /* pointer to peer address length */
)
{
	int sockfd_N;                    /* socket from accept */

	sockfd_N = accept (sockfd_L, (struct sockaddr *)peerAddr, peerAddrLen);
	if (sockfd_N < 0)
		err("accept() error");
	return sockfd_N;
}
#endif

/**
 * connectSocketTIPC - connects to a socket
 */

void connectSocketTIPC
(
        int sockfd_C,		    /* socket to use */
        struct sockaddr_tipc *addr  /* address to use */
)
{
	if (connect (sockfd_C, (struct sockaddr *)addr, sizeof (*addr)) < 0) {
		tipc_printaddr(addr);
		err("connect() error");
	}

}

/**
 * closeSocketTIPC - closes specified socket
 */

void closeSocketTIPC
(
        int sockfd_C		/* socket to close */
)
{
	if (close (sockfd_C) < 0)
		err("close() error");
}

/**
 * sendSocketBuffTIPC - sends the supplied messages via a connected socket
 *
 * "sendErrorTarget" specifies expected number of send() errors;
 * if < 0, then any number of errors is OK
 */

void sendSocketBuffTIPC
(
        int sockfd,	    /* socket to send on */
        char *msgArea,	    /* pointer to the message to send */
        int numTimes,	    /* number of messages to send */
        int msgSize,	    /* size of messages to send */
        int sendErrorTarget /* number of send errors to expect */
)
{
	int sendErrorCount; /* error counter */
	int i;              /* loop counter for the number of messages sent */
	int res;            /* return code from send */

	if ((numTimes == 0) || (msgSize == 0))
		return;

	sendErrorCount = 0;

	for (i = 0; i < numTimes; i++) {
		res = send (sockfd, msgArea, msgSize, 0);
		dbg1("sent a message %d \n",i);

		if (res != msgSize) {
			sendErrorCount++;
			dbg1("res = %d msgSize = %d\n",res, msgSize);
		}


		if ((sendErrorTarget >= 0) && (sendErrorCount != sendErrorTarget)) {
			dbg1("sendErrorCount = %d sendErrorTarget = %d\n",sendErrorCount, sendErrorTarget);
			err("unexpected number of send() errors");
		}
	}
}

/**
 * sendSocketTIPC - sends messages via a connected socket
 *
 * "sendErrorTarget" specifies expected number of send() errors;
 * if < 0, then any number of errors is OK
 */

void sendSocketTIPC
(
        int sockfd,	    /* socket to send on */
        int numTimes,	    /* number of messages to send */
        int msgSize,	    /* size of the message to send */
        int sendErrorTarget /* number of send errors to expect */
)
{
	char *msgArea;      /*  */

	if ((numTimes == 0) || (msgSize == 0))
		return;

	msgArea = (char*)malloc (msgSize);
	if (msgArea == NULL)
		err("unable to allocate send buffer");

	makeArray (msgArea, msgSize, 0, 255);
	sendSocketBuffTIPC (sockfd, msgArea, numTimes, msgSize, sendErrorTarget);

	free (msgArea);
}

/**
 * recvSocketTIPC - receives messages from a socket
 *
 * "numTimes" specifies expected number of received messages;
 * if < 0, then any number of messages is OK (quits if connection closed)
 *
 * "recvErrorTarget" specifies expected number of recv() errors;
 * if < 0, then any number of errors is OK
 *
 * "checkErrorTarget" specifies expected number of corrupted messages;
 * if < 0, then any number of corrupted messages is OK (i.e. don't bother checking)
 *
 * Note: a recv() error also counts as a corrupted message!
 *
 */

void recvSocketTIPC
(
        int sockfd,	      /* socket to receive on */
        int numTimes,	      /* number of messages to receive */
        int maxSize,	      /* size of the messages to receive */
        int recvErrorTarget,  /* number of recv errors to expect */
        int checkErrorTarget  /* number of check errors to expect */
)
{
	char *msgArea;        /* pointer to the message area to be allocated */
	int recvErrorCount;   /* count of the receive errors */
	int checkErrorCount;  /* count of the check errors */
	int res;              /* return code for receive */

	if ((numTimes == 0) || (maxSize == 0))
		return;

	msgArea = (char*)malloc (maxSize);	/* currently only call free for the success path */
	/* failure path results in application being killed */
	if (msgArea == NULL)
		err("unable to allocate receive buffer");

	recvErrorCount = 0;
	checkErrorCount = 0;

	while ((numTimes < 0) || (numTimes-- != 0)) {
		res = recv (sockfd, msgArea, maxSize, MSG_WAITALL);
		dbg2("got a message %d\n",numTimes);

		if (res == 0) {
			if (numTimes < 0)
				break;
			else if (numTimes >= 0)
				err("recv() returned unexpected disconnect");
		}
		if (res < 0) {
			recvErrorCount++;
			checkErrorCount++;
		} else if ((checkErrorTarget >= 0) && checkArray (msgArea, res) != 0) {
			checkErrorCount++;
		}
	}

	if ((recvErrorTarget >= 0) && (recvErrorCount != recvErrorTarget))
		err("unexpected number of recv() errors");
	if ((checkErrorTarget >= 0) && (checkErrorCount != checkErrorTarget))
		err("unexpected number of corrupted messages received");

	free (msgArea);
}

/**
 * sendSyncTIPC - sends a synchronization signal & waits for acknowledgement
 */

void sendSyncTIPC
(
        int sigInstance		  /* sync number to publish */
)
{
	int sockfd_R;               /* socket used for signal acknowledement */
	struct sockaddr_tipc addr;  /* address to use */

	dbg1 ("sending synchronization signal %d\n", sigInstance);
	setServerAddrTo (&addr, TIPC_ADDR_NAME, TS_SYNCRO_TYPE, sigInstance, 0);
	sockfd_R = createSocketTIPC (SOCK_RDM);
	bindSocketTIPC (sockfd_R, &addr);
	dbg1 ("sent synchronization signal %d\n", sigInstance);
	if (recv (sockfd_R, (char *)&addr, 1, 0) != 1)
		err("synchronization signal not acknowledged");
	closeSocketTIPC (sockfd_R);
	dbg1 ("got ack for synchronization signal %d \n", sigInstance);
}

/**
 * recvSyncTIPC - waits for synchronization signal & generates acknowledgement
 */

void recvSyncTIPC
(
        int sigInstance				/* sync number to publish */
)
{
	int sockfd_S;               /* socket used for signal detection */
	int sockfd_R;               /* socket used for signal acknowledement */
	struct sockaddr_tipc addr;  /* address to use */
	struct tipc_subscr subscr;  /* subscription structure to fill in */
	struct tipc_event event;    /* tipc event structure */

	/* Wait for occurrence of synchronization signal */

	dbg1 ("detecting synchronization signal %d\n", sigInstance);

	sockfd_S = createSocketTIPC (SOCK_SEQPACKET);

	setServerAddrTo (&addr, TIPC_ADDR_NAME, TIPC_TOP_SRV, TIPC_TOP_SRV, 0);
	connectSocketTIPC(sockfd_S, &addr);

	subscr.seq.type = htonl(TS_SYNCRO_TYPE);
	subscr.seq.lower = htonl(sigInstance);
	subscr.seq.upper = htonl(sigInstance);
	subscr.timeout = htonl(TIPC_WAIT_FOREVER);
	subscr.filter = htonl(TIPC_SUB_SERVICE);

	if (send(sockfd_S, (char *)&subscr, sizeof(subscr), 0) != sizeof(subscr))
		err("Failed to send subscription");
	if (recv(sockfd_S, (char *)&event, sizeof(event), 0) != sizeof(event))
		err("Failed to receive event");
	if (event.event != htonl(TIPC_PUBLISHED))
		err("Signal %d not detected\n");

	/* Acknowledge receipt of synchronization signal */

	dbg1 ("acknowledging synchronization signal %d\n", sigInstance);

	sockfd_R = createSocketTIPC (SOCK_RDM);

	/* synchronization messages mustn't be discarded when congested */
	setOption(sockfd_R, TIPC_IMPORTANCE, TIPC_CRITICAL_IMPORTANCE);

	setServerAddrTo (&addr, TIPC_ADDR_NAME, TS_SYNCRO_TYPE, sigInstance, 0);
	if (sendto (sockfd_R, (caddr_t)&subscr, 1, 0,
	                (struct sockaddr *)&addr, sizeof (addr)) != 1)
		err("can't acknowledge synchronization signal");

	closeSocketTIPC (sockfd_R);

	/* Wait for synchronization signal to end (to prevent redetection) */

	dbg1 ("detecting end of synchronization signal %d\n", sigInstance);

	if (recv(sockfd_S, (char *)&event, sizeof(event), 0) != sizeof(event))
		err("Failed to receive event");
	if (event.event != htonl(TIPC_WITHDRAWN))
		err("Signal %d not detected\n");

	closeSocketTIPC (sockfd_S);

	dbg1 ("end of synchronization signal %d\n", sigInstance);
}

/**
 * getServerAddr - get port id address of regression test server
 */

void getServerAddr
(
        struct sockaddr_tipc *addr  /* pointer to address structure */
)
{
	int sockfd_X;               /* socket */
	struct sockaddr_tipc topsrv_addr;  /* another address structure */
	struct tipc_subscr subscr;  /* subscription structure to fill in */
	struct tipc_event event;    /* tipc event structure */

	/* Wait for occurrence of synchronization signal */

	sockfd_X = createSocketTIPC (SOCK_SEQPACKET);

	setServerAddrTo (&topsrv_addr, TIPC_ADDR_NAME, TIPC_TOP_SRV, TIPC_TOP_SRV, 0);
	connectSocketTIPC(sockfd_X, &topsrv_addr);

	subscr.seq.type = htonl(TS_TEST_TYPE);
	subscr.seq.lower = htonl(TS_TEST_INST);
	subscr.seq.upper = htonl(TS_TEST_INST);
	subscr.timeout = htonl(TIPC_WAIT_FOREVER);
	subscr.filter = htonl(TIPC_SUB_SERVICE);

	if (send(sockfd_X, (char *)&subscr, sizeof(subscr), 0) != sizeof(subscr))
		err("Failed to send subscription");
	if (recv(sockfd_X, (char *)&event, sizeof(event), 0) != sizeof(event))
		err("Failed to receive event");
	if (event.event != htonl(TIPC_PUBLISHED))
		err("Signal %d not detected\n");

	closeSocketTIPC (sockfd_X);

	setServerAddrTo (addr, TIPC_ADDR_ID,
	                 ntohl(event.port.node), ntohl(event.port.ref), 0);
}

/**
 * typeString - return a string for the socket type
 */
char * typeString
(
        const int a_type, /* index of socket type from  common_test_socketOptions() */
        char * a_buffer	  /* string for the socket name */
)
{
	switch (a_type) {
	case 0:
		strcpy (a_buffer ,"SOCK_STREAM");
		break;
	case 1:
		strcpy (a_buffer ,"SOCK_SEQPACKET");
		break;
	case 2:
		strcpy (a_buffer ,"SOCK_RDM");
		break;
	case 3:
		strcpy (a_buffer ,"SOCK_DGRAM");
		break;
	}
	return a_buffer;
}

/**
 * common_test_socketOptions - test the socket options on both the client and server
 */

void common_test_socketOptions(void)
{
	static const int socket_type[4] =       /* the 4 socket types being tested in an array */
	{ SOCK_STREAM, SOCK_SEQPACKET, SOCK_RDM, SOCK_DGRAM };

	static const int MIN_SOCK_INDEX = 0;      /* used to skip stream if required for older versions */
	static const int MAX_CONN_SOCK_INDEX = 1; /* last connection orientated socket type */
	static const int MAX_SOCK_INDEX = 3;      /* last socket type in array */

	int so[4];                               /* socket to use for send and receive */
	int sol;                                 /* socket to listen on */
	struct sockaddr_tipc saddr;              /* address structure for socket */
	char buf[2048];                          /* buffer for message */
	char failString[100];                     /* string for the failure return code */

	int ii;                                  /* loop index for the sockets */
	int jj;                                  /* loop index for the option value */
	int n_opt;                               /* value for option */

	info("starting common_test_socketOptions\n");
	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {
		so[ii] = createSocketTIPC (socket_type[ii]);
	}

	setServerAddr(&saddr);


	for (ii = 0 ; ii < MIN_SOCK_INDEX ; ii++) {
		printf ("Bypassing all tests for %s\n" ,typeString (ii ,buf));
	}
	dbg1 ("Testing default values... \n");

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {

		getOption (so[ii] ,TIPC_IMPORTANCE,&n_opt);

		if (n_opt != TIPC_LOW_IMPORTANCE) {
			sprintf(failString,"FAILED : wrong default TIPC_IMPORTANCE "
			        "for socket type %s (%d)"
			        ,typeString (ii, buf) ,n_opt);
			err(failString);
		}
	}
	info("TIPC_IMPORTANCE defaults ... PASSED\n");

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {
		getOption (so[ii], TIPC_SRC_DROPPABLE, &n_opt);
		if ((int)n_opt != (ii < 3 ? 0 : 1)) {
			sprintf (failString,"FAILED : wrong default SRC_DROPPABLE "
			         "for socket type %s (%d)"
			         ,typeString (ii ,buf) ,n_opt);
			err(failString);
		}
	}
	info("TIPC_SRC_DROPPABLE defaults ... PASSED\n");

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {

		getOption (so[ii], TIPC_DEST_DROPPABLE,&n_opt);
		if ((int)n_opt != (ii < 2 ? 0 : 1)) {
			sprintf (failString,"FAILED : wrong default DEST_DROPPABLE "
			         "for socket type %s (%d)"
			         ,typeString (ii, buf) ,n_opt);
			err(failString);
		}
	}
	info("TIPC_DEST_DROPPABLE defaults ... PASSED\n");

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {

		getOption (so[ii], TIPC_CONN_TIMEOUT ,&n_opt);

		if (n_opt != 8000) {
			sprintf (failString,"FAILED : wrong default TIPC_CONN_TIMEOUT "
			         "for socket type %s (%d)"
			         ,typeString (ii, buf) ,n_opt);
			err(failString);
		}
	}
	info("TIPC_CONN_TIMEOUT defaults ... PASSED\n");

	/* only do the next test for the connection orientated sockets */

	dbg1 ("Testing setting of values then fetching them back... \n");

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {
		for (jj = TIPC_LOW_IMPORTANCE ; jj < TIPC_CRITICAL_IMPORTANCE ; jj++) {

			setOption (so[ii] ,TIPC_IMPORTANCE, jj);
			getOption (so[ii] ,TIPC_IMPORTANCE, &n_opt);
			if (n_opt != jj) {
				sprintf (failString,"FAILED : wrong TIPC_IMPORTANCE "
				         "for socket type %s (expected %d, got %d)"
				         ,typeString (ii,buf) ,jj ,n_opt);
				err(failString);
			}
		}
	}
	info("TIPC_IMPORTANCE R/W ... PASSED\n");

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {
		if (socket_type[ii] != SOCK_STREAM) { /* not supported for stream */
			for (jj = 0 ; jj < 2 ; jj++) {

				setOption (so[ii] ,TIPC_SRC_DROPPABLE, jj);
				getOption (so[ii] ,TIPC_SRC_DROPPABLE,&n_opt);
				if (n_opt != jj) {
					sprintf (failString,"FAILED : wrong TIPC_SRC_DROPPABLE "
					         "for socket type %s (expected %d, got %d)"
					         ,typeString (ii,buf) ,jj ,n_opt);
					err(failString);
				}
			}
		}
	}
	info("TIPC_SRC_DROPPABLE R/W ... PASSED\n");

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {
		for (jj = 0 ; jj < 2 ; jj++) {

			setOption (so[ii] ,TIPC_DEST_DROPPABLE, jj);
			getOption (so[ii] ,TIPC_DEST_DROPPABLE, &n_opt);
			if (n_opt != jj) {
				sprintf (failString,"FAILED : wrong TIPC_DEST_DROPPABLE "
				         "for socket type %s (expected %d, got %d)"
				         ,typeString (ii,buf) ,jj ,n_opt);
				err(failString);
			}
		}
	}
	info("TIPC_DEST_DROPPABLE R/W ... PASSED\n");

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {
		n_opt = 200; /* set to 200 milliseconds  default is 8 seconds*/
		setOption (so[ii] ,TIPC_CONN_TIMEOUT, n_opt);
		getOption (so[ii] ,TIPC_CONN_TIMEOUT, &n_opt);
		if (n_opt != 200) {
			sprintf (failString,"FAILED : wrong TIPC_CONN_TIMEOUT "
			         "for socket type %s (expected %d, got %d)"
			         ,typeString (ii,buf) ,200 ,n_opt);
			err(failString);
		}
	}
	info("TIPC_CONN_TIMEOUT R/W ... PASSED\n");

	n_opt = 300;  /* wait 300 milliseconds  default is 8000 milliseconds*/

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_CONN_SOCK_INDEX ; ii++) {
		sol = createSocketTIPC(socket_type[ii]);

		bindSocketTIPC (sol, &saddr);

		setOption(so[ii] ,TIPC_CONN_TIMEOUT, n_opt);
		/* connect on a socket that has no listener and it should timeout */

		if (0 == connect (so[ii] ,(struct sockaddr *)&saddr, sizeof (saddr))) {
			sprintf(failString, "FAILED : connect didn't return error"
			        " on %s" ,typeString (ii ,buf));
			err(failString);
		} else {
			if (ETIMEDOUT != errno) {
				err("FAILED : didn't time out");
			}
		}

		closeSocketTIPC(sol);
	}
	info("TIPC_CONN_TIMEOUT timeout ... PASSED\n");

	for (ii = MIN_SOCK_INDEX ; ii <= MAX_SOCK_INDEX ; ii++) {

		closeSocketTIPC (so[ii]);

	}

}

#define THE_STRING	"recvfrom"

#define MAX_STR		10

/**
 * common_test_recvfrom - test the recvfrom code on both the client and server
 */
void common_test_recvfrom
(
        int syncId					/* id to use for the client server sync */
)
{

	char buffer [MAX_STR];        /* buffer for message to be received in */
	char failStr [50 + MAX_STR];  /* string for failure return code */
	struct sockaddr_tipc addr;    /* address of socket */
	int so;                       /* socket to use */
	int size;                     /* size of the received message */



	setServerAddr (&addr);
	so = createSocketTIPC (SOCK_RDM);
	bindSocketTIPC(so, &addr);

	sendSyncTIPC (syncId);		/* now tell the Client to go ahead */

	size = recvfrom (so, buffer, MAX_STR, 0, NULL, 0);

	if ((strlen(THE_STRING)+1) != size) {

		sprintf (failStr,"common_test_recvfrom(): error receiving, size = %d\n", size);
		err(failStr);

	} else if (strcmp((const char *)THE_STRING, (const char *)&buffer) != 0) {
		sprintf (failStr,"common_test_recvfrom(): string mismatch got %s, wanted %s\n", buffer, THE_STRING);
		err(failStr);
	}
	closeSocketTIPC (so);
}



/**
 * common_test_sendto - test the recvfrom code on both the client and server
 */
void common_test_sendto
(
        int syncId					/* id to use for the client server sync */
)
{

	char failStr [50 + MAX_STR];  /* string for failure return code */
	struct sockaddr_tipc addr;    /* address of socket */
	int so;			      /* socket to use */
	int size;		      /* size of the sent string */

	setServerAddr (&addr);
	so = createSocketTIPC (SOCK_RDM);

	recvSyncTIPC(syncId);			/* wait for the server to be ready */

	size = sendto (so, THE_STRING, strlen(THE_STRING)+1, 0,

	               (struct sockaddr *)&addr, sizeof (addr));

	if ((strlen (THE_STRING) + 1) != size) {
		sprintf (failStr,"common_test_sendto(): error sending, size = %d\n", size);
		err(failStr);
	} else {
		closeSocketTIPC (so);
	}

}

unsigned int get_portid(int sd)
{
        struct sockaddr_tipc addr;
        socklen_t sz = sizeof(addr);

        if (getsockname(sd, (struct sockaddr *)&addr, &sz) < 0)
                err("Failed to get sock address\n");
        return addr.addr.id.ref;
}

