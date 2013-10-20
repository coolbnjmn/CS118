/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <sys/wait.h>	/* for the waitpid() system call */
#include <signal.h>	/* signal name macros, and the kill() prototype */
#include "string.h"
#include "time.h"

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

char* send_404_response();
char* send_400_response();
char* send_200_response();
void dostuff(int); /* function prototype */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno, pid;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;
     struct sigaction sa;          // for signal SIGCHLD

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     
     listen(sockfd,5);
     
     clilen = sizeof(cli_addr);
     
     /****** Kill Zombie Processes ******/
     sa.sa_handler = sigchld_handler; // reap all dead processes
     sigemptyset(&sa.sa_mask);
     sa.sa_flags = SA_RESTART;
     if (sigaction(SIGCHLD, &sa, NULL) == -1) {
         perror("sigaction");
         exit(1);
     }
     /*********************************/
     
     while (1) {
         newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
         
         if (newsockfd < 0) 
             error("ERROR on accept");
         
         pid = fork(); //create a new process
         if (pid < 0)
             error("ERROR on fork");
         
         if (pid == 0)  { // fork() returns a value of 0 to the child process
             close(sockfd);
             dostuff(newsockfd);
             exit(0);
         }
         else //returns the process ID of the child process to the parent
             close(newsockfd); // parent doesn't need this 
     } /* end of while */
     return 0; /* we never get here */
}

/******** DOSTUFF() *********************
 There is a separate instance of this function 
 for each connection.  It handles all communication
 once a connnection has been established.
 *****************************************/
void dostuff (int sock)
{
   int n;
   char buffer[256];
   char buffer2[256];
   char *req;
  
   bzero(buffer,256);
   n = read(sock,buffer,255);

   if (n < 0) error("ERROR reading from socket");
   printf("Here is the message: %s\n",buffer);

   strcpy(buffer2, buffer);
   req = strtok(buffer2, "\n");
   
   char *reqCopy = malloc(strlen(req));
   strcpy(reqCopy, req);

   if(validate_request(req))
   {
	printf("Success\n");
	char* response = send_200_response(reqCopy);
   	n = write(sock, response, strlen(response));
	printf("%s", response);
   	if (n < 0) error("ERROR writing to socket");
   }
   else 
   {
	char* response = send_400_response();
	printf("%s", response);
	n = write(sock, response, strlen(response));
   }
}

/*
* Should be bool for c++
*/
int validate_request(char * request)
{
	int i = 0;
	char* token;

	token = strtok(request, " ");
	printf("%s\n", token);
	if(strcmp(token, "GET"))
	{
		return 0;		
	}

	token = strtok(NULL, " ");
        printf("%s\n", token);

	token = strtok(NULL, " ");
	printf("%s\n", token);
	if(strncmp(token, "HTTP/1.1", 8) && strncmp(token, "HTTP/1.0", 8) )
	{
		return 0;
	}
	
	return 1;
}

char* send_400_response()
{
	char buf[30];
	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
	char* response;
	response = malloc(256);
	FILE *fp;
	fp = fopen("400response.html", "r");
	fseek(fp, 0L, SEEK_END);
	int f_size = ftell(fp);
	rewind(fp);
	char *str;
	str = malloc(f_size+1);
	size_t read_size = fread(str,1,f_size,fp);
	str[read_size] = 0;
	fclose(fp);
	printf("%s\n", str);
	sprintf(response, "HTTP/1.1 400 Bad Request\r\nDate: %s\r\nServer: Ajan and Benjamin's Server/1.0\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s\r\n", buf,f_size,str);
	return response;	
}
char* send_200_response(char* request)
{
	printf("inside 200 response\n");
	printf("request: %s\n", request);
	char* response;
	char* token;
	response = malloc(256);

	token = strtok(request, " ");
	token = strtok(NULL, " ");
        printf("%s\n", token);
	FILE *fp;
	fp = fopen(token+1, "r");
	if(!fp) {
//	  response = send_404_response();
	} else {
	  fseek(fp, 0L, SEEK_END);
	  int f_size = ftell(fp);
	  rewind(fp);
	  char *str;
	  str = malloc(f_size+1);
      	  size_t read_size = fread(str,1,f_size,fp);
	  str[read_size] = 0;
	  fclose(fp);
	  printf("%s\n", str);
	  char buf[30];
	  time_t now = time(0);
	  struct tm tm = *gmtime(&now);
	  strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
	  sprintf(response, "HTTP/1.1 200 OK\r\nDate: %s\r\nServer: Ajan and Benjamin's Server/1.0\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s\r\n", buf,f_size,str);
	}
	return response;	
}
