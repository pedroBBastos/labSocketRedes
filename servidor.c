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
#define MAXDATASIZE 100

#define MAXLINE 4096

void printNewClientInformation(int connfd, struct sockaddr_in peeraddr) {
    char   buf[MAXDATASIZE];
    time_t ticks;

    ticks = time(NULL);
    printf("New client received at %.24s!!!\r\n", ctime(&ticks));

    char local_socket_ip[16];
    inet_ntop(AF_INET, &peeraddr.sin_addr, local_socket_ip, sizeof(local_socket_ip));
    printf("	client IP address: %s\n", local_socket_ip);
    unsigned int local_socket_port = ntohs(peeraddr.sin_port);
    printf("	client local port: %u\n", local_socket_port);

    ticks = time(NULL);
    snprintf(buf, sizeof(buf), "Hello from server!\nTime: %.24s\r\n", ctime(&ticks));
    write(connfd, buf, strlen(buf));
}

int main (int argc, char **argv) {
    int    listenfd, connfd;
    struct sockaddr_in servaddr;
    // char   buf[MAXDATASIZE];
    // time_t ticks;
    char received_text[500];

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

	/*
        char local_socket_ip[16];
        inet_ntop(AF_INET, &peeraddr.sin_addr, local_socket_ip, sizeof(local_socket_ip));
        printf("Server accepted client connection with IP address: %s\n", local_socket_ip);
        unsigned int local_socket_port = ntohs(peeraddr.sin_port);
        printf("Server accepted client connection with with port: %u\n", local_socket_port);


        ticks = time(NULL);
        snprintf(buf, sizeof(buf), "Hello from server!\nTime: %.24s\r\n", ctime(&ticks));
        write(connfd, buf, strlen(buf));
	*/

	printNewClientInformation(connfd, peeraddr);


        bzero(&received_text, sizeof(received_text));
        read(connfd, received_text, 500);
        printf("Mensagem recebida do client: %s \n", received_text);

	pid_t pid = fork();
	printf("[test] pid -> %d \n", pid);
	if (pid == 0){
            close(listenfd);
	    printf("[test] Inside thread (pid): %d \n", pid);
            close(connfd);
            exit(0);
        }

        // sleep(60); - questão 2

        close(connfd);
    }
    return(0);
}
