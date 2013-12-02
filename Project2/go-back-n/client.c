#include <stdio.h>		/* for printf() and fprintf() */
#include <sys/socket.h>		/* for socket() and bind() */
#include <arpa/inet.h>		/* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>		/* for atoi() and exit() */
#include <string.h>		/* for memset() */
#include <unistd.h>		/* for close() */
#include <errno.h>
#include <memory.h>
#include <signal.h>
#include "gbnpacket.c"		/* defines go-back-n packet structure */

void DieWithError (char *errorMessage);	/* External error handling function */
void CatchAlarm (int ignored);
int check_if_corrupt(float);
int check_if_lost(float);


int
main (int argc, char *argv[])
{
  int sock;			/* Socket */
  struct sockaddr_in gbnServAddr;	/* Local address */
  struct sockaddr_in gbnClntAddr;	/* Client address */
  unsigned int cliAddrLen;	/* Length of incoming message */
  unsigned short gbnServPort;	/* Server port */
  int recvMsgSize;		/* Size of received message */
  int packet_rcvd = -1;		/* highest packet successfully received */
  struct sigaction myAction;
  double lossRate, corruptionRate;		/* loss rate as a decimal, 50% = input .50 */
  char* servIP;
  char* file_name;


  if (argc < 4 || argc > 6)		/* Test for correct number of parameters */
    {
      fprintf (stderr, "Usage:  %s <UDP SERVER IP> <UDP SERVER PORT> <FILE_NAME> [<LOSS RATE>] [<CORRUPTION RATE>]\n", argv[0]);
      exit (1);
    }

  gbnServPort = atoi (argv[2]);	/* First arg:  local port */
  servIP = argv[1];
  file_name = argv[3];

  srand(time(NULL));
  if(argc == 6) {
	corruptionRate = atof(argv[5]);
	lossRate = atof(argv[4]);	/* loss rate as percentage i.e. 50% = .50 */
  }
  else if(argc == 5) {
	corruptionRate = 0.0;
	lossRate = atof(argv[4]);
  }
  else {
	lossRate = 0.0;			/*default to 0% intentional loss rate */
	corruptionRate = 0.0;
  }

  /* Create socket for sending/receiving datagrams */
  if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
    DieWithError ("socket() failed");

  /* Construct local address structure */
  memset (&gbnServAddr, 0, sizeof (gbnServAddr));	/* Zero out structure */
  gbnServAddr.sin_family = AF_INET;	/* Internet address family */
  gbnServAddr.sin_addr.s_addr = htonl (INADDR_ANY);
  gbnServAddr.sin_port = htons (0);	/* Local port */

  /* Bind to the local address */
  if (bind (sock, (struct sockaddr *) &gbnServAddr, sizeof (gbnServAddr)) <
      0)
    DieWithError ("bind() failed");
  myAction.sa_handler = CatchAlarm;
  if (sigfillset (&myAction.sa_mask) < 0)/*setup everything for the timer - only used for teardown */
    DieWithError ("sigfillset failed");
  myAction.sa_flags = 0;
  if (sigaction (SIGALRM, &myAction, 0) < 0)
    DieWithError ("sigaction failed for SIGALRM");

  // File processing

  // send over file name
  struct gbnpacket currpacket;
  currpacket.th_seq = htonl (0);
  currpacket.length = htonl (strlen(file_name)); 
  printf("FILE NAME: %s\n", file_name);
  memcpy(currpacket.data, file_name, strlen(file_name));
  printf("size of gbnpacket: %d\n", sizeof(currpacket));

  memset((char *) &gbnClntAddr, 0, sizeof(gbnClntAddr));
  gbnClntAddr.sin_family = AF_INET;
  gbnClntAddr.sin_port = htons(gbnServPort);
  if (inet_aton(servIP, &gbnClntAddr.sin_addr)==0) {
     fprintf(stderr, "inet_aton() failed\n");
     exit(1);
  }
 
  // send the file over
  if(sendto(sock, &currpacket, 18 + strlen(file_name), 0, (struct sockaddr *) &gbnClntAddr, sizeof(gbnClntAddr)) < 0) {
      	perror("sendto");
        exit(1);
  }
 
   printf("sent message\n");
  // receive an ack, then receive the file's contents

  for (;;)			/* Run forever */
  {
      /* set the size of the in-out parameter */
      cliAddrLen = sizeof (gbnClntAddr);
      struct gbnpacket currPacket; /* current packet that we're working with */
      memset(&currPacket, 0, sizeof(currPacket));
      /* Block until receive message from a client */
      recvMsgSize = recvfrom (sock, &currPacket, sizeof (currPacket), 0, /* receive GBN packet */
			      (struct sockaddr *) &gbnClntAddr, &cliAddrLen);
      printf("got here\n");
      currPacket.length = ntohl (currPacket.length); /* convert from network to host byte order */
      currPacket.th_seq = ntohl (currPacket.th_seq);
      printf("in else condition\n");
      int packet_is_corrupt = check_if_corrupt(corruptionRate);
      int packet_is_lost = check_if_lost(lossRate);
      if(packet_is_lost) {
	printf("packet was lost\n");
        continue;
      }
      else if(packet_is_corrupt)
      {
	struct gbnpacket currAck; /* ack packet */
	currAck.th_seq = htonl (packet_rcvd);
	currAck.length = htonl(0);
	if (sendto (sock, &currAck, sizeof (currAck), 0, /* send ack */
                      (struct sockaddr *) &gbnClntAddr,
                      cliAddrLen) != sizeof (currAck))
            DieWithError
              ("sendto() sent a different number of bytes than expected");	
	printf("packet was corrupted\n");
	continue; /* drop packet - for testing/debug purposes */
      }
      printf ("---- RECEIVE PACKET %d length %d\n", currPacket.th_seq, currPacket.length);
      printf("PACKET CONTENTS: %s\n", currPacket.data);
	// write to file for diff command to work
      FILE *fp;
      fp = fopen("result.txt", "a");
      fprintf(fp, "%s", currPacket.data);
      fclose(fp);

      if (currPacket.th_seq >= packet_rcvd + 1)
      {
	packet_rcvd++;
      }
      printf ("---- SEND ACK %d\n", packet_rcvd);
      struct gbnpacket currAck; /* ack packet */
      currAck.th_seq = htonl (packet_rcvd);
      currAck.length = htonl(0);
      if (sendto (sock, &currAck, sizeof (currAck), 0, /* send ack */
		      (struct sockaddr *) &gbnClntAddr,
		      cliAddrLen) != sizeof (currAck))
	    DieWithError
	      ("sendto() sent a different number of bytes than expected");
  }
  /* NOT REACHED */
}

int
check_if_corrupt(float corruptionRate)
{
  int is_corrupt = 0;
  float rand_num = rand() % 100; // pseudo-random number
  float p = 100.0 * corruptionRate;

//  printf("rand_num is: %f\n", rand_num);
//  printf("p is: %f\n", p);

  int packet_is_corrupted = 0;
  if(rand_num < p)
  {
	is_corrupt = 1;
  }

 // printf("Packet is corrupted: %i\n", is_corrupt);
  return is_corrupt;
}

int
check_if_lost(float lossRate) {
  int is_lost = 0;
  float rand_num = rand() % 100;
  float p = 100.0 * lossRate;

  //printf("loss rand_num is: %f\n", rand_num);
 // printf("loss p is: %f\n", p);

  int packet_is_lost = 0;
  if(rand_num < p) {
  	is_lost = 1;
  }
 
 // printf("Packet is lost: %i\n", is_lost);
  return is_lost;
}
void
DieWithError (char *errorMessage)
{
  perror (errorMessage);
  exit (1);
}

void
CatchAlarm (int ignored) /* just exit when encounter timeout */
{
  exit(0);
}
