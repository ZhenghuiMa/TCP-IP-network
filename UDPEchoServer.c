/*********************************************************
*
* Module Name: UDP Echo server 
*
* File Name:    UDPEchoServer.c 
*
* Summary:
*  This file contains the echo server code
*
* Revisions:
*  $A0:  6/15/2008:  don't exit on error
*
*********************************************************/
#include "UDPEcho.h"

void DieWithError(char *errorMessage);  /* External error handling function */
void serverCNTCCode(); /* CNT-C */
double ranRate();

struct Datagram
{
  short mode;
  short messageSize;
  unsigned int seqNum;
  char rem[1];
};

struct Info
{
    unsigned short echoServPort;

};

unsigned receivedNum = 0;
unsigned int messageSeq;
char *ip;
int port;
unsigned int messageSize;

struct timeval *theTime3;
struct timeval *theTime4;
struct timeval TV3, TV4;

char Version[] = "1.1";

int main(int argc, char *argv[])
{
    int sock;                        /* Socket */
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
    char echoBuffer[ECHOMAX];        /* Buffer for echo string */
    unsigned short echoServPort;     /* Server port */
    int recvMsgSize;                 /* Size of received message */
    double lossRate;
    short debugFlag;

    //int receivedNum;
    int num; 
    struct timeval wait_time;
    struct Datagram datagram;
    fd_set read_fds;

    theTime3 = &TV3;
    theTime4 = &TV4; 

    if (argc != 4)         /* Test for correct number of parameters */
    {
        fprintf(stderr,"Usage:  %s <UDP SERVER PORT> <Loss Rate> <Debug Flag>\n", argv[0]);
        exit(1);
    }

    signal (SIGINT, serverCNTCCode);

    port = atoi(argv[1]);

    if (argc == 2) {
       echoServPort = atoi(argv[1]);
       lossRate = 0;
       debugFlag = 0; 
    }
    else if (argc == 3) {
        echoServPort = atoi(argv[1]);  /* First arg:  local port */
        lossRate = atof(argv[2]);
        debugFlag = 0;
    }else if (argc == 4){
        echoServPort = atoi(argv[1]);  /* First arg:  local port */
        lossRate = atof(argv[2]);
        debugFlag = atof(argv[3]);
    }

    messageSize = 32;

//$A0
    printf("UDPEchoServer(version:%s): Port:%d\n",(char *)Version,echoServPort);    

    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
//        DieWithError("socket() failed");
      printf("Failure on socket call , errno:%d\n",errno);
    }

    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0) {
//        DieWithError("bind() failed");
          printf("Failure on bind, errno:%d\n",errno);
    }   

    receivedNum = 0;
  
    for (;;) /* Run forever */
    {

        if(ranRate() < lossRate){
            wait_time.tv_sec = 1;  
            wait_time.tv_usec = 0;  
            FD_ZERO(&read_fds);  
            FD_SET(sock, &read_fds);  
      
            num = select(sock+1, &read_fds, NULL, NULL, &wait_time);

            if (num < 0)  
            {  
                perror("Select fail");  
                continue;  
            }  

            if (FD_ISSET(sock, &read_fds))  
            {


                /* Set the size of the in-out parameter */
                cliAddrLen = sizeof(echoClntAddr);

                /* Block until receive message from a client */
                if ((recvMsgSize = recvfrom(sock, echoBuffer, ECHOMAX, 0,
                    (struct sockaddr *) &echoClntAddr, &cliAddrLen)) < 0)
                {
        //$A0
        //            DieWithError("recvfrom() failed");
                  printf("Failure on recvfrom, client: %s, errno:%d\n", inet_ntoa(echoClntAddr.sin_addr),errno);
                }

                receivedNum++;

                if(receivedNum == 1){
                    gettimeofday(theTime3, NULL);
                }

                //printf("RecivedIP %s\n", inet_ntoa(echoClntAddr.sin_addr));

                memcpy(&datagram, echoBuffer, sizeof(datagram));

                printf("Size: %d\n", datagram.messageSize);

                //free(echoBuffer);

                ip = inet_ntoa(echoClntAddr.sin_addr);

                //printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

                /* Send received datagram back to the client */
                if (sendto(sock, echoBuffer, recvMsgSize, 0,  
                     (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) != recvMsgSize) {
        //            DieWithError("sendto() sent a different number of bytes than expected");
                  printf("Failure on sendTo, client: %s, errno:%d\n", inet_ntoa(echoClntAddr.sin_addr),errno);
                } 
            }

        }
        messageSeq++;
    }
    /* NOT REACHED */
}

double ranRate()
{
    return (double)rand() / (double)RAND_MAX ;
}

void serverCNTCCode() {
    //bStop = 1;
    int i;

    gettimeofday(theTime4, NULL);

    long time1 = theTime3->tv_sec;
    long time2 = theTime4->tv_sec;
    long timeval = time2 - time1;

    float lossRateOfMessages; 

    long totalBytes = receivedNum * messageSize;

    long avgThroughOutput = (totalBytes*8)/timeval;
    
    lossRateOfMessages = receivedNum/messageSeq; 

    printf("\n %s %d %ld %ld %.2f\n", ip, port, totalBytes, avgThroughOutput, lossRateOfMessages);

    fflush(stdout);
    
    exit(0);

}