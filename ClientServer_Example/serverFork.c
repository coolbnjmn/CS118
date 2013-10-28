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

void dostuff(int);
void analyze_request(int, char*);
int  validate_request(char *);
void send_valid_response(int, char*);
void send_invalid_response(int);
void send_image_response(FILE *, int);
void send_html_response(FILE*, int);
void send_404_response(int);

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

/* function prototype */
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

   	if (n < 0) 
		error("ERROR reading from socket");
   	printf("Here is the message: %s\n",buffer); // Answer for part A

   	strcpy(buffer2, buffer);
   	req = strtok(buffer2, "\n");

	analyze_request(sock, req);
}

/******** analyze_request() *****************************
 function takes two arguements, sock and  request
 sock is a socket descriptor describing the the port
 with which we try to connect.
 request is a string describing the http request received
 from the client
 analyze_request makes a copy of the request and checks if 
 it is valid by calling two other functions.
 ********************************************************/
void
analyze_request(int   sock, 
				char* request)
{
   	char* reqCopy = malloc(strlen(request));
   	strcpy(reqCopy, request);
   	if(validate_request(request))
   	{
		send_valid_response(sock, reqCopy);
   	}
   	else 
   	{
		send_invalid_response(sock);
   	}
}

/******** validate_request() *****************************
 function takes two arguements, sock and  request
 request is a string describing the http request received
 from the client
 validate_request splits the request into a set of tokens.
 Each token corresponds to a header field and is checked
 for validity.
 0 is returned if the request is invalid
 1 is returned if each header field is valid
 ********************************************************/
int 
validate_request(char *request)
{
	char* token;

	token = strtok(request, " ");
	if(strcmp(token, "GET"))
	{
		return 0;		
	}

	token = strtok(NULL, " ");
	token = strtok(NULL, " ");

	if(strncmp(token, "HTTP/1.1", 8) && strncmp(token, "HTTP/1.0", 8)) {
		return 0;
	}
	
	return 1;
}

/******** send_valid_response() *****************************
 function takes two arguements, sock and  request
 sock is a socket descriptor describing the the port
 request is a string describing the http request received
 from the client
 send_valid_response first checks if the file mentioned in 
 the http request exists in the server directory. If it 
 doesn't, a 404 response is sent. If it does, the file extension 
 is parsed. If it is an acceptable format, either an image is to or a
 page depending on the extension.
 Otherwise a 404 response is sent.
 ********************************************************/
void
send_valid_response(int   sock,
			  		char* request)
{
	int n;
	
	char* response;
	char* file_name;
	char* ext;
	FILE *fp;
	char c;
	char* ext_loc;
	int pos;

	response = malloc(256);			
	file_name = strtok(request, " ");
	file_name = strtok(NULL, " ");
	fp = fopen(file_name+1, "rb");
	if(fp == NULL){
		send_invalid_response(sock);
	}
	else {
		c = '.';
		ext_loc = strchr(file_name, c);
		pos = ext_loc - file_name;
		ext = malloc(strlen(file_name) - pos + 1);
		memcpy(ext, &file_name[pos + 1], strlen(file_name) - pos);
		ext[strlen(file_name) - pos + 1] = '\0';
		int i;		
		for(i = 0; i < strlen(ext); i++)
			ext[i] = tolower(ext[i]); 
		if(strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0 || strcmp(ext, "gif") == 0 || strcmp(ext, "jpeg") == 0 || strcmp(ext, "jfif") == 0 || strcmp(ext, "png") == 0)
			send_image_response(fp, sock);
		else if(strcmp(ext, "html") == 0 || strcmp(ext, "txt") == 0)
			send_html_response(fp, sock);
		else
			send_invalid_response(sock);
	}
   	n = write(sock, response, strlen(response));
   	if (n < 0) 
		error("ERROR writing to socket");
}

/******** send_invalid_response() *****************************
 function takes two arguements, sock and  request
 sock is a socket descriptor describing the the port
 send_invalid_response sends a http 400 response and writes an 
 html page 400response.html decribing the nature of the error to 
 the client. It finds the current time, converts it into a RFC 
 defined format and finds the length of the file. Both of these 
 fields are input into a string which is written out. An html file 
 is also written out.
 ********************************************************/
void
send_invalid_response(int sock)
{
	char buf[30];
	char* response;
	char *str;
	int f_size;
	size_t read_size;
	FILE *fp;

	response = malloc(256);

	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
	fp = fopen("400response.html", "r");
	fseek(fp, 0L, SEEK_END);
	f_size = ftell(fp);
	rewind(fp);
	str = malloc(f_size+1);
	read_size = fread(str,1,f_size,fp);
	str[read_size] = 0;
	fclose(fp);
	printf("%s\n", str);
	sprintf(response, "HTTP/1.1 400 Bad Request\r\nDate: %s\r\nServer: Ajan and Benjamin's Server/1.0\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s\r\n", buf,f_size,str);
	write(sock, response, strlen(response));
}

/******** send_image_response() *****************************
 function takes two arguements, sock and  request
 fp is a file descriptor to an opened image file
 sock is a socket descriptor describing the the port
 send_image_response sends a http 200 response and writes the 
 image described by the file descriptor.
 ********************************************************/
void 
send_image_response(FILE *fp, 
					int   sock)
{
	char* response;
	long f_size;
	unsigned char *str;
	size_t read_size;
	char buf[30];
	size_t n;
	long count_written;
	size_t  bytes_to_write;
	printf("inside image response\n");

	count_written = 0;
	bytes_to_write = 65536;

	response = malloc(256);			
	fseek(fp, 0L, SEEK_END);
	f_size = ftell(fp);
	printf("f_size: %ld\n", f_size);
	rewind(fp);
	str = malloc(f_size+1);
	read_size = fread(str,1,f_size,fp);
	printf("Read size: %d\n", read_size);
	str[read_size] = 0;
	fclose(fp);
	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
	sprintf(response, "HTTP/1.1 200 OK\r\nDate: %s\r\nServer: Ajan and Benjamin's Server/1.0\r\nContent-Type: image/jpeg\r\nContent-Length: %ld\r\n\r\n", buf,f_size);
	n = write(sock, response, strlen(response));
	printf("Bytes written: %d\n", n);
	// try to write 64kb at a time
	if (f_size < bytes_to_write) {
	        bytes_to_write = f_size;
	  }

	int count = 0;
	while (count_written <= f_size) {
		printf("Count-written before: %ld\n", count_written);
	  	n = write(sock, str+count_written, bytes_to_write); 
	  	printf("Second bytes written: %d\n", n);
		count_written += bytes_to_write;
		printf("Count-written after: %ld fsize:%ld\n", count_written, f_size);
		if(f_size - count_written < bytes_to_write)
			bytes_to_write = f_size - count_written;
		if(bytes_to_write == 0) count++;
		if (count > 1024) break;
	  }		   
}

/******** send_html_response() *****************************
 function takes two arguements, sock and  request
 fp is a file descriptor to an opened image file
 sock is a socket descriptor describing the the port
 send_html_response sends a http 200 response and writes the 
 html described by the file descriptor. 
 ********************************************************/
void send_html_response(FILE* fp, 
						int   sock)
{
	char* response;
	char *str;
	char buf[30];
	int f_size;
	size_t read_size;
	size_t n;
	long count_written;
	size_t  bytes_to_write;

	response = malloc(256);
	count_written = 0;
	bytes_to_write = 65536;

	fseek(fp, 0L, SEEK_END);
	f_size = ftell(fp);
	rewind(fp);

	str = malloc(f_size+1);
	read_size = fread(str,1,f_size,fp);
	printf("Read size: %d\n", read_size);
	str[read_size] = 0;
	fclose(fp);

	time_t now = time(0);
	struct tm tm = *gmtime(&now);
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
	sprintf(response, "HTTP/1.1 200 OK\r\nDate: %s\r\nServer: Ajan and Benjamin's Server/1.0\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", buf,f_size);
	n = write(sock, response, strlen(response));
	printf("Bytes written: %d\n", n);
	  // try to write 64kb at a time
	if (f_size < bytes_to_write) {
	        bytes_to_write = f_size;
	}

	int count = 0;
	while (count_written <= f_size) {
		printf("Count-written before: %ld\n", count_written);
	  	n = write(sock, str+count_written, bytes_to_write); 
	  	printf("Second bytes written: %d\n", n);
		count_written += n;
		printf("Count-written after: %ld\n", count_written);
		if(f_size-count_written < bytes_to_write)
			bytes_to_write = f_size - count_written;

		if(bytes_to_write == 0) count++;
		if(count > 1024) break;
	}	
}

/******** send_404_response() *****************************
 function takes two arguements, sock and  request
 sock is a socket descriptor describing the the port
 send_404_response sends a http 404 response and writes the 
 html 404.html to the client. 
 ********************************************************/
void send_404_response(int sock)
{
	char buf[30];
        time_t now = time(0);
        struct tm tm = *gmtime(&now);
        strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &tm);
        char* response;
        response = malloc(1024);
	FILE *fp;
        fp = fopen("404.html", "r");
        fseek(fp, 0L, SEEK_END);
        int f_size = ftell(fp);
        rewind(fp);
        char *str;
        str = malloc(f_size+1);
        size_t read_size = fread(str,1,f_size,fp);
        str[read_size] = 0;
        fclose(fp);
        sprintf(response, "HTTP/1.1 404 Not Found\r\nDate: %s\r\nServer: Ajan and Benjamin's Server/1.0\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s\r\n", buf,f_size, str);
	printf("%s", response);
        write(sock, response, strlen(response));
}
