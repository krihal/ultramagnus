#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifndef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) if (debug) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, \
    __FILE__, __LINE__, __func__, ##args)
#endif


/*
 * Send a packet to the destination
 */
int send_packet(int sockfd, char *dest, int dport, char *source, int sport, char *payload)
{
    struct sockaddr_in dest_addr;

    ssize_t sent = 0;

    printf("Sending packet to %s:%d from %s:%d with payload: %s\n", dest, dport, source, sport, payload);
    
    memset(&dest_addr, '0', sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dport);

    if (inet_pton(AF_INET, dest, &dest_addr.sin_addr) <= 0) {
	perror("Failed to convert IP address: ");
	return -1;
    }

    sent = sendto(sockfd, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    if (sent < 0) {
	perror("Failed to send packet: ");
    }

    return 0;
}


/*
 * Main function
 */
int main(int argc, char **argv)
{
    int i = 0;
    int sockfd = 0;
    int sport = 4001;
    int dport = 4001;
    int sock_opt = 1;

    size_t seq_len = 0;
    
    char *seq = NULL;
    char *source = "130.242.68.15";
    char *destination = "1.1.118.0";
    
    struct sockaddr_in source_addr;    
  
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("Error: Could not create socket: ");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt)) < 0) {
	perror("Error: Could not set socket options: ");
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, "enp9s0f1np1", sizeof("enp9s0f1np1")) < 0) {
	perror("Error: Could not set socket options interface: ");
    }
        
    memset(&source_addr, '0', sizeof(source_addr));
    source_addr.sin_family = AF_INET;
    source_addr.sin_addr.s_addr = INADDR_ANY;
    source_addr.sin_port = htons(sport);

    if (bind(sockfd, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
	perror("Error: Could not bind socket: ");
    }

    for (i = 0; i < 3; i++) {
	seq_len = snprintf(NULL, 0, "%d", i);
	seq = malloc(seq_len + 1);
	snprintf(seq, seq_len + 1, "%d", i);
	
	sport = 4000 + i;
	    
	if (send_packet(sockfd, destination, dport, source, sport, seq) < 0) {
	    perror("Error: Could not send packet: ");
	}
    }
    
    destination = "1.0.16.0";
    
    for (i = 0; i < 3; i++) {
	seq_len = snprintf(NULL, 0, "%d", i);
	seq = malloc(seq_len + 1);
	snprintf(seq, seq_len + 1, "%d", i);
	
	sport = 4000 + i;
	    
	if (send_packet(sockfd, destination, dport, source, sport, seq) < 0) {
	    perror("Error: Could not send packet: ");
	}
    }
    
    if (send_packet(sockfd, destination, dport, source, sport, "-1") < 0) {
	perror("Error: Could not send packet: ");
    }
    
    return 0;
}
