#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <string.h>
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

void printLocalSocketInfo(int socket_file_descriptor) {
    char local_socket_ip[16];
    unsigned int local_socket_port;
    struct sockaddr_in local_addr;

    bzero(&local_addr, sizeof(local_addr));
    socklen_t len = sizeof(local_addr);
    
    //obtendo informações do socket local.
    //sock_file_descriptor foi criado previamente para se comunicar com o servidor
    getsockname(socket_file_descriptor, (struct sockaddr *) &local_addr, &len);

    // função inet_ntop -> para converter de endereço binário para formato texto
    inet_ntop(AF_INET, &local_addr.sin_addr, local_socket_ip, sizeof(local_socket_ip));

    // função ntohs -> conversão endereço de rede para host
    local_socket_port = ntohs(local_addr.sin_port);

    printf("Local socket IP Address: %s\n", local_socket_ip);
    printf("Local socket port: %u\n", local_socket_port);
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

    printf("Server IP Address: %s\n", address);
    printf("Server port: %s\n", port);
    printLocalSocketInfo(socket_file_descriptor);

    return socket_file_descriptor;
}

void handleCommandExecution(int socket_file_descriptor, char outputPart[MAXLINE]) {
    printf("%s", outputPart);
    //Envia mensagem para o servidor
    if(send(socket_file_descriptor, outputPart, 500, 0) < 0) {
        puts("Send failed");
        exit(1);
    }
}

void executeReceivedCommand(int socket_file_descriptor, char command[MAXLINE + 1]) {
    FILE *fp;
    int status;
    char path[MAXLINE];

    fp = popen(command, "r");
    if (fp == NULL) {
        printf("Error while trying to execute command!!\n");
    }

    while (fgets(path, MAXLINE, fp) != NULL) {
        handleCommandExecution(socket_file_descriptor, path);
    }
    //Envia mensagem para o servidor
    if(send(socket_file_descriptor, "end-command-execution", 500, 0) < 0) {
        puts("Send failed");
        exit(1);
    }

    status = pclose(fp);
    if (status == -1) {
        printf("Error while trying to close popen oppened stream!!\n");
    }
}

void readCommandsFromServer(int socket_file_descriptor) {
    int n;
    char recvline[MAXLINE + 1];

    for ( ; ; ) {
        bzero(&recvline, sizeof(recvline));
        if( (n = read(socket_file_descriptor, recvline, MAXLINE)) > 0) {
            printf("Command received from server: %s\n", recvline);

            if(strcmp(recvline, "exit") == 0) {
                printf("Finishing interaction with server\n");
                break;
            } else {
                executeReceivedCommand(socket_file_descriptor, recvline);
            }
        } else {
            perror("read error");
            exit(1);
        }
    }
}

void interactWithServer(int socket_file_descriptor) {
    char text_to_server[500];
    for ( ; ; ) {
        bzero(&text_to_server, sizeof(text_to_server));
        printf("Type your command: \n");

        fgets(text_to_server, 500, stdin);
        text_to_server[strcspn(text_to_server, "\n")] = 0;

        //Envia mensagem para o servidor
        if(send(socket_file_descriptor, text_to_server, 500, 0) < 0) {
            puts("Send failed");
            exit(1);
        }

        if(strcmp(text_to_server, "exit") == 0) {
            printf("Finishing interaction with server\n");
            break;
        }
    }
}

int main(int argc, char **argv) {
    int socket_file_descriptor;

    checkProgramInput(argc, argv);
    socket_file_descriptor = conectToServer(argv[1], argv[2]);

    readCommandsFromServer(socket_file_descriptor);
    
    exit(0);
}
