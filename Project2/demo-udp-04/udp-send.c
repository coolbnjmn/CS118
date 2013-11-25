/*
        demo-udp-03: udp-send: a simple udp client
	send udp messages
	This sends a sequence of messages (the # of messages is defined in MSGS)
	The messages are sent to a port defined in SERVICE_PORT 

        usage:  udp-send

        Paul Krzyzanowski
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include "port.h"
#include "protocol-header.h"

#define BUFLEN 1024
#define MSGS 5	/* number of messages to send */

int main(int argc, char* argv[])
{
	struct sockaddr_in myaddr, remaddr;
	int fd, i;
	socklen_t slen=sizeof(remaddr);
	char buf[BUFLEN];	/* message buffer */
	int recvlen;		/* # bytes in acknowledgement message */
	int msgcnt = 0;
	
	if (argc < 4) {
		printf("Need server address, port num, and filename to send\n");	
		exit(1);
	}
	
	char *server = argv[1];	/* change this to use a different server */
	int  portno = atoi(argv[2]);
	char *file_name = argv[3];
		
	printf("%s\n", server);
	printf("%d\n", portno);
	printf("%s\n", file_name);

	/* create a socket */

	if ((fd=socket(AF_INET, SOCK_DGRAM, 0))==-1)
		printf("socket created\n");

	/* bind it to all local addresses and pick any port number */

	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(0);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}       

	/* now define remaddr, the address to whom we want to send messages */
	/* For convenience, the host address is expressed as a numeric IP address */
	/* that we will convert to a binary format via inet_aton */

	memset((char *) &remaddr, 0, sizeof(remaddr));
	remaddr.sin_family = AF_INET;
	remaddr.sin_port = htons(SERVICE_PORT);
	if (inet_aton(server, &remaddr.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}
	
	tcpheader * t = malloc(20);
	t->th_sport = myaddr.sin_port;
	t->th_dport = remaddr.sin_port;
	t->th_flags = 0;
        t->th_sum = 0;
        t->th_urp = 0;


// send the file name over
	if(sendto(fd, file_name, strlen(file_name), 0, (struct sockaddr *)&remaddr, slen) < 0) {
		perror("sendto");
		exit(1);
	}
	
	// receive ack for file name packet	
	recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
        if (recvlen >= 0) {
 	       buf[recvlen] = 0;	/* expect a printable string - terminate it */
               printf("received message: \"%s\"\n", buf);
        }
	recvlen = 0;
	// now we should receive the file's contents
	for(;;) {
	        char* to_send = malloc(20);
		if (recvlen != 0 && recvlen < BUFLEN) break;
		printf("about to attempt recvfrom\n");
		recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
		printf("recvlen: %d\n", recvlen);
		if (recvlen > 0) {
 	       		buf[recvlen] = 0;	/* expect a printable string - terminate it */
               		printf("received message: \"%s\"\n", buf+20);
			
			// send back ACKs
                        t->th_ack = msgcnt;
			t->th_seq = msgcnt++;
			sprintf(to_send, "%d%d%d%d%c%c%c%d%d%d", t->th_sport, t->th_dport, t->th_seq, t->th_ack, t->th_x2, t->th_off, t->th_flags, t->th_win, t->th_sum, t->th_urp);
			printf("See this string: %s msgcnt: %d\n", to_send, msgcnt);
			if(sendto(fd, to_send, 20, 0, (struct sockaddr *)&remaddr, slen) < 0) 
				perror("sendto");
			
		}
		else break;
	}
	
	/* now let's send the messages */
/*
	for (i=0; i < MSGS; i++) {
		printf("Sending packet %d to %s port %d\n", i, server, SERVICE_PORT);
		sprintf(buf, "This is packet %d", i);
		if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, slen) < 0) {
			perror("sendto");
			exit(1);
		}
		recvlen = recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&remaddr, &slen);
                if (recvlen >= 0) {
                        buf[recvlen] = 0;	
                        printf("received message: \"%s\"\n", buf);
                }
	}
*/
	close(fd);
	return 0;
}
