/*********************************************************
* Module Name: perfClient source
*
* File Name:    perfClient.c
*
* Summary:
*  This file contains the client portion of a client/server
*         UDP-based performance tool.
*
* Revisions:
*
* Author(s):  
*       Steven Hearndon (shearnd@g.clemson.edu) : initial code
*         Fall 2016  for CPSC 8510 (School of Computing, Clemson)
*
*********************************************************/
#include "perfTool.h"

int bStop = 0;
unsigned int debugFlag = 1;

void CatchAlarm(int ignored) {}
void ClientStop() { bStop = 1; }

unsigned int receivedCount = 0;
long totalPing = 0;
long minPing = LONG_MAX;
long maxPing = 0;
long avgPing = 0;

uint16_t mode = 0;
uint16_t messageSize = 1472;
unsigned int messageCount = 0;
struct timeval sessionStart, sessionEnd;

typedef struct serverResponse {
  unsigned int responseNum;
  long messagePing;
  struct serverResponse *next;
} serverResponse;

serverResponse *firstResponse = NULL;
serverResponse *lastResponse = NULL;

unsigned int numberResponses = 0;

serverResponse *addResponse(unsigned int num, long ping) {
  serverResponse *resp = malloc(1 * sizeof(serverResponse));
  resp->responseNum = num;
  resp->messagePing = ping;
  resp->next = NULL;
  if (firstResponse == NULL) {
    firstResponse = lastResponse = resp;
  } else {
    lastResponse->next = resp;
    lastResponse = resp;
  }
  numberResponses++;
  return resp;
}

void freeResponses() {
  serverResponse *tofree;
  while (firstResponse != NULL) {
    tofree = firstResponse;
    firstResponse = firstResponse->next;
    free(tofree);
    numberResponses--;
  }
}

double getPingStdDev() 
{
double avgDiff = 0.0;
double stddev = 0.0;

  serverResponse *toeval = firstResponse;
  long meanDiff;
  long sumSqrdDiffs = 0;
  while (toeval != NULL) 
  {
    if (debugFlag > 1)
      printf("%ld,", toeval->messagePing);

    meanDiff = toeval->messagePing - avgPing;
    sumSqrdDiffs += (meanDiff * meanDiff);
    toeval = toeval->next;
  }
  if (receivedCount > 0) 
  {
    avgDiff = sumSqrdDiffs / (float)receivedCount;
    stddev = sqrt(avgDiff) / 1000000.0;
    if (debugFlag > 1) {
      printf("perfClient: rxcount:%d, sumSqrdDiffs:%ld, avgDiff:%f, stddev:%f \n", 
          receivedCount,sumSqrdDiffs,avgDiff,stddev);
    }
  } 
  return stddev;
}

void setUnblockOption(int sock, char unblock) {
  int opts = fcntl(sock, F_GETFL);
  if (unblock == 1)
    opts |= O_NONBLOCK;
  else
    opts &= ~O_NONBLOCK;
  fcntl(sock, F_SETFL, opts);
}

void sockBlockingOn(int sock) { setUnblockOption(sock, 0); }
void sockBlockingOff(int sock) { setUnblockOption(sock, 1); }

char Version[] = "0.95";

int main(int argc, char *argv[]) 
{
  int sock;                         /* Socket descriptor */
  struct sockaddr_in serverAddress; /* Echo server address */
  struct sockaddr_in clientAddress; /* Source address of echo */
  unsigned short serverPort;        /* Echo server port */
  unsigned int clientAddressSize;   /* In-out of address size for recvfrom() */
  char *serverIP;                   /* IP address of server */
  char responseBuffer[MESSAGEMAX + 1]; /* Buffer for receiving echoed string */
  struct hostent *thehost;             /* Hostent from gethostbyname() */
  struct timeval currentTime, lastTokenTime;
  struct timeval sentTime, receivedTime, lastSentTime;
  struct sigaction myaction;
  long messagePing = 0;
  unsigned int seqNumber = 1;
  unsigned int RxSeqNumber = 1;
  unsigned int averageRate = 1000000;
  unsigned int tokenSize = 5888;
  unsigned int bucketSize = 5888;
  unsigned int numberIterations = 0;
  int bucket = 0;
  long tokenAddInterval;

  unsigned int messagesWaiting = 0;
  int numberMsgsInFlight  = 0;
  unsigned int largestSeqSent = 0;
  unsigned int largestSeqRecv = 0;
  int rc  = 0;

  if (argc < 3) /* Test for correct number of arguments */
  {
    fprintf(stderr, "perfClient(v%s): <Server IP> <Server Port> [<Average Rate>] "
                    "[<Bucket Size>] [<Token Size>] [<Message Size>] "
                    "[<Mode (RTT = 0, One-way = 1)>] [<# of iterations>] "
                    "[<Debug Flag>]\n",
            Version);
    exit(1);
  }

  signal(SIGINT, ClientStop);

  serverIP = argv[1]; /* First arg: server IP address (dotted quad) */
  serverPort = atoi(argv[2]);

  // get info from parameters if specified:
  if (argc >= 4)
    averageRate = atoi(argv[3]);

  if (argc >= 5)
    bucketSize = atoi(argv[4]);

  if (argc >= 6)
    tokenSize = atoi(argv[5]);

  if (argc >= 7) {
    messageSize = atoi(argv[6]);
    if (messageSize < MESSAGEMIN || messageSize > MESSAGEMAX) {
      printf("perfClient: HARD ERROR Message Size must be between %d and %d bytes\n", MESSAGEMIN, MESSAGEMAX);
      exit(1);
    }
  }

  if (argc >= 8)
    mode = atoi(argv[7]);

  if (argc >= 9)
    numberIterations = atoi(argv[8]);

  if (numberIterations == 0)
    numberIterations = UINT_MAX;

  if (argc >= 10)
    debugFlag = atoi(argv[9]);

  if (debugFlag > 0)
    printf("perfClient: averageRate:%d, bucketSize:%d, tokenSize:%d, messageSize:%d, mode:%d, numberIterations:%d \n",
       averageRate, bucketSize, tokenSize, messageSize, mode, numberIterations);

  myaction.sa_handler = CatchAlarm;
  if (sigfillset(&myaction.sa_mask) < 0)
    DieWithError("sigfillset() failed");

  myaction.sa_flags = 0;

  if (sigaction(SIGALRM, &myaction, 0) < 0)
    DieWithError("sigaction failed for sigalarm");

  /* Set up the message string */

  char message[messageSize];
  memset(&message, 0, messageSize);

  messageHeader *header = (messageHeader *)message;
  header->size = htons(messageSize);
  header->mode = htons(mode);
  messageHeader *rcvdHeader;

  // Calculate the token add interval in microseconds:
  float tokensPerSecond = (averageRate / 8.0) / tokenSize;
  tokenAddInterval = 1000000 / tokensPerSecond;
  if (debugFlag > 0)
    printf("perfClient: r:%f, Interval:%ld sizeof(long):%d sizeof(int):%d sizeof(short):%d \n", 
      tokensPerSecond, tokenAddInterval,sizeof(long),sizeof(unsigned int), sizeof(unsigned short));

  /* Construct the server address structure */
  memset(&serverAddress, 0, sizeof(serverAddress)); /* Zero out structure */
  serverAddress.sin_family = AF_INET;               /* Internet addr family */
  serverAddress.sin_addr.s_addr = inet_addr(serverIP); /* Server IP address */

  /* If user gave a dotted decimal address, we need to resolve it  */
  if (serverAddress.sin_addr.s_addr == -1) {
    thehost = gethostbyname(serverIP);
    serverAddress.sin_addr.s_addr = *((unsigned long *)thehost->h_addr_list[0]);
  }

  serverAddress.sin_port = htons(serverPort); /* Server port */

  /* Create a datagram/UDP socket */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
    DieWithError("socket() failed");

  gettimeofday(&sessionStart, NULL);
  lastTokenTime = sessionStart;
  lastSentTime = sessionStart;
  numberResponses=0;

  // sockBlockingOff(sock);
  // Loops for desired # of iterations or if an error occurs
  // messageCount tracks # of sends.
  // messagesWaiting tracks # of messages sent but not acked
  while (bStop != 1) {
    // Add token if tokenAddInterval has passed...
    gettimeofday(&currentTime, NULL);
    if (getTimeSpan(&lastTokenTime, &currentTime) >= tokenAddInterval) 
    {
      // ...and bucket still has capacity
      if (bucket + tokenSize <= bucketSize)
        bucket += tokenSize;
      else
        bucket = bucketSize;

      lastTokenTime = currentTime;
    }

    // loop and send up to a bucket of data....
  //  if (bucket >= messageSize)
    if (bucket >= messageSize && messageCount < numberIterations) 
    {
      header->timestamp = time(NULL);

      if (debugFlag > 1)
        printf("perfClient: timestamp = %ld\n", header->timestamp);

      header->sequenceNum = htonl(seqNumber);
      gettimeofday(&lastSentTime, NULL);
      // header->microseconds = getMicroseconds(&currentTime);
      header->timeSent = lastSentTime;

      largestSeqSent = seqNumber;
      numberMsgsInFlight = largestSeqSent - largestSeqRecv;

      //
      // Allow blocking on send
      sockBlockingOn(sock);
      rc =sendto(sock, &message, messageSize, 0, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
      if (rc == messageSize) 
      {
        gettimeofday(&sessionEnd, NULL);
        bucket -= messageSize;
        if (debugFlag > 1) {
          printf("perfClient: Sent seqNumber:%d, messageSize:%d, updatedBucket:%d  \n", seqNumber,messageSize,bucket);
        }

        if (bucket < 0) 
          printf("perfClient: HARD ERROR Sent seqNumber:%d, messageSize:%d, updatedBucket:%d  \n", seqNumber,messageSize,bucket);

        messageCount++;
        messagesWaiting++;
        seqNumber++;

      } else {
        printf("perfClient: HARD ERROR Failure on sendTo, client: %s, errno:%d\n",
               inet_ntoa(clientAddress.sin_addr), errno);
        if (errno == EMSGSIZE)
          printf("perfClient: HARD ERROR  Message Size of %d is too big\n", messageSize);
        exit(1);
      }
    }

    // Receive a response if in mode 0 (RTT)
//TODO:   numberMsgsInFlight  > 0
    if (mode == 0 && messagesWaiting > 0) {
      clientAddressSize = sizeof(clientAddress);

      // Set to non-blocking
      sockBlockingOff(sock);
      rc = recvfrom(sock, responseBuffer, MESSAGEMAX, 0, (struct sockaddr *)&clientAddress, &clientAddressSize);
      if (rc >=0 ) 
      {
        if (rc != messageSize)
          printf("perfClient: WARNING partial receive (%d bytes, should have been %d bytes) \n", rc,messageSize);


        gettimeofday(&receivedTime, NULL);
        rcvdHeader = (messageHeader *)responseBuffer;
        RxSeqNumber = ntohl(rcvdHeader->sequenceNum);
        if (largestSeqRecv < RxSeqNumber)
          largestSeqRecv = RxSeqNumber;
        numberMsgsInFlight = largestSeqSent - largestSeqRecv;

        sentTime = rcvdHeader->timeSent;
        messagePing = getTimeSpan(&sentTime, &receivedTime);
        messagesWaiting--;

        if (debugFlag > 1)
          printf("perfClient: (%d): RTT sample: %ld microseconds,  add to the response list (%d), waiting:%d \n", 
             RxSeqNumber, messagePing,numberResponses,messagesWaiting);


        addResponse(RxSeqNumber, messagePing);

        // Update stats
        receivedCount += 1;
        totalPing += messagePing;
        if (messagePing < minPing)
          minPing = messagePing;
        if (messagePing > maxPing)
          maxPing = messagePing;
        avgPing = totalPing / receivedCount;
      } else {
        //return of EAGAIN since nonblocking and no data
        if (errno != EAGAIN) {
          printf("perfClient: HARD ERROR Failure on recvFrom  errno:%d\n", errno);
          exit(1);
        }
      }
    }

    //TODO: messagesWaiting but numberMsgsInFlight 
#if 0
    gettimeofday(&currentTime, NULL);
    long tmpDelay =  getTimeSpan(&lastSentTime, &currentTime);
    if ( (messageCount >= numberIterations) &&
         ( (messagesWaiting <= 0) || (tmpDelay  > 1000000)) ) {
      bStop = 1;
      if (debugFlag > 0)
        printf("perfClient: ENDING while loop: last RxSeq:%d  tmpDelay:%ld messagesWaiting:%d numberOutstanding:%d \n", 
             RxSeqNumber, tmpDelay,messagesWaiting, numberMsgsInFlight);
    }
#endif
    gettimeofday(&currentTime, NULL);
    if (messageCount >= numberIterations &&
        (messagesWaiting <= 0 ||
         getTimeSpan(&lastSentTime, &currentTime) > 1000000))
      bStop = 1;
  }

  // Send final message:
  header->sequenceNum = 0xffffffff;
  sendto(sock, &message, messageSize, 0, (struct sockaddr *)&serverAddress,
         sizeof(serverAddress));

  close(sock);

  long bytesSent = messageCount * messageSize;
  float totalSeconds = getTimeSpan(&sessionStart, &sessionEnd) / 1000000.0;
  unsigned int sendingRate = 0;
  if (totalSeconds > 0)
    sendingRate = (bytesSent * 8) / totalSeconds;

  float lossPercent = 0.0;
  //messagesWaiting is the number not ack'ed
  //# outstanding: largestSeqSent - largestSeqRecv
  numberMsgsInFlight = largestSeqSent - largestSeqRecv;
  messagesWaiting -=numberMsgsInFlight;

  if (messageCount > 0) 
    lossPercent = messagesWaiting / (float)messageCount;

  if (debugFlag > 0)
    printf("perfClient: sent:%d, rx:%d, waiting::%d (after being adjusted by #inflight:%d) sendingRate:%d, numberResponses:%d \n", 
         messageCount,receivedCount,messagesWaiting,numberMsgsInFlight, sendingRate,numberResponses);

  if (mode == 0) {
    float pMin = minPing / 1000000.0;
    float pMax = maxPing / 1000000.0;
    float pAvg = avgPing / 1000000.0;
    double pSD = getPingStdDev();
    printf("#sent min max avg  std  lossPercent  sendingRate\n");
    printf("%d  %f %f %f %f %f %d\n", messageCount, pMin, pMax, pAvg, pSD, lossPercent,sendingRate);
  }
  freeResponses();
  exit(0);
}
