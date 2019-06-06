/* Blaster
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
#include <sys/wait.h>
#include <time.h>
#include "packet.h"

#define DATA_PACKET 'D'
#define END_PACKET 'E'
#define ECHO_PACKET 'C'
#define BAD_PACKET 'X'


// Flag and signal handler which will be used for SIGINT
static volatile sig_atomic_t doneflag = 0;
static void setdoneflag(int signo) {
	doneflag = 1;
}

static void printusage(void){
  fprintf(stderr, "useage: ./blaster -s <hostname> -p <port> -r <rate> -n <num> -q <seq> -l <len> -c <echo>\n");
}


int main(int argc, char* argv[]) {
	
  /* check */
  if(argc < 15 || argc > 15){
    printf("Incorrect number of arguments\n"); 
    printusage();
    exit(2);
  }


  int numargs = 0;

  /* type of packet */
  char type  = 'D';  // initial setup

  /* packet header */
  struct packet pack;
  //  struct packet * packp;

  /* nanosleep */
  struct timespec t, tremain; // won't ever look at tremain

  /* getaddressinfo */  
  char *hostname = NULL, *servicename = NULL;

  /* parameters */
  unsigned int rate = 0, num = 0, seq = 0, len = 0, echo = 0;
  char echoport[100];
  /* send loop */
  int x;


  /* getopt */
  int c;
  extern char *optarg;
  extern int optind, optopt, opterr;
  int negcheck = 0;


  while ((c = getopt(argc, argv, ":s:p:r:n:q:l:c:")) != -1) {
    switch(c) {
    case 's':
      numargs++;
      hostname = optarg; 
      break;
    case 'p':
      numargs++;
      servicename = optarg; 
      int port = atoi(optarg);
      if(port < 1025 || port > 65535){
	fprintf(stderr, "Incorrect port, 1024 < port < 65536.\n");
	printusage();
	exit(2);
      }
      else{
	port++;
	if(port == 65536)
	  port = 65534;
	sprintf(echoport, "%d", port);
      }
      break;
    case 'r':
      numargs++;
      if( (negcheck = atoi(optarg)) == 0){
	printf("incorrect rate value\n");
	printusage();
	exit(1);
      }
      if(negcheck < 1){
	printf("incorrect rate value\n");
	printusage();
	exit(1);
      }

      rate = atoi(optarg);
      if(rate > 1000000000){
	rate = 1000000000;
	printf("setting rate to 1 packet per nanosecond\n");
      }
 
      break;
    case 'n':
      numargs++;
      if( (negcheck = atoi(optarg)) == 0){
	printf("incorrect packet number value\n");
	printusage();
	exit(1);
      }
      if(negcheck < 1){
	printf("incorrect packet number value\n");
	printusage();
	exit(1);
      }
      num = atoi(optarg); 
      break;
    case 'q':
      numargs++;
      if( (negcheck = atoi(optarg)) == 0){
	printf("incorrect sequence value\n");
	printusage();
	exit(1);
      }
      if(negcheck < 1){
	printf("incorrect sequence value\n");
	printusage();
	exit(1);
      }
      seq = atoi(optarg);  
        break;
    case 'l':
      numargs++;
      len = atoi(optarg);
      if(len > 50 * 1024){
	fprintf(stderr, "Payload length is too long, must be less than 50 KiB\n");
	printusage();
	exit(1); 
      }
      else if( len < 1){
	fprintf(stderr, "Payload length is too short, must be at least 1 byte\n");
	printusage();
	exit(1);
      }
      break;
    case 'c':
      numargs++;
      echo = atoi(optarg);
      if(  echo == 1){
	//	type = 'C';  blastee changes this when echoed back
      }
      else if( echo != 0){
	printf("Invalid value for echo.\n");
	printusage();
	exit(2);
      }      
      break;
    case ':':
      fprintf(stderr, "-%c without parameter\n", optopt);
      printusage();
      exit(2);
      break;
    case '?':
      fprintf(stderr, "unknown arg %c\n", optopt);
      printusage();
      exit(2);
      break;
    }
}


  /* double check */

  if( numargs != 7){
    printf("Need all arguments\n");
    printusage();
    exit(1);
  }




	/* Set up signal handler for graceful exit on SIGINT */
	struct sigaction act;
	act.sa_handler = setdoneflag;
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1)) {
		perror("Failed to set SIGINT handler");
		exit(1);
	}

	/* setup socket */
	
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	/* We use getaddrinfo instead of the deprecated gethostbyname() function
	   so that we can be agnostic about IPv4/v6.  */
	int status;
	if ((status = getaddrinfo(hostname, servicename, &hints, &servinfo)) != 0) {
		printf("Error: %s\n", gai_strerror(status));
		exit(1);
	}
	/*  getaddrinfo returns a linked list of addrinfo structs, each of which corresponds
	 *  to info about one of possibly many network interfaces on this host. 
	 *  Thus we'll loop through each one and bind to the first one that works. */
	for (p = servinfo; p != NULL; p = p->ai_next) {
		// note that we directly pass info from addrinfo struct to socket()
		if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("blaster: socket");
			continue;
		}
	

		/* NO BINDING DONE since its not my socket */

		break; // if we get here, we either have a socket established or we
			   // couldn't find a suitable addrinfo
	}
	if (p == NULL) { // if we didn't find one...
		printf("blaster: failed to bind socket\n");
		exit(2);
	}
	
	/* save for echo if needed */
	if(!echo)
	  freeaddrinfo(servinfo);

	/* set up nanosleep based on rate */
	
	if(rate > 1){
	  t.tv_sec = 0; // no whole seconds
	  t.tv_nsec = 1000000000 / rate;  // time in nano sec
	}
	else{
	  t.tv_sec = 1; 
	  t.tv_nsec = 0; 
	}


	/* start blasting */

	/* set up packet */
	pack.seq = seq;
	pack.len = len;
 
	/* set up buffer */

	unsigned long size = pack.len + sizeof(struct packet) - sizeof(int) + sizeof(char);
	unsigned char *buf = (unsigned char*)malloc(size);
	unsigned char *buf_rx = (unsigned char*)malloc(size); // for echo
	
	memset(buf, 'P', sizeof(buf)); // actually put something in it


 /* set up a socket to receive on */

	    int sockfd_rx = 0;
	    struct addrinfo  *e;
	    struct packet *pack_rx;	
	    int numBytes = 0;

	/* get a child if receiving echo packets */
	if(echo){

	    /* hints are the same, just new port */
	  // if ((status = getaddrinfo(NULL, "1029", &hints, &servinfo)) != 0) {
	    if ((status = getaddrinfo(NULL, echoport, &hints, &servinfo)) != 0) {
	      fprintf(stderr, "Error: %s\n", gai_strerror(status));
	      exit(1);
	    }
	    /*  getaddrinfo returns a linked list of addrinfo structs, each of which corresponds
	     *  to info about one of possibly many network interfaces on this host. 
	     *  Thus we'll loop through each one and bind to the first one that works. */
	    for (e = servinfo; e != NULL; e = e->ai_next) {
	      // note that we directly pass info from addrinfo struct to socket()
	      if ((sockfd_rx = socket(e->ai_family, e->ai_socktype, e->ai_protocol)) == -1) {
		perror("blaster: socket echo");
		continue;
	      }

	      if (bind(sockfd_rx, e->ai_addr, e->ai_addrlen) == -1) {
		close(sockfd_rx);
		perror("blaster: bind echo");
		continue;
	      }
		
	      break; // if we get here, we either have a socket established or we
	      // couldn't find a suitable addrinfo
	    }
	    if (e == NULL) { // if we didn't find one...
	      fprintf(stderr, "blaster: failed to bind socket for echo\n");
	      exit(1);
	    }

	    freeaddrinfo(servinfo);

 
	} // end echo socket setup


	/* send loop */
	for( x = 0; x < num; x++){

	  /* switch sequence number to network byte order*/
	  pack.seq = htonl(pack.seq);

	  /* set first 4 bytes of payload to random number */
	  pack.payload = rand();

	  /* if end packet, set type */
	  if(x == num - 1){
	    type = 'E'; 
	  }

	  /* add packet to buffer */
	  buf[0] = type;
	  memcpy(&buf[1], &pack, sizeof(struct packet));

	  /* if payload length is less than 4 FIX ME*/
	 

	  /* send packet */
	  if (sendto(sockfd, buf, size, 0, p->ai_addr, p->ai_addrlen) < 0){
	    fprintf(stderr, "sendto error\n");
	    exit(1);
	  }

	  /* switch sequence number back to normal order*/
	  pack.seq = ntohl(pack.seq);

	  /* print some stuff about packet */
	  // printf("BLASTER -> type : %c seq: %u payload: 0x%08X\n", type, packp->seq, packp->payload);
	  int y;
	  /* print sequence number and first byte */
	  printf("BLAST -> sequence: %u payload: 0x%02x", pack.seq, buf[9] );
	  /* at most prints 10, 11, 12 */
	  for (y = 10; y < 13 && y < len + 9; y++){ 
	    printf("%02x", buf[y] );
	  }
	  printf("\n");
	  /* increment sequence number */
	  pack.seq += pack.len;

	  /* possibly get some echo packets from blastee */
	  if(echo){

	    /* start receiving */
	    char type_rx = 'C'; // for echo	   
	    numBytes = 0;
	    while (!doneflag && numBytes == 0) { //MUST FIX doneflag to use this line
	    // while (numBytes == 0) {    
	      numBytes = recvfrom(sockfd_rx, buf_rx, size, 0, NULL, NULL); 

	      if (numBytes == -1) {
		if ( errno != EINTR ) {
		  perror("blaster: recvfrom");
		  exit(1);
		}
		continue;
	      }
		
	      if ( numBytes >= 9 ) {
		/* parse and print */

		/* get packet type */
		type_rx = (char) buf_rx[0];

		/* get header */
		pack_rx = (struct packet *) &buf_rx[1];
		/* switch to host byte ordering */
		pack_rx->seq = ntohl(pack_rx->seq);
		
		//		printf("echo <- packet type: %c seq: %u len: %u, payload: 0x%08X\n", type_rx, pack_rx->seq, pack_rx->len, pack_rx->payload);
	/* print sequence number and first byte */
		printf("ECHO <- sequence: %u, payload: 0x%02x", pack_rx->seq, buf_rx[9]); 
	
	  /* at most prints 10, 11, 12 */
	  for (y = 10; y < 13 && y < len + 9; y++){ 
	    printf("%02x", buf_rx[y] );
	  }
	  printf("\n");
	      }
	      else {
		fprintf(stderr, "Blaster error: Incomplete packet header received, only %d bytes.\n", numBytes);
		exit(1);
	      }
	    }

	  }

	  
	  /* now go to sleep */       
	  nanosleep(&t, &tremain);
	
	} // end for loop

	close(sockfd_rx);
	exit(0);

} // end main
