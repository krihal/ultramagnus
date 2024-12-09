#include <arpa/inet.h>
#include <getopt.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
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

#ifndef MAX_THREADS
#define MAX_THREADS 100
#endif

char **buffer;

struct thread_args {
    char *source;
    int debug;
    int dport;
    int nr_prefixes;
    int sport;
    int ptrain;
};

/*
 * Send server notification
 */
void send_server_notification(char *path, int debug, char *message)
{
    int sock;
    struct sockaddr_un server;

    // Connect to the UNIX socket in path and write a message
    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, path);

    DEBUG_PRINT("Connecting to server at %s\n", path);

    if (connect(sock, (struct sockaddr *)&server, sizeof(struct sockaddr_un)) < 0) {
	perror("Error: Could not connect to server: ");
	return;
    }

    if (write(sock, message, strlen(message)) < 0) {
	perror("Error: Could not write to server: ");
    }

    close(sock);
}

/*
 * Send a packet to the destination
 */
int send_packet(int sockfd, char *dest, int dport, char *source, int sport, long long seqno)
{
    struct sockaddr_in dest_addr;

    char payload[8];
    ssize_t sent = 0;

    memset(&dest_addr, '0', sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dport);

    snprintf(payload, sizeof(payload), "%lld", seqno);

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
 * Print the usage of the program
 */
void usage(char *progname)
{
    printf("Usage: %s [OPTION]\n"				\
	   "  -d=[Destination port]\n"				\
	   "  -s=[Source_ip]\n"					\
	   "  -p=[Source ports, delimited by ',']\n"		\
	   "  -f=[Prefix file]\n"				\
	   "  -t=[Packet train]\n"				\
	   "  -u=[Unix socket path]\n"				\
	   "  -D [Debug]\n", progname);
    printf("\nExample: %s -d 4001 -s 109.105.111.8 -p 4001,4002 -f ../prefixes\n", progname);
    exit(1);
}

/*
 * This function reads the prefixes from the file
 */
int read_prefixes(char *filename, int debug)
{
    FILE *fd;
    char *line = NULL;
    int i = 0;
    size_t len = 0;
    ssize_t read = 0;

    fd = fopen(filename, "r");

    if (fd == NULL) {
	printf("Failed to read file %s\n", filename);
	exit(-1);
    }

    // Allocate memory for the buffer
    buffer = malloc(sizeof(char *));

    // Read the prefixes from the file and put each one in the buffer
    while ((read = getline(&line, &len, fd)) != -1) {
	line[strcspn(line, "\n")] = '\0';

	buffer = realloc(buffer, (i + 1) * sizeof(char *));
	buffer[i] = strdup(line);

	i++;
    }

    DEBUG_PRINT("Read %d prefixes from \"%s\"\n", i, filename);

    return i;
}

/*
 * This function sends packets to the destination
 */
void *packet_thread(void *input)
{
	int i = 0;
	int j = 0;
	int sockfd = 0;
	int sock_opt = 1;

	struct sockaddr_in source_addr;

	// Read the input arguments from the thread_args structure
	int debug = ((struct thread_args *)input)->debug;
	int dport = ((struct thread_args *)input)->dport;
	int nr_prefixes = ((struct thread_args *)input)->nr_prefixes;
	int ptrain = ((struct thread_args *)input)->ptrain;
	int sport = ((struct thread_args *)input)->sport;
	char *source = ((struct thread_args *)input)->source;

	DEBUG_PRINT("Sending packets from port %d\n", sport);

	// Create a socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	    perror("Error: Could not create socket: ");
	}

	// Set the socket options
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sock_opt, sizeof(sock_opt)) < 0) {
	    perror("Error: Could not set socket options: ");
	}

	// Bind the socket to the source address
	memset(&source_addr, '0', sizeof(source_addr));
	source_addr.sin_family = AF_INET;
	source_addr.sin_addr.s_addr = INADDR_ANY;
	source_addr.sin_port = htons(sport);

	if (bind(sockfd, (struct sockaddr *)&source_addr, sizeof(source_addr)) < 0) {
	    perror("Error: Could not bind socket: ");
	}

	// Send the packets, one to each destination
	for (i = 0; i < nr_prefixes; i++) {
	    for (j = 0; j < ptrain; j++) {
		DEBUG_PRINT("%s:%d/UDP -> %s:%d/UDP\n", source, dport, buffer[i], sport);

		if (send_packet(sockfd, buffer[i], dport, source, sport, j) < 0) {
		    perror("Error: Could not send packet: ");
		}
	    }
	}

	close(sockfd);

	return NULL;
}

/*
 * Main function
 */
int main(int argc, char **argv)
{
    char *file = NULL;
    char *message = NULL;
    char *rest = NULL;
    char *sockpath = NULL;
    char *source = NULL;
    char *sports = NULL;
    char *token = NULL;

    int c = 0;
    int debug = 0;
    int dport = 0;
    int nr_prefixes = 0;
    int thread_id = 0;
    int ptrain = 0;

    size_t message_len = 0;
    
    pthread_t thread_ids[MAX_THREADS];

    struct thread_args packet_args[100];

    // Parse the command line arguments
    while ((c = getopt(argc, argv, "d:s:p:f:t:u:h?D")) != -1) {
	switch (c) {
	case 'd':
	    dport = atoi(optarg);
	    break;
	case 's':
	    source = optarg;
	    break;
	case 'p':
	    sports = strdup(optarg);
	    break;
	case 'f':
	    file = optarg;
	    break;
	case '?':
	    usage(argv[0]);
	    break;
	case 'h':
	    usage(argv[0]);
	    break;
	case 'D':
	    debug = 1;
	    break;
	case 't':
	    ptrain = atoi(optarg);
	    break;
	case 'u':
	    sockpath = optarg;
	    break;
	default:
	    usage(argv[0]);
	}
    }

    // Read the prefixes from the file
    nr_prefixes = read_prefixes(file, debug);
    DEBUG_PRINT("Got %d prefixes\n", nr_prefixes);

    message_len = snprintf(NULL, 0, "client_start:%d", ptrain);
    message = malloc(message_len + 1);
    snprintf(message, message_len + 1, "client_start:%d", ptrain);
    
    send_server_notification(sockpath, debug, message);

    usleep(500);

    // Split the source ports and create a thread for each
    rest = sports;
    while ((token = strtok_r(rest, ",", &rest))) {
	// Construct the packet arguments
	packet_args[thread_id].dport = dport;
	packet_args[thread_id].nr_prefixes = nr_prefixes;
	packet_args[thread_id].source = source;
	packet_args[thread_id].sport = atoi(token);
	packet_args[thread_id].debug = debug;
	packet_args[thread_id].ptrain = ptrain;

	// Create the thread
	pthread_create(&thread_ids[thread_id], NULL, packet_thread,
		       &packet_args[thread_id]);

	thread_id++;
    }
    
    for (int i = 0; i < thread_id; i++) {
	pthread_join(thread_ids[i], NULL);
    }

    DEBUG_PRINT("Sending client done notification\n");
    send_server_notification(sockpath, debug, "client_done");

    return 0;
}
