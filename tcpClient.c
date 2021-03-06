#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "myfunction.h"

#define MAX_BUF_SIZE 1024 // Maximum size of UDP messages

int main(int argc, char *argv[]){
  struct sockaddr_in server_addr; // struct containing server address information
  struct sockaddr_in client_addr; // struct containing client address information
  int sfd; // Server socket filed descriptor
  int cr; // Connect result
  int stop = 0;
  ssize_t byteRecv; // Number of bytes received
  ssize_t byteSent; // Number of bytes sent
  size_t msgLen; 
  socklen_t serv_size;
  char receivedData [MAX_BUF_SIZE]; // Data to be received
  char sendData [MAX_BUF_SIZE]; // Data to be sent 
  
  if (argc != 3) {
		printf("\nErrore numero errato di parametri\n");
		printf("\n%s <server IP (dotted notation)> <server port>\n", argv[0]);
		exit(1);
  }
  
  sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  
  if (sfd < 0){
  	perror("socket"); // Print error message
  	exit(EXIT_FAILURE);
  }
  
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[2]));
  server_addr.sin_addr.s_addr = inet_addr(argv[1]);
  
  serv_size = sizeof(server_addr);

  cr = connect(sfd, (struct sockaddr *) &server_addr, sizeof(server_addr));

  if (cr < 0){
    perror("connect"); // Print error message
    exit(EXIT_FAILURE);
  }

  
  while(!stop){
  	printf("Insert message:\n");
  	scanf("%s", sendData);
  	printf("String going to be sent to server: %s\n", sendData);
  	
  	if(strcmp(sendData, "exit") == 0){
  		stop = 1;
  	}
  	strcat(sendData, "\0");
  	msgLen = countStrLen(sendData);
  
  	byteSent = send(sfd, sendData, msgLen, 0);
  	printf("Bytes sent to server: %zd\n", byteSent);
  	 
  	if(!stop){
  		byteRecv = recv(sfd, receivedData, MAX_BUF_SIZE, 0);
  		perror("Received from server: ");
  		printData(receivedData, byteRecv);
  	}	
  }
  
  return 0;
}
