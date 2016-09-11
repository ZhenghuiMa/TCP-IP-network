/*********************************************************
* Module Name: UDP Echo client source 
*
* File Name:    UDPEchoClient2.c
*
* Summary:
*  This file contains the echo Client code.
*
* Revisions:
*
* $A0: 6-15-2008: misc. improvements
*
*********************************************************/
#include "UDPEcho.h"
#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>

typedef struct token_buffer {
  size_t bucketSize;
  size_t tokenSize;
  double rate;
  uint64_t timestamp;

} token_buffer;

void clientCNTCCode();
void CatchAlarm(int ignored);
int numberOfTimeOuts=0;
int numberOfTrials;
long totalPing;
int bStop;


struct token_buffer {
  size_t bucketSize;
  size_t tokenSize;
  double rate;
  uint64_t timestamp;

};

struct Datagram
{
  short mode;
  short messageSize;
  unsigned int seqNum;
};

char Version[] = "1.1";   

int main(int argc, char *argv[])
{
    int sock;                        /* Socket descriptor */
    struct sockaddr_in echoServAddr; /* Echo server address */
    struct sockaddr_in fromAddr;     /* Source address of echo */
    unsigned short echoServPort;     /* Echo server port */
    unsigned int fromSize;           /* In-out of address size for recvfrom() */
    char *servIP;                    /* IP address of server */
    char *echoString;                /* String to send to echo server */
    char echoBuffer[ECHOMAX+1];      /* Buffer for receiving echoed string */
    int echoStringLen;               /* Length of string to echo */
    int respStringLen;               /* Length of received response */
    struct hostent *thehost;       /* Hostent from gethostbyname() */
    int delay, packetSize;  /* Iteration delay in seconds, packetSize*/
    struct timeval *theTime1;
    struct timeval *theTime2;
    struct timeval TV1, TV2;
    int i;
    struct sigaction myaction;
    long usec1, usec2, curPing;
    int *seqNumberPtr;
    unsigned int seqNumber = 1;
    unsigned int RxSeqNumber = 1;

    struct Datagram datagram;

    unsigned int averageRate;
    unsigned int bucketSize;
    unsigned int tokenSize;
    unsigned int messageSize;
    short mode;
    short debugFlag;

    theTime1 = &TV1;
    theTime2 = &TV2;

    //Initialize values
    numberOfTimeOuts = 0;
    numberOfTrials = 0;
    totalPing =0;
    bStop = 0;

    if(argc != 10)
    {
        fprintf(stderr,"Usage: %s <Server IP> [<Server Port>] [<Average Rate>] [<Bucket Size>] [<Token Size>] [Message Size] [Mode] [<No. of Iterations>] [Debug Flag]\n", argv[0]);
        exit(1);
    }

    signal (SIGINT, clientCNTCCode);

    servIP = argv[1];           /* First arg: server IP address (dotted quad) */

    /* get info from parameters , or default to defaults if they're not specified */
    if (argc == 2) {
       echoServPort = 7;
       delay = 1.0;

       averageRate = 1000000;
       messageSize = 1472;
       tokenSize = 4*messageSize;
       bucketSize = tokenSize;
       mode = 0;
       debugFlag = 0;

       packetSize = 32;
       nIterations = 1;
    }
    else if (argc == 3) {
       echoServPort = atoi(argv[2]);
       delay = 1.0;

       averageRate = atoi(argv[3]);
       messageSize = 1472;
       tokenSize = 4*messageSize;
       bucketSize = tokenSize;
       mode = 0;
       debugFlag = 0;

       packetSize = 32;
       nIterations = 1;
    }
    else if (argc == 4) {
       echoServPort = atoi(argv[2]);
       delay = 0.1;

       averageRate = atoi(argv[3]);
       messageSize = 1472;
       tokenSize = 4*messageSize;
       bucketSize = atoi(argv[4]);
       mode = 0;
       debugFlag = 0;

       packetSize = 32;
       nIterations = 1;
    }
    else if (argc == 5) {
       echoServPort = atoi(argv[2]);
       delay = 0.1;

       averageRate = atoi(argv[3]);
       messageSize = 1472;
       tokenSize = atoi(argv[5]);
       bucketSize = atoi(argv[4]);
       mode = 0;
       debugFlag = 0;

       packetSize = 32;
       if (packetSize > ECHOMAX)
         packetSize = ECHOMAX;
       nIterations = 1;
    }
    else if (argc == 6) {
      echoServPort = atoi(argv[2]);
      delay = 0.1;

       averageRate = atoi(argv[3]);
       messageSize = atoi(argv[6]);
       tokenSize = atoi(argv[5]);
       bucketSize = atoi(argv[4]);
       mode = 0;
       debugFlag = 0;

      packetSize = 32;
      if (packetSize > ECHOMAX)
        packetSize = ECHOMAX;
      nIterations = 1;
    }else if (argc == 7) {
      echoServPort = atoi(argv[2]);
      delay = 0.1;

       averageRate = atoi(argv[3]);
       messageSize = atoi(argv[6]);
       tokenSize = atoi(argv[5]);
       bucketSize = atoi(argv[4]);
       mode = atoi(argv[7]);
       debugFlag = 0;

      packetSize = 32;
      if (packetSize > ECHOMAX)
        packetSize = ECHOMAX;
      nIterations = 1;
    }else if (argc == 8) {
      echoServPort = atoi(argv[2]);
      delay = 0.1;

       averageRate = atoi(argv[3]);
       messageSize = atoi(argv[6]);
       tokenSize = atoi(argv[5]);
       bucketSize = atoi(argv[4]);
       mode = atoi(argv[7]);
       debugFlag = 0;

      packetSize = 32;
      if (packetSize > ECHOMAX)
        packetSize = ECHOMAX;
      nIterations = atoi(argv[8]);
    }else if (argc == 9) {
      echoServPort = atoi(argv[2]);
      delay = 0.1;

       averageRate = atoi(argv[3]);
       messageSize = atoi(argv[6]);
       tokenSize = atoi(argv[5]);
       bucketSize = atoi(argv[4]);
       mode = atoi(argv[7]);
       debugFlag = atoi(argv[9]);

      packetSize = 32;
      if (packetSize > ECHOMAX)
        packetSize = ECHOMAX;
      nIterations = atoi(argv[8]);
    }
       
    myaction.sa_handler = CatchAlarm;
    if (sigfillset(&myaction.sa_mask) < 0)
       DieWithError("sigfillset() failed");

    myaction.sa_flags = 0;

    if (sigaction(SIGALRM, &myaction, 0) < 0)
       DieWithError("sigaction failed for sigalarm");

    /* Set up the echo string */

    echoStringLen = packetSize;
    echoString = (char *) echoBuffer;

    for (i=0; i<packetSize; i++) {
       echoString[i] = 0;
    }

    seqNumberPtr = (int *)echoString;
    echoString[packetSize-1]='\0';


    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));    /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                 /* Internet addr family */
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);  /* Server IP address */
    
    /* If user gave a dotted decimal address, we need to resolve it  */
    if (echoServAddr.sin_addr.s_addr == -1) {
        thehost = gethostbyname(servIP);
      echoServAddr.sin_addr.s_addr = *((unsigned long *) thehost->h_addr_list[0]);
    }
    
    echoServAddr.sin_port   = htons(echoServPort);     /* Server port */
    
    datagram.mode = 0x01;
    datagram.messageSize = 0x01;
    datagram.seqNum = 1;
    
    char * SendBuffer = (char *)malloc(sizeof(struct Datagram)+1);
    memcpy(SendBuffer, &datagram, sizeof(datagram));

  while (1) {

    if (bStop == 1) exit(0);

    *seqNumberPtr = htonl(seqNumber++); 

    /* Create a datagram/UDP socket */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");


    /* Send the string to the server */
    //printf("UDPEchoClient: Send the string: %s to the server: %s \n", echoString,servIP);
    gettimeofday(theTime1, NULL);

    printf("Size check %d\n", sizeof(datagram));

    if (sendto(sock, SendBuffer, (int)strlen(SendBuffer), 0, (struct sockaddr *)
               &echoServAddr, sizeof(echoServAddr)) != echoStringLen)
      DieWithError("sendto() sent a different number of bytes than expected");
  
    printf("End\n");
    /* Recv a response */

    fromSize = sizeof(fromAddr);
    alarm(2);            //set the timeout for 2 seconds

    if ((respStringLen = recvfrom(sock, echoBuffer, ECHOMAX, 0,
         (struct sockaddr *) &fromAddr, &fromSize)) != echoStringLen) {
        if (errno == EINTR) 
        { 
           printf("Received a  Timeout !!!!!\n"); 
           numberOfTimeOuts++; 
           continue; 
        }
    }

    RxSeqNumber = ntohl(*(int *)echoBuffer);

    alarm(0);            //clear the timeout 
    gettimeofday(theTime2, NULL);

    usec2 = (theTime2->tv_sec) * 1000000 + (theTime2->tv_usec);
    usec1 = (theTime1->tv_sec) * 1000000 + (theTime1->tv_usec);

    curPing = (usec2 - usec1);
    printf("Ping(%d): %ld microseconds\n",RxSeqNumber,curPing);

    totalPing += curPing;
    numberOfTrials++;
    close(sock);
    sleep(delay);

  }
  exit(0);
}

void CatchAlarm(int ignored) { }

void clientCNTCCode() {
  long avgPing, loss;

  bStop = 1;
  if (numberOfTrials != 0) 
    avgPing = (totalPing/numberOfTrials);
  else 
    avgPing = 0;
  if (numberOfTimeOuts != 0) 
    loss = ((numberOfTimeOuts*100)/numberOfTrials);
  else 
    loss = 0;

  printf("\nAvg Ping: %ld microseconds Loss: %ld Percent\n", avgPing, loss);
}

uint64_t time_now()
{
  struct timeval ts;
  gettimeofday(&ts, NULL);
  return (uint64_t)(ts.tv_sec * 1000 + ts.tv_usec/1000);
}

int token_buffer_init(token_buffer *tbf, size_t max_burst, size_t token, double rate)
{
  tbf->bucketSize = max_burst;
  tbf->tokenSize   = token;
  tbf->rate = rate;
  tbf->timestamp = time_now();
}

int token_buffer_consume(token_buffer *tbf, int bytes)
{
  uint64_t now = time_now();
  size_t delta = (size_t)(tbf->rate * (now - tbf->timestamp));
  tbf->tokenSize = (tbf->bucketSize < tbf->tokenSize+delta)?tbf->bucketSize:tbf->tokenSize+delta;
  tbf->timestamp = now;

  fprintf(stdout, "TOKENS %d  bytes: %d\n", tbf->tokenSize, bytes);

  if(tbf->tokenSize > 0) {
    tbf->tokenSize -= bytes;
  } else {
    return -1;
  }

  return 0;
}