#include <stdio.h>		/* for printf() and fprintf() */
#include <sys/socket.h>		/* for socket(), connect(), sendto(), and recvfrom() */
#include <arpa/inet.h>		/* for sockaddr_in and inet_addr() */
#include <stdlib.h>		/* for atoi() and exit() */
#include <string.h>		/* for memset() */
#include <unistd.h>		/* for close() and alarm() */
#include <errno.h>		/* for errno and EINTR */
#include <signal.h>		/* for sigaction() */
#include "gbnpacket.c"

#define TIMEOUT_SECS    1	/* Seconds between retransmits */
#define MAXTRIES        10000	/* Tries before giving up */

int tries = 0;			/* Count of times sent - GLOBAL for signal-handler access */
int base = 0;
int windowSize = 0;
int sendflag = 1;

void DieWithError (char *errorMessage);	/* Error handling function */
void CatchAlarm (int ignored);	/* Handler for SIGALRM */
int max (int a, int b);		/* macros that most compilers include - used for calculating a few things */
int min(int a, int b);		/* I think gcc includes them but this is to be safe */

int
main (int argc, char *argv[])
{
  int sock;			/* Socket descriptor */
  struct sockaddr_in gbnServAddr;	/* Echo server address */
  struct sockaddr_in fromAddr;	/* Source address of echo */
  unsigned short gbnServPort;	/* Echo server port */
  unsigned int fromSize;	/* In-out of address size for recvfrom() */
  struct sigaction myAction;	/* For setting signal handler */
  char *servIP;			/* IP address of server */
   int respLen;			/* Size of received datagram */
  int packet_received = -1;	/* highest ack received */
  int packet_sent = -1;		/* highest packet sent */
  char* str;
  long int str_len = 0;
  int chunkSize = 997;		/* chunk size in bytes */
  int nPackets = 0;		/* number of packets to send */
  
  if (argc != 3)		/* Test for correct number of arguments */
    {
      fprintf (stderr,
	       "Usage: %s <Server Port No> <Window Size>\n",
	       argv[0]);
      exit (1);
    }

  gbnServPort = atoi (argv[1]);	/* Use given port */
  windowSize = atoi (argv[2]);

  // nPackets--;
  /* Create a best-effort datagram socket using UDP */
	  if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) < 0)
	    DieWithError ("socket() failed");

	  /* Set signal handler for alarm signal */
	  myAction.sa_handler = CatchAlarm;
	  if (sigfillset (&myAction.sa_mask) < 0)	/* block everything in handler */
	    DieWithError ("sigfillset() failed");
	  myAction.sa_flags = 0;

	  if (sigaction (SIGALRM, &myAction, 0) < 0)
	    DieWithError ("sigaction() failed for SIGALRM");

	  /* Construct the server address structure */
	  memset (&gbnServAddr, 0, sizeof (gbnServAddr));	/* Zero out structure */
	  gbnServAddr.sin_family = AF_INET;
	//  gbnServAddr.sin_addr.s_addr = inet_addr (servIP);	/* Server IP address */
	  gbnServAddr.sin_addr.s_addr = htonl (INADDR_ANY);
	  gbnServAddr.sin_port = htons (gbnServPort);	/* Server port */

	  if (bind(sock, (struct sockaddr *)&gbnServAddr, sizeof(gbnServAddr)) < 0) {
		perror("bind failed:");
		return 0;
	  }
	 
	  fromSize = sizeof (fromAddr);
	  struct gbnpacket file_namepacket;

	  if((respLen = (recvfrom (sock, &file_namepacket, 1024, 0,(struct sockaddr *) &fromAddr, &fromSize))) > 0) {
 	printf("got the message\n"); 
	// open file here
	char* file_name = file_namepacket.data;
	printf("File name is: %s\n", file_name);
	FILE * file_p = fopen(file_name, "rb");
	if (file_p == NULL) 
	{
		printf("file wasn't opened\n");
		exit(1);
	} else {
		fseek(file_p, 0L, SEEK_END);
		str_len = ftell(file_p);
		rewind(file_p);
		str = malloc(str_len+1);
		size_t read_size = fread(str,1,str_len,file_p);
		str[read_size] = 0;
		fclose(file_p);
		printf("str_len is: %ld\n", str_len);
  		nPackets = str_len / chunkSize; 
  		if (str_len % chunkSize)
   			nPackets++;			/* if it doesn't divide cleanly, need one more odd-sized packet */
		printf("nPackets is: %d\n", nPackets);
	}
  }

  /* Send the file contents to the client */
  while ((packet_received < nPackets-1) && (tries < MAXTRIES))
    {
     // printf ("in the send loop base %d packet_sent %d packet_received %d\n",
	//      base, packet_sent, packet_received);
      if (sendflag > 0)
	{
	sendflag = 0;
	  int ctr; /*window size counter */
	  for (ctr = 0; ctr < windowSize; ctr++)
	    {
	      packet_sent = min(max (base + ctr, packet_sent),nPackets-1); /* calc highest packet sent */
	      struct gbnpacket currpacket; /* current packet we're working with */
	      if ((base + ctr) < nPackets)
		{
		  memset(&currpacket,0,sizeof(currpacket));
 		  printf ("sending packet %d packet_sent %d packet_received %d\n",
                      base+ctr, packet_sent, packet_received);

		  currpacket.th_seq = htonl (base + ctr);
		  currpacket.th_cwin = windowSize;
		  if (nPackets == base+ctr +1) {
		  	currpacket.th_fin = nPackets;
  		  } else {
		  	currpacket.th_fin = 0;
		  }
		  int currlength;
		  if ((str_len - ((base + ctr) * chunkSize)) >= chunkSize) /* length chunksize except last packet */
		    currlength = chunkSize;
		  else
		    currlength = str_len % chunkSize;
		  currpacket.length = htonl (currlength);
		  memcpy (currpacket.data, /*copy buffer data into packet */
			  str + ((base + ctr) * chunkSize), currlength);
		// 26 is the header length of the gbnpacket
		  if (sendto
		      (sock, &currpacket, 26 + currlength, 0, /* send packet */
		       (struct sockaddr *) &fromAddr,
		       sizeof (fromAddr)) !=
		      (26 + currlength))
		    DieWithError
		      ("sendto() sent a different number of bytes than expected");
		}
	    }
	}
      /* Get a response */

      alarm (TIMEOUT_SECS);	/* Set the timeout */
      struct gbnpacket currAck;
      while ((respLen = (recvfrom (sock, &currAck, sizeof (int) * 3, 0,
				   (struct sockaddr *) &fromAddr,
				   &fromSize))) < 0)
	if (errno == EINTR)	/* Alarm went off  */
	  {
	    if (tries < MAXTRIES)	/* incremented by signal handler */
	      {
		printf ("timed out\n");
		break;
	      }
	    else
	      DieWithError ("No Response");
	  }
	else
	  DieWithError ("recvfrom() failed");

      /* recvfrom() got something --  cancel the timeout */
      if (respLen > 0)
	{
	  printf("resplen: %d\n", respLen);
	  //int acktype = ntohl (currAck.type); /* convert to host byte order */
	  int ackno = ntohl (currAck.th_seq); 
	  printf("ackno: %d\n", ackno);
	  if (ackno > packet_received)
	    {
	      printf ("received ack\n"); /* receive/handle ack */
	      packet_received++;
	      base = packet_received+1; /* handle new ack */
	      if (packet_received == packet_sent) /* all sent packets acked */
		{
		  alarm (0); /* clear alarm */
		  tries = 0;
		  sendflag = 1;
		}
	      else /* not all sent packets acked */
		{
		  tries = 0; /* reset retry counter */
		  sendflag = 0;
		  alarm(TIMEOUT_SECS); /* reset alarm */

		}
	    }
	}
    }
  close (sock); /* close socket */
  exit (0);
}

void
CatchAlarm (int ignored)	/* Handler for SIGALRM */
{
  tries += 1;
  sendflag = 1;
}

void
DieWithError (char *errorMessage)
{
  perror (errorMessage);
  exit (1);
}

int
max (int a, int b)
{
  if (b > a)
    return b;
  return a;
}

int
min(int a, int b)
{
  if(b>a)
	return a;
  return b;
}
