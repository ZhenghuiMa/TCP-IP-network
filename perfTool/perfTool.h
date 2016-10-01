/*********************************************************
*
* Module Name: Perf tool lient/server header file
*
* File Name:    perfTool.h
*
* Summary:
*  This file contains common stuff for the client and server
*
* Revisions:
*
*********************************************************/
#include <arpa/inet.h> /* for sockaddr_in and inet_addr() */
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h> /* for in_addr */
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <sys/socket.h> /* for socket(), connect(), sendto(), and recvfrom() */
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h> /* for close() */

#define MESSAGEMIN 24
#define MESSAGEMAX 20000

#ifndef LINUX
#define INADDR_NONE 0xffffffff
#endif

void DieWithError(char *errorMessage); /* External error handling function */

long getMicroseconds(struct timeval *t) {
  return (t->tv_sec) * 1000000 + (t->tv_usec);
}

long getTimeSpan(struct timeval *start_time, struct timeval *end_time) {
  long usec2 = getMicroseconds(end_time);
  long usec1 = getMicroseconds(start_time);
  return (usec2 - usec1);
}

typedef struct {
  uint16_t size;
  uint16_t mode;
  time_t timestamp;
  uint32_t sequenceNum;
  struct timeval timeSent;
} messageHeader;
