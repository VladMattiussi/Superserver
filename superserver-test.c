#include<errno.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/time.h>
#include<sys/wait.h>
#include<signal.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

//Constants and global variable declaration goes here
#define ALLSTD 3
#define BACK_LOG 5 // Maximum queued requests
#define MAX(a,b) (((a)>(b))?(a):(b))

//Service structure definition goes here
typedef struct SERVER {
	char protocol[4];			/*'tcp' if protocol is TCP, ’udp’ if protocol is UDP*/
	char service_mode[10];		/* ’wait’ if service is sequential, ‘nowait’ if service is concurrent */
	uint16_t service_port;		/* port on which service is available */
	char service_path_name[50]; /* complete name of the service, including full path */
	char service_name[30];		/* name of the service */
	int sfd;					/* socket descriptor associated to the port */
	pid_t PID;					/* PID of the process; this is meaningful only for services of type ‘wait’ */
} server;

server *sv;
struct sockaddr_in server_addr, client_addr;		/* socket information: port, ip */	
fd_set readSet;	

int nsock;							/* count file descriptor to use */

//Function prototype devoted to handle the death of the son process
void handle_signal(int sig);

int clientSocket();

//prepare all socket in inetd.conf
int clientSocket(){
	sv = malloc(10*sizeof(server));
	char temp[10];
	int br; 	/* bind result operation */
	int	lr; 	/* listen result operation */
	int	ar; 	/* accept result operation */
	int i;

	FILE *fp = fopen("inetd.conf", "r");
	if(fp == NULL) { exit(EXIT_FAILURE); }

	for(i=0; !feof(fp); i++) { /* lettura di inted.conf */	
		fscanf(fp,"%s %s %hu %s", sv[i].service_name, sv[i].protocol, &sv[i].service_port, sv[i].service_mode);	
		strncpy(temp, sv[i].service_name, sizeof(temp));
		realpath(temp, sv[i].service_path_name);
		strncpy(sv[i].service_name, temp, sizeof(sv[i].service_name));

		/* SOCKET */
		if (strcmp(sv[i].protocol, "tcp") == 0) { /* TCP */	
			sv[i].sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		}
		else { /* UDP */ 
			sv[i].sfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		}
		if (sv[i].sfd < 0){	perror("socket"); exit(EXIT_FAILURE); }
		printf("check socket\n");
		
		/* BIND */	
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(sv[i].service_port);
		server_addr.sin_addr.s_addr = INADDR_ANY;		
		
		br = bind(sv[i].sfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
		if (br!=0) {perror("bind");exit(EXIT_FAILURE);}
		printf("check bind\n");

		/* LISTEN */
		if (strcmp(sv[i].protocol, "tcp") == 0) { /* TCP */		
			lr = listen(sv[i].sfd, BACK_LOG);
			if (lr!=0) {perror("listen");exit(EXIT_FAILURE);}
			printf("check listen\n");		
		}
	}
	fclose(fp);	/* chiude il file */

	return (i);
}

int  main(int argc,char **argv,char **env){ // NOTE: env is the variable to be passed, as last argument, to execle system-call
	// Other variables declaration goes here
	struct timeval tv;	/* time value before timeout */
	int dupSock;	/* socket duplication */
	int maxfd;	/* highest socket file descriptor */
	int	newSock;	/* new socket file descriptor for client communication - Accept result*/
	int	sr; 	/* select result operation */
	int	er;	/* execle result operation */
	socklen_t cli_size;			/* file descriptor set used on select operation*/
	
	// Server behavior implementation goes here
	nsock = clientSocket();
	printf("NFD %d\n",nsock);

	/* Handle signals sent by son processes - call this function when it's ought to be */
	signal(SIGCHLD,handle_signal);

	for (int c=0;;c++) {
		errno = 0;
		FD_ZERO(&readSet);	
		printf("For %d\n",c);
		maxfd = sv[0].sfd;
		for (int i = 0; i < nsock; i++){
			maxfd = MAX(maxfd, sv[i].sfd);
			printf("sv[%d] = %d\n",i, sv[i].sfd);
		}	
		printf("maxfd %d\n", maxfd);
		
		for (int i = 0; i < nsock; i++) {
			FD_SET(sv[i].sfd, &readSet); 						

			/* SELECT */ 
			tv.tv_sec = 1;
   			tv.tv_usec = 0;
			sr = select(maxfd+1, &readSet, NULL, NULL, &tv);
			if (sr < 0 && errno != EINTR) {perror("select");exit(EXIT_FAILURE);}
			else if (sr == 0 ) { printf("Timeout expired, no pending con nection on socket\n");}
			else {
				printf("sr %d errno %d\n",sr,errno);
				if (!FD_ISSET(sv[i].sfd, &readSet)) { perror("FDSET"); exit(EXIT_FAILURE);} /* client request */
				/* ACCEPT */
				if (strcmp(sv[i].protocol, "tcp") == 0){
					cli_size = sizeof(client_addr);
					newSock = accept(sv[i].sfd, (struct sockaddr *) &client_addr, &cli_size);
					if (newSock < 0) {perror("accept");exit(EXIT_FAILURE);}
					printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
				}
				printf("At least one connection pending on socket\n");

				/* Concurrent mode and Sequential mode */
				if ((sv[i].PID = fork()) == 0)  { /* CHILD PROCESS */
					printf("Child Process %u\n", getpid());

					/* Close stdin, stout, sterr */
					for (int j = 0; j < ALLSTD; close(j), j++);
					
					/* Close welcome socket */
					if (strcmp(sv[i].protocol, "tcp") == 0){
						close(sv[i].sfd);
					}

					/* DUP */
					for(int j = 0; j < ALLSTD; j++) {
						dupSock = dup((strcmp(sv[i].protocol, "tcp") == 0) ? newSock : sv[i].sfd);
						if (dupSock < 0) {perror("dup");exit(EXIT_FAILURE);}
					}
					
					/* EXECLE */
					er = execle(sv[i].service_path_name, sv[i].service_name, NULL, env);
					if (er < 0) {perror("execle");}

					close((strcmp(sv[i].protocol, "tcp") == 0) ? newSock : sv[i].sfd);

					exit(0);
				} else { /* FATHER PROCESS */ 
					if (strcmp(sv[i].protocol, "tcp") == 0){
						close(newSock);
					}					
		 			printf("Father Process %u\n", getpid());
					if (strcmp(sv[i].service_mode, "wait") == 0){
						sv[i].PID = getpid(); 
						FD_CLR(sv[i].sfd, &readSet);
					}		
				}
			}		
		}	
	}
	return 0;
}

// handle_signal implementation
void handle_signal(int sig){
	pid_t PID;
	printf("entro nella signal\n");
	// Call to wait system-call goes here	
	PID = wait(NULL);

	switch (sig) {
		case SIGCHLD : 
			// Implementation of SIGCHLD handling goes here	
			printf("WAIT restituisce PID = %u\n",PID);
			printf("nsock %d\n",nsock);

			for (int i = 0; (i<nsock); i++)
			{
				printf("sv[%d].PID = %u\n",i,sv[i].PID);
				if(strcmp(sv[i].service_mode, "wait") == 0 && sv[i].PID == PID){			
					FD_SET(sv[i].sfd, &readSet);
				}		
			}	
			break;
		default : printf ("Signal not known!\n");
			break;
	}
}
