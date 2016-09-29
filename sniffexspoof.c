#include <pcap.h>

#include <stdio.h>

#include <string.h>

#include <stdlib.h>

#include <ctype.h>

#include <errno.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <netdb.h>

#include <netinet/in_systm.h>

#include <netinet/ip.h>

#include <netinet/ip_icmp.h>

/* default snap length (maximum bytes per packet to capture) */

#define SNAP_LEN 1518

/* ethernet headers are always exactly 14 bytes [1] */

#define SIZE_ETHERNET 14

/* Ethernet addresses are 6 bytes */

#define ETHER_ADDR_LEN     6

/* Ethernet header */

struct ethernet_hdr {

u_char  ether_dhost[ETHER_ADDR_LEN];    /* destination host address */

u_char  ether_shost[ETHER_ADDR_LEN];    /* source host address */

u_short ether_type;                     /* IP? ARP? RARP? etc */

};

/* IP header */

struct ip_hdr {

u_char  ip_vhl;                   /* version << 4 | header length >> 2 */

u_char  ip_tos;                 /* type of service */

u_short ip_len;                 /* total length */

u_short ip_id;                  /* identification */

u_short ip_off;                 /* fragment offset field */

u_char  ip_ttl;                 /* time to live */

u_char  ip_p;                   /* protocol */

u_short ip_sum;                 /* checksum */

struct  in_addr ip_src;           /* source address */

struct  in_addr ip_dst;    /* dest address */

};

#define IP_HL(ip)               (((ip)->ip_vhl) & 0x0f)

#define IP_V(ip)                (((ip)->ip_vhl) >> 4)

/* ICMP header */

struct icmp_hdr {

unsigned char type;       //icmp service type, 8 echo request, 0 echo reply

unsigned char code;       //icmp header code

unsigned short checksum;  //icmp header chksum

unsigned short id;        //icmp packet identification

unsigned short seq;       //icmp packet sequent

};

// We can use this function to create a packet and send it out.

void myping(struct ip_hdr *sniff_ip, const u_char *packet, int size_ip, int count, int id, int seq)

{

// Don’t miss the Ethernet header.

char buf[size_ip+SIZE_ETHERNET];

const struct ethernet_hdr *ethernet;

ethernet = (struct ethernet_hdr*)(packet);

int s, i;

struct ip_hdr *spoof_ip = (struct ip_hdr*)(buf+SIZE_ETHERNET);

struct icmp_hdr *spoof_icmp = (struct icmp_hdr*)(buf+size_ip+SIZE_ETHERNET);

struct hostent *hp, *hp2;

struct sockaddr_in dst;

int on;

int offset = 0;

on = 1;

bzero(buf, sizeof(buf));

/* Create RAW socket */

if((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)

{

perror("socket() error");

/* If something wrong, just exit */

exit(1);

}

/* socket options, tell the kernel we provide the IP structure */

if(setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0)

{

perror("setsockopt() for IP_HDRINCL error");

exit(1);

}

printf("Sending to %s from spoofed", inet_ntoa(sniff_ip->ip_dst));

printf(" %s\n", inet_ntoa(sniff_ip->ip_dst));

/* Ip structure, check the ip.h */

spoof_ip->ip_vhl = sniff_ip->ip_vhl;

spoof_ip->ip_tos = sniff_ip->ip_tos;

spoof_ip->ip_len = sniff_ip->ip_len;

spoof_ip->ip_id = id;

spoof_ip->ip_off = sniff_ip->ip_off;

spoof_ip->ip_ttl = sniff_ip->ip_ttl;

spoof_ip->ip_p = sniff_ip->ip_p;

spoof_ip->ip_sum =  sniff_ip->ip_sum;

spoof_ip->ip_src = sniff_ip->ip_dst;

spoof_ip->ip_dst = sniff_ip->ip_src;

dst.sin_addr = spoof_ip->ip_dst;

dst.sin_family = AF_INET;

spoof_icmp->type = 0;

spoof_icmp->code = 0;

spoof_icmp->checksum = 0;

spoof_icmp->id = id;

spoof_icmp->seq = seq;

if(sendto(s, packet, sizeof(buf), 0, (struct sockaddr *)&dst, sizeof(dst)) < 0)

{

fprintf(stderr, "offset %d: ", offset);

perror("sendto() error");

}

else

printf("sendto() is OK.\n");

/* close socket */

close(s);

}

// We can use this function to modify the original packet.

void myping2(struct ip_hdr *sniff_ip, char *packet, struct icmp_hdr *spoof_icmp, int size_ethernet, int size_ip)

{

int s;

int on = 1;

struct sockaddr_in dst;

/* Create RAW socket */

if((s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)

{

perror("socket() error");

/* If something wrong, just exit */

exit(1);

}

/* socket options, tell the kernel we provide the IP structure */

if(setsockopt(s, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0)

{

perror("setsockopt() for IP_HDRINCL error");

exit(1);

}

printf("Sending to %s from spoofed", inet_ntoa(sniff_ip->ip_dst));

printf(" %s\n", inet_ntoa(sniff_ip->ip_dst));

/* Ip structure, check the ip.h */

struct  in_addr tmp = sniff_ip->ip_src;

sniff_ip->ip_src = sniff_ip->ip_dst;

sniff_ip->ip_dst = tmp;

dst.sin_addr = sniff_ip->ip_dst;

dst.sin_family = AF_INET;

spoof_icmp->type = 0;

if(sendto(s, packet, sizeof(packet), 0, (struct sockaddr *)&dst, sizeof(dst)) < 0)

{

fprintf(stderr, "offset %d: ", 0);

perror("sendto() error");

}

else

printf("sendto() is OK.\n");

/* close socket */

close(s);

}

