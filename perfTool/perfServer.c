/*********************************************************
* Module Name: perfServer source
*
* File Name:    perfServer.c
*
* Summary:
*  This file contains the Server portion of a client/server
*         UDP-based performance tool.
*
* Program parameters:
*   UDP Port : specifies the servers UDP port (0-64K)
*   Emulated Avg Loss Rate : If specified, represents the
*    target loss rate of a random uncorrelated loss generator.
*   debug Flag :  specifies if debug messages are sent to standard out.
*            Value of 0 :  No output except for HARD ERRORS (which cause an exit)
*            Value of 1 :  Minimal output (default setting)
*            Value of 2 :  Verbose output (handy when debugging)
*
* Revisions:
*
* Author(s):  
*       Steven Hearndon (shearnd@g.clemson.edu) : initial code
*         Fall 2016  for CPSC 8510 (School of Computing, Clemson)
*
*********************************************************/
#include "perfTool.h"

int sock;
int bStop = 0;
void ServerStop() {
  bStop = 1;
  close(sock);
}

typedef struct session {
  struct in_addr clientIP;
  unsigned short clientPort;
  struct timeval timeStarted;
  long duration;
  unsigned int bytesReceived;
  unsigned int messagesReceived;
  unsigned int messagesLost;
  unsigned int lastSequenceNum;
  struct session *prev;
  struct session *next;
} session;

session *firstSession = NULL;
session *lastSession = NULL;
session *firstActive = NULL;
unsigned int sessionCount = 0;

session *findActive(struct in_addr clientIP, unsigned short clientPort) {
  session *s = firstActive;
  while (s != NULL) {
    if (s->clientIP.s_addr == clientIP.s_addr && s->clientPort == clientPort)
      return s;
    s = s->next;
  }
  return s;
}

session *getActive(struct in_addr clientIP, unsigned short clientPort) {
  // Find active session
  session *s = findActive(clientIP, clientPort);
  // If matching session is not found...
  if (s == NULL) {
    // ...add new session
    s = malloc(1 * sizeof(session));
    s->clientIP = clientIP;
    s->clientPort = clientPort;
    gettimeofday(&(s->timeStarted), NULL);
    s->lastSequenceNum = 0;
    s->bytesReceived = 0;
    s->messagesReceived = 0;
    s->messagesLost = 0;
    s->duration = 0;
    s->next = firstActive;
    s->prev = NULL;
    firstActive = s;
    sessionCount++;
  }
  return s;
}

void removeActive(struct in_addr clientIP, unsigned short clientPort) {
  session *s = findActive(clientIP, clientPort);
  if (s != NULL) {
    if (s->next != NULL)
      s->next->prev = s->prev;
    if (s->prev != NULL)
      s->prev->next = s->next;
    if (s == firstActive)
      firstActive = (s->prev != NULL) ? s->prev : s->next;
    // Add to archive list:
    s->next = NULL;
    if (firstSession == NULL) {
      firstSession = s;
      lastSession = s;
      s->prev = NULL;
    } else {
      lastSession->next = s;
      lastSession = s;
    }
  }
}

void updateSessionDuration(session *s) {
  struct timeval sessionEnd;
  gettimeofday(&sessionEnd, NULL);
  s->duration = getTimeSpan(&(s->timeStarted), &sessionEnd);
}

void printSessions() {
  session *toprint = firstSession;
  while (toprint != NULL) {
    char *cAddr = inet_ntoa(toprint->clientIP);
    if (toprint->duration == 0)
      updateSessionDuration(toprint);
    float durSecs = toprint->duration / 1000000.0;
    long bps = (toprint->bytesReceived * 8) / durSecs;
    float lossPercent =
        toprint->messagesLost /
        (float)(toprint->messagesLost + toprint->messagesReceived);
    printf("%s %d %f %d %ld %f\n", cAddr, toprint->clientPort, durSecs,
           toprint->bytesReceived, bps, lossPercent);
    toprint = toprint->next;
  }
}

void freeAllSessions() {
  while (firstSession != NULL) {
    session *tofree = firstSession;
    firstSession = tofree->next;
    free(tofree);
  }
}

char Version[] = "0.95";

int main(int argc, char *argv[]) {  /* Socket */
  struct sockaddr_in serverAddress; /* Local address */
  struct sockaddr_in clientAddress; /* Client address */
  unsigned int cliAddrLen;          /* Length of incoming message */
  char echoBuffer[MESSAGEMAX];      /* Buffer for echo string */
  unsigned short serverPort;        /* Server port */
  int recvMsgSize;                  /* Size of received message */
  messageHeader *header;
  struct tm *ptm;
  char time_string[256];
  int clientMode = 0;
  int clientSequenceNum = 0;
  float avgLossRate = 0.0;
  unsigned int debugFlag = 1;
  unsigned int receivedCount = 0;

  if (argc < 2) /* Test for correct number of parameters */
  {
    fprintf(stderr, "PerfServer(v%s):  <UDP Port> [<Emulated Avg Loss Rate>] [<Debug Flag>]\n",
            Version);
    exit(1);
  }

  signal(SIGINT, ServerStop);

  serverPort = atoi(argv[1]); /* First arg:  local port */
  if (argc >= 3)
    avgLossRate = atof(argv[2]);

  if (argc >= 4)
    debugFlag = atoi(argv[3]);

  if (debugFlag > 0)
    printf("perfServer:(v:%s): Port:%d\n", (char *)Version, serverPort);

  /* Create socket for sending/receiving datagrams */
  if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) 
  {
    printf("perfServer:(v:%s): HARD ERROR:  Failed on socket (errno:%d)\n ", (char *)Version,errno);
    exit(1); 
  }

  /* Construct local address structure */
  memset(&serverAddress, 0, sizeof(serverAddress));                   /* Zero out structure */
  serverAddress.sin_family = AF_INET;                                 /* Internet address family */
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);                  /* Any incoming interface */
  serverAddress.sin_port = htons(serverPort);                         /* Local port */

  /* Bind to the local address */
  if (bind(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) 
  {
    printf("perfServer:(v:%s): HARD ERROR:  Failed on bind (errno:%d)\n ", (char *)Version,errno);
    exit(1); 
  }

  session *currentSession = NULL;
  srand(time(NULL));

  //Loop until:  sigint or error
  while (bStop != 1) 
  {
    /* Set the size of the in-out parameter */
    cliAddrLen = sizeof(clientAddress);

    /* Block until receive message from a client */
    if ((recvMsgSize =
             recvfrom(sock, echoBuffer, MESSAGEMAX, 0,
                      (struct sockaddr *)&clientAddress, &cliAddrLen)) < 0) {
      if (bStop != 1)
        printf("Failure on recvfrom, client: %s, errno:%d\n",
               inet_ntoa(clientAddress.sin_addr), errno);
    } else {
      // printf("Message received!\n");
      receivedCount++;
      header = (messageHeader *)&echoBuffer;
      clientMode = ntohs(header->mode);
      clientSequenceNum = ntohl(header->sequenceNum);
      int rcvdSize = ntohs(header->size);
      // Find session - will return a new one if not found
      currentSession =
          getActive(clientAddress.sin_addr, clientAddress.sin_port);
      // If this is the last message...
      if (clientSequenceNum == 0xffffffff) {
        // ... "close" the session
        if (debugFlag >0)
          printf("Session Ended\n");
        updateSessionDuration(currentSession);
        removeActive(clientAddress.sin_addr, clientAddress.sin_port);
      } else {
        // ... otherwise, update the session info
        currentSession->bytesReceived += recvMsgSize;
        currentSession->messagesReceived++;
        currentSession->messagesLost +=
            clientSequenceNum - currentSession->lastSequenceNum - 1;
        currentSession->lastSequenceNum = clientSequenceNum;
      }

      if (debugFlag > 1) {
        printf("Size: %d, Mode: %d, SeqNum: %d\n", rcvdSize, clientMode,
               clientSequenceNum);
        printf("timestamp = %ld\n", header->timestamp);
        time_t t = header->timestamp;
        ptm = gmtime(&t);
        strftime(time_string, sizeof time_string, "%F %T", ptm);
        printf("%s\n", time_string);
      }
    }

    if (clientMode == 0 && (rand() / (float)RAND_MAX) > avgLossRate) {
      /* Send received datagram back to the client */
      if (sendto(sock, echoBuffer, recvMsgSize, 0,
                 (struct sockaddr *)&clientAddress,
                 sizeof(clientAddress)) != recvMsgSize) {
        printf("Failure on sendTo, client: %s, errno:%d\n",
               inet_ntoa(clientAddress.sin_addr), errno);
      }
    }

  }

  printf("\n%d %d\n", sessionCount, receivedCount);

  // Link any remaining active to the archived
  if (firstSession == NULL)
    firstSession = firstActive;
  else
    lastSession->next = firstActive;
  printSessions();
  freeAllSessions();

  fflush(stdout);
  exit(0);
}
