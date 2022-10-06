#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>

#define LISTENQ 10
#define MAXDATASIZE 500

#define MAXLINE 4096

void printNewClientInformation(int connfd, struct sockaddr_in peeraddr) {
    time_t ticks;
    char   buf[MAXDATASIZE];

    ticks = time(NULL);
    printf("New client received at %.24s!!!\r\n", ctime(&ticks));

    char local_socket_ip[16];
    inet_ntop(AF_INET, &peeraddr.sin_addr, local_socket_ip, sizeof(local_socket_ip));
    printf("	client IP address: %s\n", local_socket_ip);
    unsigned int local_socket_port = ntohs(peeraddr.sin_port);
    printf("	client local port: %u\n", local_socket_port);



    snprintf(buf, sizeof(buf), 
		    "Hello! These are the commands available:\n \
		    	atack\n \
			defend\n \
			hide\n \
			transform\n \
			exit \nChoose wisely..");
    write(connfd, buf, strlen(buf));
}

void handleClient(int connfd) {
    char received_text[500];
    int n;

    for ( ; ; ) {
	    printf("Entered server loop for client %d\n", connfd);
	    bzero(&received_text, sizeof(received_text));
	    if( (n = read(connfd, received_text, 500)) > 0) {
            printf("Comand received from client (%d): %s \n", connfd, received_text);
        } else {
            printf("Read error! Finishing client handling \n");
            break;
        }

	    if(strcmp(received_text, "exit") == 0) {
		    break;
	    }
    }
}

int main (int argc, char **argv) {
    int    listenfd, connfd;
    struct sockaddr_in servaddr;
    char   error[MAXLINE + 1];

    if (argc != 2) {
        strcpy(error,"uso: ");
        strcat(error,argv[0]);
        strcat(error," <Port>");
        perror(error);
        exit(1);
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(argv[1]));

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen");
        exit(1);
    }

    for ( ; ; ) {
        struct sockaddr_in peeraddr;
        socklen_t peerlen;
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept");
            exit(1);
        }

        getpeername(connfd , (struct sockaddr*) &peeraddr , (socklen_t*) &peerlen);

	    printNewClientInformation(connfd, peeraddr);

        pid_t pid = fork();
        printf("[test] pid -> %d \n", pid);
        if (pid == 0){
            close(listenfd);
            printf("[test] Inside thread (pid): %d \n", pid);
            handleClient(connfd);

            printf("Closing connection with client %d \n", connfd);
            close(connfd);
            exit(0);
        }

        close(connfd);
    }
    return(0);
}
