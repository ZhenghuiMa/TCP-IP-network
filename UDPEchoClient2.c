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

void clientCNTCCode();
void CatchAlarm(int ignored);
int numberOfTimeOuts=0;
int numberOfTrials;
long totalPing;
long totalNumber;
long minRTT;
long maxRTT;
long totalRTT;
unsigned int size;
int bStop;
short mode;


struct timeval *theTime3;
struct timeval *theTime4;
struct timeval TV3, TV4;

struct Datagram
{
  short mode;
  short messageSize;
  unsigned int seqNum;
  char rem[1];
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
    double delay;                /* Iteration delay in seconds */
    int packetSize;                  /* PacketSize*/
    struct timeval *theTime1;
    struct timeval *theTime2;
    struct timeval TV1, TV2;
    int i;
    struct sigaction myaction;
    long usec1, usec2, curPing;
    int *seqNumberPtr;
    unsigned int seqNumber = 1;
    unsigned int RxSeqNumber = 1;
    struct timespec reqDelay, remDelay;
    int nIterations;
    long avgPing, loss;
    long timeRTT;

    struct Datagram datagram;
    struct token_buffer *tb;

    unsigned int averageRate;
    unsigned int bucketSize;
    unsigned int tokenSize;
    unsigned int messageSize;
    unsigned int container;
    short mode;
    short debugFlag;

    theTime3 = &TV3;
    theTime4 = &TV4;

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

    echoServPort = atoi(argv[2]);
    bucketSize = atoi(argv[4]);
    tokenSize = atoi(argv[5]);
    averageRate = atoi(argv[3]);
    container = atoi(argv[4]);
    messageSize = atoi(argv[6]);
    mode = atoi(argv[7]);
    nIterations = atoi(argv[8]);
    debugFlag = atoi(argv[9]);
    packetSize = 32;

    size = packetSize;
    /* get info from parameters , or default to defaults if they're not specified */
    /*
    if (argc == 2) {
       echoServPort = 7;
       delay = 1.0;
       packetSize = 32;
     nIterations = 1;
    }
    else if (argc == 3) {
       echoServPort = atoi(argv[2]);
       delay = 1.0;
       packetSize = 32;
     nIterations = 1;
    }
    else if (argc == 4) {
       echoServPort = atoi(argv[2]);
       delay = atof(argv[3]);
       packetSize = 32;
       nIterations = 1;
    }
    else if (argc == 5) {
       echoServPort = atoi(argv[2]);
       delay = atof(argv[3]);
       packetSize = atoi(argv[4]);
       if (packetSize > ECHOMAX)
         packetSize = ECHOMAX;
       nIterations = 1;
    }
    else if (argc == 6) {
      echoServPort = atoi(argv[2]);
      delay = atof(argv[3]);
      packetSize = atoi(argv[4]);
      if (packetSize > ECHOMAX)
        packetSize = ECHOMAX;
      nIterations = atoi(argv[5]);
    }
     */

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

    datagram.mode = mode;
    datagram.messageSize = messageSize;
    datagram.seqNum = 1;

    for (i=8; i<packetSize; i+=4) {
       datagram.rem[i] = 0x31;
       datagram.rem[i+1] = 0x32;
       datagram.rem[i+2] = 0x33;
       datagram.rem[i+3] = 0x34;
    }
    
    char * SendBuffer = (char *)malloc(sizeof(packetSize));
    memcpy(SendBuffer, &datagram, sizeof(packetSize));

    size = packetSize;

    gettimeofday(theTime3, NULL);

  while (nIterations > 0 && bStop != 1) {

    //*seqNumberPtr = htonl(seqNumber++); 

    /* Create a datagram/UDP socket */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");


    /* Send the string to the server */
    //printf("UDPEchoClient: Send the string: %s to the server: %s \n", echoString,servIP);
    gettimeofday(theTime1, NULL);

    datagram.seqNum = seqNumber++;

    //printf("Num %d \n", datagram.seqNum);

    if (sendto(sock, SendBuffer, packetSize, 0, (struct sockaddr *)
             &echoServAddr, sizeof(echoServAddr)) != echoStringLen)
    DieWithError("sendto() sent a different number of bytes than expected");

    bucketSize = bucketSize - tokenSize;  

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

    if(timeRTT>0)
    {
      minRTT = (usec2 - usec1) < timeRTT ? (usec2 - usec1) : timeRTT; 
    
    }else{

      minRTT = 0;
    }
    
    maxRTT = (usec2 - usec1) > timeRTT ? (usec2 - usec1) : timeRTT;

    timeRTT = (usec2 - usec1);
    totalRTT += timeRTT;
    /*
    printf("Ping(%d): %ld microseconds\n",RxSeqNumber,curPing);

    totalPing += curPing;
    */
    numberOfTrials++;
    totalNumber++;
    
    close(sock);
    sleep(delay);
    close(sock);

    reqDelay.tv_sec = delay;
    remDelay.tv_nsec = 0;
    nanosleep((const struct timespec*)&reqDelay, &remDelay);

    bucketSize = (bucketSize + (averageRate/8000000)*(usec2 - usec1)) > container ? container : (bucketSize + (averageRate/8000000)*(usec2 - usec1));

    nIterations--;
  }
  /*
  if (numberOfTrials != 0) 
    avgPing = (totalPing/numberOfTrials);
  else 
    avgPing = 0;
  if (numberOfTimeOuts != 0) 
    loss = ((numberOfTimeOuts*100)/numberOfTrials);
  else 
    loss = 0;

  printf("\nAvg Ping: %ld microseconds Loss: %ld Percent\n", avgPing, loss);
  */
  exit(0);
}

void CatchAlarm(int ignored) { }

void clientCNTCCode() {
  
  bStop = 1;

  gettimeofday(theTime4, NULL);

  long time1 = theTime3->tv_sec;
  long time2 = theTime4->tv_sec;
  long timeval = time2 - time1;

  long sentRate = (totalNumber * 8 * size)/timeval;

  if(mode == 0)
  {
    float min = minRTT/1000000;
    float max = maxRTT/1000000;
    float mean = totalRTT/(totalNumber*1000000);

    printf("\n %ld %ld %f %f %f\n", totalNumber, sentRate, min, max, mean);
  }else{
    printf("\n %ld %ld \n", totalNumber, sentRate);
  }
}