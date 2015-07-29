/*
 * msend.c  -- Sends UDP packets to a multicast group
 * 
 * (c)  Jianping Wang, Yvan Pointurier, Jorg Liebeherr, 2002
 *      Multimedia Networks Group, University of Virginia
 *
 * SOURCE CODE RELEASED TO THE PUBLIC DOMAIN
 * 
 * Based on this public domain program:
 * u_mctest.c            (c) Bob Quinn           2/4/97
 * 
 */
#include <stdio.h>
#ifdef WIN32
#include <stdlib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h> 
#include <sys/time.h>
#endif


#define TRUE 1
#define FALSE 0
#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif
#define LOOPMAX   20
#define BUFSIZE   1024

char *TEST_ADDR;
int TEST_PORT;
int TTL_VALUE;
int SLEEP_TIME=1000; 
unsigned long IP; 
int NUM=0; 

#ifndef WIN32
typedef struct timerhandler_s {
	int 	s; 
	char    *achOut; 
	int     len; 
	int 	n; 
	struct sockaddr *stTo; 
	int 	addr_size; 
} timerhandler_t; 
timerhandler_t handler_par; 
void timerhandler(); 
#endif

void printHelp(void) {
      printf("msend version 2.0\nUsage: msend -g group -p port [-i IP] [-t ttl] [-P period] [-text \"text\"|-n] \n\
        -g group    specifies the IP multicast address of the group\n\
                    to which packets are sent.\n\
        -p port     specifies the port number on which packets are sent.\n\
        -i ip       specifies the IP address of the interface to use \n\
                    to send the packets.\n\
        -t ttl      Sets the TTL of the packets.\n\
        -P period   Send a packet every \"period\" milliseconds.\n\
        -text       Puts \"text\" in the payload of the message.\n\
        -n          Encodes an increasing counter in the payload of\n\
                    the packets.\n");
}

int main( int argc, char *argv[])
{
  struct sockaddr_in stLocal, stTo;
  char achOut[BUFSIZE];
  int s, i;
  struct ip_mreq stMreq;
  int iTmp, iRet;
  int ii;
  int addr_size = sizeof(struct sockaddr_in);
#ifdef WIN32
  WSADATA stWSAData;
#else
  struct itimerval times;
  sigset_t sigset;
  struct sigaction act;
  siginfo_t si; 
#endif

  TTL_VALUE = 1;
  if( argc < 5 ) {
    printHelp(); 
    return 0;
  }
  strcpy(achOut, ""); 
  ii = 1;
  while (ii < argc) {
    if ( strcmp(argv[ii], "-g") == 0 ) {
        ii++;
        TEST_ADDR = argv[ii];
        ii++;
    }
    else if ( strcmp(argv[ii], "-p") == 0 ) {
        ii++;
        TEST_PORT = atoi(argv[ii]);
        ii++;
    }
    else if ( strcmp(argv[ii], "-i") == 0 ) {
        ii++;
        IP = inet_addr(argv[ii]); 
        ii++; 
    }
    else if ( strcmp(argv[ii], "-t") == 0 ) {
        ii++;
        TTL_VALUE = atoi(argv[ii]);
        ii++;
    }
    else if ( strcmp(argv[ii], "-P") == 0 ) {
        ii++;
        SLEEP_TIME = atoi(argv[ii]);
        ii++;
    }
    else if ( strcmp(argv[ii], "-n") == 0 ) {
        ii++;
        NUM=1;
        ii++;
    }
    else if ( strcmp(argv[ii], "-text") == 0 ) {
        ii++;
        strcpy(achOut, argv[ii]);
        ii++;
    }
    else {
        printHelp();
        return 1;
    }
  }

#ifdef WIN32
  /* Init WinSock */
  i = WSAStartup(0x0202, &stWSAData);
  if (i) {
      printf ("WSAStartup failed: %d\r\n", i);
      exit(1);
  }
#endif

  /* get a datagram socket */
  s = socket(AF_INET, 
     SOCK_DGRAM, 
     0);
  if (s == INVALID_SOCKET) {
    printf ("socket() failed.\n");
    exit(1);
  }

  /* avoid EADDRINUSE error on bind() */ 
  iTmp = TRUE;
  iRet = setsockopt(s, 
     SOL_SOCKET, 
     SO_REUSEADDR, 
     (char *)&iTmp, 
     sizeof(iTmp));
  if (iRet == SOCKET_ERROR) {
    printf ("setsockopt() SO_REUSEADDR failed.\n");
  }

  /* name the socket */
  stLocal.sin_family = 	 AF_INET;
  stLocal.sin_addr.s_addr = IP;
  stLocal.sin_port = 	 htons(TEST_PORT);
  iRet = bind(s, (struct sockaddr*) &stLocal, sizeof(stLocal));
  if (iRet == SOCKET_ERROR) {
    printf ("bind() failed.\n");
  }

  /* join the multicast group. */
  stMreq.imr_multiaddr.s_addr = inet_addr(TEST_ADDR);
  stMreq.imr_interface.s_addr = INADDR_ANY;
  iRet = setsockopt(s, 
     IPPROTO_IP, 
     IP_ADD_MEMBERSHIP, 
     (char *)&stMreq, 
     sizeof(stMreq));
  if (iRet == SOCKET_ERROR) {
    printf ("setsockopt() IP_ADD_MEMBERSHIP failed.\n");
  } 

  /* set TTL to traverse up to multiple routers */
  iTmp = TTL_VALUE;
  iRet = setsockopt(s, 
     IPPROTO_IP, 
     IP_MULTICAST_TTL, 
     (char *)&iTmp, 
     sizeof(iTmp));
  if (iRet == SOCKET_ERROR) {
    printf ("setsockopt() IP_MULTICAST_TTL failed.\n");
  }

  /* enable loopback */
  iTmp = TRUE;
  iRet = setsockopt(s, 
     IPPROTO_IP, 
     IP_MULTICAST_LOOP, 
     (char *)&iTmp, 
     sizeof(iTmp));
  if (iRet == SOCKET_ERROR) {
    printf ("setsockopt() IP_MULTICAST_LOOP failed.\n");
  }

  /* assign our destination address */
  stTo.sin_family =      AF_INET;
  stTo.sin_addr.s_addr = inet_addr(TEST_ADDR);
  stTo.sin_port =        htons(TEST_PORT);
  printf ("Now sending to multicast group: %s\n",
    TEST_ADDR);

#ifdef WIN32
  for (i=0;;i++) {
    static iCounter = 1;

    /* send to the multicast address */
    if (NUM) {
      achOut[3] = (unsigned char)(iCounter >>24); 
      achOut[2] = (unsigned char)(iCounter >>16); 
      achOut[1] = (unsigned char)(iCounter >>8); 
      achOut[0] = (unsigned char)(iCounter ); 
      printf("Send out msg %d to %s:% d\n", iCounter, TEST_ADDR, TEST_PORT);
    } else {
      printf("Send out msg %d to %s:%d: \"%s\"\n", iCounter, TEST_ADDR, TEST_PORT, achOut);
    }

    iRet = sendto(s, 
      achOut, 
      (NUM?4:strlen(achOut)+1), 
      0,
      (struct sockaddr*)&stTo, 
      addr_size);
    if (iRet < 0) {
      printf ("sendto() failed.");
      exit(1);
    }

    iCounter++;

    Sleep(SLEEP_TIME);
    
  } /* end for(;;) */
#else
	SLEEP_TIME*=1000; /* convert to microsecond */
	if (SLEEP_TIME>0) {
		/* block SIGALRM */
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGALRM);
		sigprocmask(SIG_BLOCK, &sigset, NULL);
	
		/* set up handler for SIGALRM */
		act.sa_handler = &timerhandler;
		sigemptyset(&act.sa_mask);
		act.sa_flags = SA_SIGINFO;
		sigaction(SIGALRM, &act, NULL);
		/*
	 	 * set up interval timer
 		 */
		times.it_value.tv_sec     = 0; /* wait a bit for system to "stabilize"  */
		times.it_value.tv_usec    = 1; /* tv_sec or tv_usec cannot be both zero */
		times.it_interval.tv_sec  = (time_t)(SLEEP_TIME/1000000); 
		times.it_interval.tv_usec = (long)  (SLEEP_TIME%1000000); 
		setitimer(ITIMER_REAL, &times, NULL); 
	
		handler_par.s 		= s;  
		handler_par.achOut 	= achOut; 
		handler_par.len 	= strlen(achOut)+1; 
		handler_par.n 		= 0; 
		handler_par.stTo 	= (struct sockaddr*)&stTo; 
		handler_par.addr_size	= addr_size; 
	
		/* now wait for the alarms */
		sigemptyset(&sigset);
		for(;;) {
			sigsuspend(&sigset);
		}
		return; 
	} else {
		for (i=0;i<10;i++) {
    			int addr_size = sizeof(struct sockaddr_in);
			
			if (NUM) {
				achOut[3] = (unsigned char)(i>>24); 
				achOut[2] = (unsigned char)(i>>16); 
				achOut[1] = (unsigned char)(i>>8); 
				achOut[0] = (unsigned char)(i); 
				printf("Send out msg %d to %s:%d\n", i, TEST_ADDR, TEST_PORT);
			} else {
				printf("Send out msg %d to %s:%d: %s\n", i, TEST_ADDR, TEST_PORT, achOut);
			}
			
    			iRet = sendto(s, 
      				achOut, 
      				(NUM?4:strlen(achOut)+1), 
      				0,
      				(struct sockaddr*)&stTo, 
      				addr_size);
    			if (iRet < 0) {
      			printf ("sendto() failed.\n");
      			exit(1);
    			}
    		} /* end for(;;) */
  	} 
#endif
  return 0; 
} /* end main() */  

#ifndef WIN32
void timerhandler(int sig, siginfo_t *siginfo, void  *context) {
    int iRet; 
    static long iCounter = 1;

    if (NUM) {
      handler_par.achOut = (char *)(&iCounter); 
      handler_par.len    = sizeof(iCounter); 
      printf("Send out msg %d to %s:%d\n", iCounter, TEST_ADDR, TEST_PORT);
    } else {
      printf("Send out msg %d to %s:%d: %s\n", iCounter, TEST_ADDR, TEST_PORT, handler_par.achOut);
    }
    iRet = sendto( handler_par.s, 
      handler_par.achOut, 
      handler_par.len, 
      handler_par.n, 
      handler_par.stTo, 
      handler_par.addr_size);
    if (iRet < 0) {
      printf ("sendto() failed.\n");
      exit(1);
    }
    iCounter++;
    return; 
}
#endif
