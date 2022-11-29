#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#define MAXLINE 4096

void checkProgramInput(int argc, char **argv) {
    char error[MAXLINE + 1];
    if (argc != 3) {
        strcpy(error,"uso: ");
        strcat(error,argv[0]);
        strcat(error," <Server-IPaddress> <Server-Port>");
        perror(error);
        exit(1);
    }
}

int conectToServer(char *address, char *port) {
    int socket_file_descriptor;
    struct sockaddr_in servaddr;
    if ( (socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(atoi(port));
    if (inet_pton(AF_INET, address, &servaddr.sin_addr) <= 0) {
        perror("inet_pton error");
        exit(1);
    }

    if (connect(socket_file_descriptor, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("connect error");
        exit(1);
    }

    return socket_file_descriptor;
}

void interactWithServer(int socket_file_descriptor) {
    char text_to_server[500];
    for ( ; ; ) {
        bzero(&text_to_server, sizeof(text_to_server));
        // printf("Type your command: \n");

        fgets(text_to_server, 500, stdin);
        text_to_server[strcspn(text_to_server, "\n")] = 0;

        //Envia mensagem para o servidor
        if(send(socket_file_descriptor, text_to_server, 500, 0) < 0) {
            puts("Send failed");
            exit(1);
        }

        if(strcmp(text_to_server, "exit") == 0) {
            // printf("Finishing interaction with server\n");
            break;
        }
    }
}

void readAvailableClientsToConnect(int socket_file_descriptor) {
    char recvline[MAXLINE + 1];

    read(socket_file_descriptor, recvline, MAXLINE);
    printf(recvline);

    interactWithServer(socket_file_descriptor);
}

int main(int argc, char **argv) {
    int socket_file_descriptor;

    checkProgramInput(argc, argv);
    socket_file_descriptor = conectToServer(argv[1], argv[2]);

    readAvailableClientsToConnect(socket_file_descriptor);
    // interactWithServer(socket_file_descriptor);
    
    exit(0);
}
