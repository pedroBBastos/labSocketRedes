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

void checkProgramInput(int argc, char **argv) {
    char error[MAXLINE + 1];
    if (argc != 2) {
        strcpy(error,"uso: ");
        strcat(error,argv[0]);
        strcat(error," <Port>");
        perror(error);
        exit(1);
    }
}

int initiateServer(char *port) {
    int    listenfd;
    struct sockaddr_in servaddr;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(port));

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen");
        exit(1);
    }

    return listenfd;
}

void printNewClientInformation(int connfd, struct sockaddr_in peeraddr) {
    time_t ticks;

    ticks = time(NULL);
    printf("New client received at %.24s!!!\r\n", ctime(&ticks));

    char local_socket_ip[16];
    inet_ntop(AF_INET, &peeraddr.sin_addr, local_socket_ip, sizeof(local_socket_ip));
    printf("	client IP address: %s\n", local_socket_ip);
    unsigned int local_socket_port = ntohs(peeraddr.sin_port);
    printf("	client local port: %u\n", local_socket_port);
}

void writeClienteResponseIntoFile(char received_text[500]) {
    FILE *out_file = fopen("server-output", "a");
    if (out_file == NULL) {
        printf("Error! Could not open file\n");
        exit(-1);
    }
    fprintf(out_file, "----------------------------\n");
    strcat(received_text, "\n");
    fprintf(out_file, received_text);
    fprintf(out_file, "----------------------------\n");
}

void handleClient(int connfd) {
    char received_text[500];
    int n;

    char commands[4][40];
    strcpy(commands[0], "ls\0"); 
    strcpy(commands[1], "pwd\0");
    strcpy(commands[2], "ls -l\0");
    strcpy(commands[3], "exit\0");

    for (int i=0; i < 4; i++) {
	    printf("Entered server loop for nex client command to be executed\n");
        printf("Command to be sent: %s\n", commands[i]);
        write(connfd, commands[i], strlen(commands[i]));

	    // bzero(&received_text, sizeof(received_text));
	    // if( (n = read(connfd, received_text, 500)) > 0) {
        //     printf("Comand received from client (%d): %s \n", connfd, received_text);
        //     writeClienteResponseIntoFile(received_text);            
        // } else {
        //     printf("Read error! Finishing client handling \n");
        //     break;
        // }
    }

    printf("All commands available sent. Finishing client handling. \n");
}

void startListenToConnections(int listenfd) {
    int connfd;

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
}

int main (int argc, char **argv) {
    int listenfd;

    checkProgramInput(argc, argv);
    listenfd = initiateServer(argv[1]);

    startListenToConnections(listenfd);

    return(0);
}
