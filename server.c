#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include "dict.h"
#include "packet.h"

#ifndef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) if (debug) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, \
    __FILE__, __LINE__, __func__, ##args)
#endif

int socket_create(char *interface)
{
    int sockfd = 0;

    if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0) {
	perror("socket");
	return -1;
    }

    if ((setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface) + 1)) < 0) {
	perror("setsockopt");
	return -1;
    }

    return sockfd;
}

unsigned char *socket_capture(int sockfd, short debug)
{
    struct sockaddr saddr;
    size_t recv = 0;
    size_t sockaddr_len = sizeof(struct sockaddr);
    unsigned char *packet = calloc(1522, sizeof(char));

    recv = recvfrom(sockfd, packet, 1522, 0, &saddr, (socklen_t *)&sockaddr_len);

    if (recv < 0) {
	DEBUG_PRINT("recvfrom: Failed to get packet");
	return NULL;
    } else if (recv == 0) {
	DEBUG_PRINT("recvfrom: No packet received\n");
    } else {
	DEBUG_PRINT("recvfrom: Received %ld bytes\n", recv);
    }

    return packet;
}

struct packet *packet_process(unsigned char *packet, size_t size, short debug)
{
    struct iphdr *iph = NULL;
    struct iphdr *iph2 = NULL;
    struct udphdr *udph = NULL;
    struct sockaddr_in source;
    struct sockaddr_in dest;
    struct packet *p = NULL;
    
    size_t iph_len = 0;

    int dst_port = 0;
    int src_port = 0;

    char *payload = NULL;
    
    iph = (struct iphdr *)(packet + sizeof(struct ethhdr));
    iph_len = iph->ihl * 4;

    if (iph->protocol != IPPROTO_GRE) {
	return NULL;
    }
    
    // Get the inner IP header, which is 4 bytes after the GRE header.
    iph2 = (struct iphdr *)(packet + iph_len + 4 + sizeof(struct ethhdr));    

    memset(&source, 0, sizeof(struct sockaddr_in));
    memset(&dest, 0, sizeof(struct sockaddr_in));

    source.sin_addr.s_addr = iph2->saddr;
    dest.sin_addr.s_addr = iph2->daddr;

    // Get the UDP header, add 24 bytes for the GRE and extra IP header.
    udph = (struct udphdr *)(packet + iph_len + 24 + sizeof(struct ethhdr));

    payload = (char *)udph + sizeof(struct udphdr);

    dst_port = ntohs(udph->dest);
    src_port = ntohs(udph->source);

    if ((p = calloc(0, sizeof(struct packet))) == NULL) {
	perror("calloc");
    }
    
    p->source = strdup(inet_ntoa(source.sin_addr));
    p->dest = strdup(inet_ntoa(dest.sin_addr));
    p->src_port = src_port;
    p->dst_port = dst_port;
    p->payload = strdup(payload);

    return p;
}

void usage(char **argv)
{
    printf("Usage: %s <interface> <debug>\n", argv[0]);
    exit(1);
}

int main(int argc, char **argv)
{
    int i = 0;
    int sockfd = 0;
    short debug = 0;
    unsigned char *packet = NULL;
    char *iface = NULL;
    char *key = NULL;
    size_t key_len = 0;
    
    struct packet *p = NULL;
    
    if (argc < 2) {
	usage(argv);
    }

    iface = strdup(argv[1]);

    if (argc == 3) {
	debug = atoi(argv[2]);
    }

    sockfd = socket_create(iface);

    if (sockfd < 0) {
	printf("Failed to create socket\n");
	return -1;
    }

    printf("Listening on interface %s\n", iface);
    
    while (1) {		
	packet = socket_capture(sockfd, debug);
	p = packet_process(packet, 1522, debug);

	if (!p)
	    continue;

	if (!strcmp(p->payload, "-1"))
	    break;

	key_len = snprintf(NULL, 0, "%s:%d", p->dest, p->src_port);
	key = calloc(key_len + 1, sizeof(char));
	snprintf(key, key_len + 1, "%s:%d", p->dest, p->src_port);	

	if (i + 1 % 1000 == 0) {
	    printf("\rReceived packet %d, waiting for stop packet...", i + 1);
	    fflush(stdout);
	}

	printf("\rReceived packet %d, waiting for stop packet...", i + 1);
	fflush(stdout);
	
	dict_insert(key, p);
	i++;
    }
    
    dict_print_payload();
    
    return 0;
}
