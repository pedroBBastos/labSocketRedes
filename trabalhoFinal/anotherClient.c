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

int max(int a, int b) {
    return a > b ? a : b;
}

int initiateUDPSocket(struct sockaddr_in* udpaddr) {
    int udpSockFileDescriptor;

    if ((udpSockFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("udp socket");
        exit(1);
    }

    bzero(udpaddr, sizeof(*udpaddr));
    udpaddr->sin_family = AF_INET;
    udpaddr->sin_addr.s_addr = htonl(INADDR_ANY);
    udpaddr->sin_port = htons(0);

    if (bind(udpSockFileDescriptor, (struct sockaddr *)udpaddr, sizeof(*udpaddr)) == -1) {
        perror("udp bind");
        exit(1);
    }

    return udpSockFileDescriptor;
}

unsigned int retrieveCurrentUDPPort(int udpSockFileDescriptor) {
    struct sockaddr_in local_addr;

    bzero(&local_addr, sizeof(local_addr));
    socklen_t len = sizeof(local_addr);

    getsockname(udpSockFileDescriptor, (struct sockaddr*) &local_addr, &len);
    return ntohs(local_addr.sin_port);
}

/*****************************************************************************
 * method to connect to server                                               *
 *****************************************************************************/

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

/*****************************************************************************
 * method to send message to server                                          *
 *****************************************************************************/

void sendMessageToServer(int socket_file_descriptor) {
    char text_to_server[500];
    bzero(&text_to_server, sizeof(text_to_server));

    fgets(text_to_server, 500, stdin);
    text_to_server[strcspn(text_to_server, "\n")] = 0;

    //Envia mensagem para o servidor
    if(send(socket_file_descriptor, text_to_server, 500, 0) < 0) {
        puts("Send failed");
        exit(1);
    }
}

/*****************************************************************************
 * method to read message from server                                        *
 *****************************************************************************/

void readMessageFromServer(int socket_file_descriptor, unsigned int udpPort) {
    char recvline[MAXLINE + 1];
    bzero(&recvline, sizeof(recvline));

    printf("Got message from server\n");

    if (read(socket_file_descriptor, recvline, MAXLINE + 1) == 0) {
        perror("Server terminated prematurely!!");
        exit(1);
    } else {
        printf("The message -> %s\n", recvline);
        if (strncmp(recvline, "give_me_your_udp_port", 21) == 0) {
            printf("Entered if to send UDP port\n");
            // char   buf[MAXLINE + 1];
            // strcpy(buf, "my_udp_port_is ");
            // char myUdpPortString[sizeof(unsigned int)*8+1];
            // snprintf(myUdpPortString, sizeof(unsigned int)*8+1, "%u", udpPort);
            // strcat(buf, myUdpPortString);



            char text_to_server[500];
            bzero(&text_to_server, sizeof(text_to_server));

            strcpy(text_to_server, "my_udp_port_is ");
            char myUdpPortString[sizeof(unsigned int)*8+1];
            snprintf(myUdpPortString, sizeof(unsigned int)*8+1, "%u", udpPort);
            strcat(text_to_server, myUdpPortString);

            //Envia mensagem para o servidor
            if(send(socket_file_descriptor, text_to_server, 500, 0) < 0) {
                puts("Send failed");
                exit(1);
            }



            // int n = write(socket_file_descriptor, buf, strlen(buf));
            // printf("n from write %d\n", n);
        } else {
            printf("%s", recvline);
        }
    }
}

/*****************************************************************************
 * main method                                                               *
 *****************************************************************************/

int main(int argc, char **argv) {
    int socket_file_descriptor, maxfdp;
    struct sockaddr_in udpaddr;
    int udpSockFileDescriptor;
    unsigned int currentUdpPort;

    checkProgramInput(argc, argv);

    socket_file_descriptor = conectToServer(argv[1], argv[2]);
    udpSockFileDescriptor = initiateUDPSocket(&udpaddr);
    currentUdpPort = retrieveCurrentUDPPort(udpSockFileDescriptor);

    fd_set rset;
    FD_ZERO(&rset);

    for ( ; ; ) {
        FD_SET(socket_file_descriptor, &rset);
        FD_SET(STDIN_FILENO, &rset);
        maxfdp = max(STDIN_FILENO, socket_file_descriptor) + 1;
        select(maxfdp, &rset, NULL, NULL, NULL);

        if (FD_ISSET(socket_file_descriptor, &rset)) {
            readMessageFromServer(socket_file_descriptor, currentUdpPort);
        }

        if (FD_ISSET(STDIN_FILENO, &rset)) {
            sendMessageToServer(socket_file_descriptor);
        }
    }
    
    exit(0);
}
