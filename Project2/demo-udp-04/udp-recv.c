/*
        demo-udp-03: udp-recv: a simple udp server
	receive udp messages

        usage:  udp-recv

        Paul Krzyzanowski
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "port.h"
#include "protocol-header.h"
#include <signal.h>

#define BUFSIZE 2048

void alarm_catcher (int ignored);
int check_to_send(int *window, int window_size);	

int
main(int argc, char **argv)
{
	struct sockaddr_in myaddr;	/* our address */
	struct sockaddr_in remaddr;	/* remote address */
	socklen_t addrlen = sizeof(remaddr);		/* length of addresses */
	int recvlen;			/* # bytes received */
	int fd;				/* our socket */
	int msgcnt = 0;			/* count # of messages we received */
	unsigned char buf[BUFSIZE];	/* receive buffer */


	if (argc < 3) {
		printf("Need port num and window size\n");
		exit(1);
	}
	int portno = atoi(argv[1]);
	int window_size = atoi(argv[2]);
	//double prob_loss = atof(argv[3]);
	//double prof_corr = atof(argv[4]);
	int con_win[window_size];
	bzero(con_win,window_size);
	/* create a UDP socket */

	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("cannot create socket\n");
		return 0;
	}

	struct sigaction alarm_handler;
	alarm_handler.sa_handler = alarm_catcher;
	alarm_handler.sa_flags = 0;
	if (sigaction(SIGALRM, &alarm_handler, 0) < 0) {
		perror("sigaction failed for SIGALRM\n");
	}
	/* bind the socket to any valid IP address and a specific port */



	memset((char *)&myaddr, 0, sizeof(myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	myaddr.sin_port = htons(portno);

	if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind failed");
		return 0;
	}
	
	// first receive should be the file name
	recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
	if (recvlen > 0) {
		buf[recvlen] = 0;
		FILE * file_p = fopen(buf, "rb");
		// try to open the file of filename buf
		printf("received message: \"%s\" (%d bytes)\n", buf, recvlen);
		
		// send back ack for file name reception
		tcpheader *ackT = malloc(20);
		ackT->th_sport = ntohs(myaddr.sin_port);
		ackT->th_dport = ntohs(remaddr.sin_port);	
		ackT->th_flags = 0;
		ackT->th_sum = 0;
		ackT->th_urp = 0;
		ackT->th_ack = 0;
		ackT->th_seq = 0;
		sprintf(buf, "%d %d %d %d %c %c %c %d %d %d", ackT->th_sport, ackT->th_dport, ackT->th_seq, ackT->th_ack, ackT->th_x2, ackT->th_off, ackT->th_flags, ackT->th_win, ackT->th_sum, ackT->th_urp);
		if (sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&remaddr, addrlen) < 0)
			perror("sendto");

		if (file_p == NULL) {
			// no file, do nothing
			printf("file wasn't opened\n");
		} else {
			fseek(file_p, 0L, SEEK_END);
			int f_size = ftell(file_p);
			rewind(file_p);
			char* str = malloc(f_size+1);
			size_t read_size = fread(str,1,f_size,file_p);
			str[read_size] = 0;
			fclose(file_p);
			long int count_written = 0;
			int bytes_to_write = 1024;	
			int n = 0;
		// now send the packets?
			tcpheader *t = malloc(20);
			t->th_sport = myaddr.sin_port;
			t->th_dport = remaddr.sin_port;	
			t->th_flags = 0;
			t->th_sum = 0;
			t->th_urp = 0;

			char* to_send = malloc(1024);
			
			while (count_written < f_size) {
			    int count = check_to_send(con_win, window_size);
			
		   	    int i = 0;
			    for(i = 0; i < count; i++) {
				printf("sending packet\n");
				t->th_seq = msgcnt++;
				t->th_ack = msgcnt; // not sure
				con_win[(msgcnt-1)%window_size] = msgcnt;
				int k = 0;
				for(k = 0; k < window_size; k++) {
					printf("%d - ",con_win[k]);
				} printf("\n");
				t->th_win = window_size; // need to accept user input for this
				sprintf(to_send, "%d %d %d %d %c %c %c %d %d %d", t->th_sport, t->th_dport, t->th_seq, t->th_ack, t->th_x2, t->th_off, t->th_flags, t->th_win, t->th_sum, t->th_urp);
				memcpy(to_send+20, str+count_written, 1004);
				n = sendto(fd, to_send, bytes_to_write,0, (struct sockaddr *)&remaddr, addrlen);
				if(n < 0) {
					perror("sendto");
				}
				count_written += (n-20);
				if(f_size - count_written < bytes_to_write) {
					bytes_to_write = f_size - count_written + 20;
				}
				
			   }
			   int j = 0;
			   for(j = 0; j < count; j++){
			   		printf("waiting on port %d\n", SERVICE_PORT);
					recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
					if (recvlen > 0) {
						buf[recvlen] = 0;
						printf("received message: \"%s\" (%d bytes)\n", buf, recvlen);
					}
					else
						printf("uh oh - something went wrong!\n");
					int th_sport, th_dport, th_seq, th_ack;
					sscanf(buf, "%d%d%d%d", &th_sport, &th_dport, &th_seq, &th_ack);
						
					printf("th_ack: %d\n", th_ack);
					
					con_win[th_ack%window_size] = 0;
					int k = 0;
					for(k = 0; k < window_size; k++) {
						printf("%d -",con_win[k]);
					} printf("\n");
					//sprintf(buf, "ack %d", msgcnt++);
				//	printf("sending response \"%s\"\n", buf);
				//	if (sendto(fd, buf, 20, 0, (struct sockaddr *)&remaddr, addrlen) < 0)
				//		perror("sendto");
			   	}
			}
		}			
	}

}


int check_to_send(int *window, int window_size) {	
	int i;
	printf("check to send being called\n");
	for(i = 0; i < window_size; i++) {
		if(window[i] != 0) break;
	}
	return i;
}
void alarm_catcher (int ignored) {
	printf("alarm_cathcer called\n");
	exit(1);
}

static int
makeTimer( char *name, timer_t *timerID, int expireMS, int intervalMS )
{
    struct sigevent         te;
    struct itimerspec       its;
    struct sigaction        sa;
    int                     sigNo = SIGRTMIN;

    /* Set up signal handler. */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timerHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sigNo, &sa, NULL) == -1)
    {
        fprintf(stderr, "%s: Failed to setup signal handling for %s.\n", PROG, name);
        return(-1);
    }

    /* Set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = sigNo;
    te.sigev_value.sival_ptr = timerID;
    timer_create(CLOCK_REALTIME, &te, timerID);

    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = intervalMS * 1000000;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = expireMS * 1000000;
    timer_settime(*timerID, 0, &its, NULL);

    return(0);
}

static void
timerHandler( int sig, siginfo_t *si, void *uc )
{
    timer_t *tidp;
    tidp = si->si_value.sival_ptr;

    if ( *tidp == firstTimerID )
        firstCB(sig, si, uc);
    else if ( *tidp == secondTimerID )
        secondCB(sig, si, uc);
    else if ( *tidp == thirdTimerID )
        thirdCB(sig, si, uc);
}
