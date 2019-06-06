/* Blastee
 * CS640 Project 1
 *
 * Authors: Jared Punzel & Jim O'Connor (Loethen)
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <ctype.h>

#define BUF_SIZE 50*1024
#define DATA_PACKET 'D'
#define END_PACKET 'E'
#define ECHO_PACKET 'C'
#define BAD_PACKET 'X'

// Flag and signal handler which will be used for SIGINT
static volatile sig_atomic_t doneflag = 0;
static void setdoneflag(int signo) {
	doneflag = 1;
}

char get_packet_type(unsigned char* buffer);
int get_sequence(unsigned char* buffer);
int get_packet_length(unsigned char* buffer);
void *get_in_addr(struct sockaddr *sa);

int main(int argc, char* argv[]) {


	/* Set up signal handler for graceful exit on SIGINT */
	struct sigaction act;
	act.sa_handler = setdoneflag;
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1)) {
		perror("Failed to set SIGINT handler");
		exit(1);
	}
	
	/* Lots of argument handling */
	int echo = 0;
	char *port_str = NULL;
	int portnum = 1024;
	int c;
	int numargs = 0;
	opterr = 0;
	if ( argc < 5 ) {
		fprintf(stderr, "Missing arguments.\n");
		fprintf(stderr, "Usage: %s -p <port> -c <echo>\n", argv[0]);
		exit(1);
	}
	if ( argc > 5 ) {
		fprintf(stderr, "too many arguments given.\n");
		fprintf(stderr, "Usage: %s -p <port> -c <echo>\n", argv[0]);
		exit(1);
	}
	while ((c = getopt(argc,argv,"c:p:")) != -1) {
		switch (c) {
			case 'c':
			  numargs++;
				if ( strcmp(optarg, "0") != 0 && strcmp(optarg, "1") != 0 ) {
					fprintf(stderr, "Invalid echo argument.\n");
					fprintf(stderr, "Usage: %s -p <port> -c <echo>\n", argv[0]);
					exit(1);
				} else {
					echo = (int)strtol(optarg, (char**)NULL,10);
									}
				break;
			case 'p':
			  numargs++;
				port_str = optarg;
				if ( (portnum = (int)strtol(optarg, (char**)NULL,10)) > 65535 || portnum <=1024 ) {
					fprintf(stderr, "Bad port # given\n");
					exit(1);
				}
				break;
			case '?':
				if (optopt == 'c' || optopt == 'p') {
					fprintf(stderr, "Missing arguments.\n");
					fprintf(stderr, "Usage: %s -p <port> -c <echo>\n", argv[0]);
				}
				else if (isprint(optopt)) {
					fprintf(stderr, "Bad argument %c.\n", optopt);
					fprintf(stderr, "Usage: %s -p <port> -c <echo>\n", argv[0]);
				}
				else {
					fprintf(stderr, "unknown option char \\x%x", optopt);
				}
				exit(1);
			default:
				abort();
		}
	}


  /* double check */

  if( numargs != 2){
    printf("Need all arguments\n");
    fprintf(stderr, "Usage: %s -p <port> -c <echo>\n", argv[0]);
    exit(1);
  }


	if ( port_str == NULL ) {
		fprintf(stderr, "Missing arguments.\n");
		fprintf(stderr, "Usage: %s -p <port> -c <echo>\n", argv[0]);
		exit(1);
	}
	/* End argument handling
	   Begin socket setup */
		
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	/* We use getaddrinfo instead of the deprecated gethostbyname() function
	   so that we can be agnostic about IPv4/v6.  */
	int status;
	if ((status = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) {
		fprintf(stderr, "Error: %s\n", gai_strerror(status));
		exit(1);
	}
	/*  getaddrinfo returns a linked list of addrinfo structs, each of which corresponds
	 *  to info about one of possibly many network interfaces on this host. 
	 *  Thus we'll loop through each one and bind to the first one that works. */
	for (p = servinfo; p != NULL; p = p->ai_next) {
		// note that we directly pass info from addrinfo struct to socket()
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("blastee: socket");
			continue;
		}
        int yes = 1;
        if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        } 
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("blastee: bind");
			continue;
		}
		
		break; // if we get here, we either have a socket established or we
			   // couldn't find a suitable addrinfo
	}
	if (p == NULL) { // if we didn't find one...
		fprintf(stderr, "blastee: failed to bind socket\n");
		exit(2);
	}
	freeaddrinfo(servinfo);



	/* End socket setup
	   Begin receiving and handling */
	
	printf("blastee: waiting to get blasted!\n");
	/* Now that we have everything set up, we're ready to do receiving */
	struct sockaddr_storage their_addr; // ipv6 compatible sockaddr struct
	socklen_t addr_len = sizeof(their_addr);
	char src[INET6_ADDRSTRLEN]; // string for source address
	
	unsigned char *buf = (unsigned char*)malloc(BUF_SIZE*sizeof(unsigned char)); // recvfrom buffer
	int numBytes, totalBytes, sentBytes, numPackets = 0; // byte counts for recvfrom, whole session, individual packets
	
	// packet info
	char packetType;
	int sequence, packetLength;
	// timing stuff
	struct timeval startTime, nowTime;
	gettimeofday(&startTime, NULL);
	// echo socket stuff
    struct addrinfo *e = NULL;
	int sockfd_tx = 0;

	totalBytes = 0;
	while (!doneflag) { // all the work gets done here.

		numBytes = recvfrom(sockfd, buf, BUF_SIZE-1, 0, (struct sockaddr*)&their_addr, &addr_len);
		if (numBytes == -1) {
			if ( errno != EINTR ) { // not really an error if we get ctrl-C
				perror("blastee: recvfrom");
			}
			continue;
		}
		inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), src, sizeof(src) );
	
		if (totalBytes == 0) { // get the start time on the first iteration only
			gettimeofday(&startTime, NULL);
		}
		gettimeofday(&nowTime, NULL);
		
		// if we got a full header, then 
		if ( numBytes >= 9 ) {
			
			numPackets++;
			
			if ( (packetType = get_packet_type(buf)) != 'D' && packetType != 'E' ) {
				fprintf(stderr, "Invalid packet type in header");
			}
			if ( packetType == 'E' ) {
				doneflag = true;
			}
			sequence = get_sequence(buf);
			if ( (packetLength = get_packet_length(buf)) > BUF_SIZE || packetLength < 0 ) {
				fprintf(stderr, "Invalid packet length of %d", packetLength);
				continue;
			}
			/* Echo section -- only if it was specified */
			if ( echo ) { 
				if (totalBytes == 0) { // set up socket on first iteration. inefficient to reset it every time.
                    
                    int echoPort;
                    // decide echo port number: listening port + 1
		    // if ( portnum == 65536 ) {  65536 is out of range
                    if ( portnum == 65535 ) {
		      echoPort = 65534; 
                    } else {
                        echoPort = portnum+1;
                    }
                    char echoPort_str[6];
                    sprintf(echoPort_str, "%d", echoPort);
                    
					if ( (status = getaddrinfo(src, echoPort_str, &hints, &servinfo)) != 0) {
						fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
						exit(1);
					}
					for ( e = servinfo; e != NULL; e = e->ai_next) {
						if ( (sockfd_tx = socket(e->ai_family, e->ai_socktype, e->ai_protocol)) == -1) {
							perror("blastee: echo socket");
							continue;
						}
						break;
					}

				}
				buf[0] = 'C'; // change data field to C
				
				if ((sentBytes = sendto(sockfd_tx, buf, numBytes, 0, e->ai_addr, e->ai_addrlen )) == -1) {
					perror("blastee: echo sendto");
				}
			}
			totalBytes += numBytes;
			
			// Print summary of every DATA packet
			if ( packetType == 'D' ) {
				double now = (nowTime.tv_sec-startTime.tv_sec)*1000.0+(nowTime.tv_usec-startTime.tv_usec)/1000.0;
				printf("Source: %s(%s) | Length: %d | Seq#: %d | Time: %.1f ",
					src, port_str, packetLength, sequence, now); 
				printf("| Payload: 0x");
				// we don't want to print too much if the payload is very small
				int i,x;
				if ( packetLength > 4 ) {
		
					x = 4;
				} else {
					x = packetLength;
				}
				for (i = 9; i < 9+x; i++ ) {
					printf("%02x", buf[i]);
				}
				printf("\n");
			}
		}
		else { // we didn't see a full header and didn't do anything
			fprintf(stderr, "Blastee error: Incomplete packet header received.\n");
		}
		
	
	}
	
	close(sockfd_tx);
	free(buf);
	printf("\nDone.\n");
	struct timeval endTime;
	gettimeofday(&endTime,NULL);
	double elapsedTime = (endTime.tv_sec - startTime.tv_sec) * 1000.0;
	elapsedTime += (endTime.tv_usec - startTime.tv_usec) / 1000.0;
	printf("Elapsed time: %.2f s\n", elapsedTime/1000);
	printf("Packets received: %d\n", numPackets);
	printf("Bytes received: %d\n", totalBytes);
	printf("Average packets/sec: %.2f\n", (double)numPackets/(elapsedTime/1000));
	printf("Average bytes/sec: %.2f\n", (double)totalBytes/(elapsedTime/1000));
	
	return 0;
}

/* Header parsing functions */
char get_packet_type(unsigned char* buffer) {
	switch (buffer[0]) {
		case DATA_PACKET:
			return DATA_PACKET;
			break;
		case END_PACKET:
			return END_PACKET;
			break;
		case ECHO_PACKET:
			return ECHO_PACKET;
			break;
		default:
			return BAD_PACKET;
			break;
	}
}
int get_sequence(unsigned char* buffer) {
  unsigned int *uip = (unsigned int *)&buffer[1];
  unsigned int x = *uip;
  return ntohl(x);

}
int get_packet_length(unsigned char* buffer) {
  unsigned int *uip = (unsigned int *)&buffer[5];
  unsigned int x = *uip;
  //  return ntohl(x);
  return x;
}



/* This is for ipv6 compatibility. */
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
