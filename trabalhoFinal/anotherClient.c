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

typedef struct {
    int inChatWithAnotherClient;
    int peerId;
    char peerIPAddress[16];
    unsigned int peerUDPPort;
    struct sockaddr_in peeraddr;
} ChatObject;

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

void getChatPeerInfo(char recvline[MAXLINE + 1], ChatObject* chatObject) {
    // chat_init_with_client <client_id> <client_ip_address> <client_udp_port>

    chatObject->inChatWithAnotherClient = 1;

    char delim[] = " ";
    char *ptr = strtok(recvline, delim);

    // getting peer_id
    ptr = strtok(NULL, delim);
    chatObject->peerId = strtol(ptr, 0L, 10);

    // getting peer_ip_address
    ptr = strtok(NULL, delim);
    strcpy(chatObject->peerIPAddress, ptr);

    // getting peer_udp_port
    ptr = strtok(NULL, delim);
    chatObject->peerUDPPort = strtoul(ptr, 0L, 10);

    printf("chatObject->peerId %d\n", chatObject->peerId);
    printf("chatObject->peerIPAddress %s\n", chatObject->peerIPAddress);
    printf("chatObject->peerUDPPort %u\n", chatObject->peerUDPPort);

    bzero(&chatObject->peeraddr, sizeof(chatObject->peeraddr));
    chatObject->peeraddr.sin_family = AF_INET;
    chatObject->peeraddr.sin_port = htons(chatObject->peerUDPPort);
    inet_pton(AF_INET, chatObject->peerIPAddress, &chatObject->peeraddr.sin_addr);
}

void sendMessageToAnotherClient(ChatObject* chatObject, int sockToSendMessagefd) {
    printf("To send message to another client\n");

    char text_to_peer[500];
    bzero(&text_to_peer, sizeof(text_to_peer));

    fgets(text_to_peer, 500, stdin);
    // text_to_peer[strcspn(text_to_peer, "\n")] = 0;

    int n = sendto(sockToSendMessagefd, text_to_peer, strlen(text_to_peer), 0, (struct sockaddr*) &chatObject->peeraddr, sizeof(chatObject->peeraddr));
    printf("n --> %d\n", n);
}

/*****************************************************************************
 * method to send message from stdin to server                               *
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

void replyCurrentUDPPortToServer(int socket_file_descriptor,
                                 unsigned int udpPort) {
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
}

/*****************************************************************************
 * method to read message from server                                        *
 *****************************************************************************/

void readMessageFromServer(int socket_file_descriptor, unsigned int udpPort,
                           ChatObject* chatObject) {
    char recvline[MAXLINE + 1];
    bzero(&recvline, sizeof(recvline));

    printf("Got message from server\n");

    if (read(socket_file_descriptor, recvline, MAXLINE + 1) == 0) {
        perror("Server terminated prematurely!!");
        exit(1);
    } else {
        if (strncmp(recvline, "give_me_your_udp_port", 21) == 0) {
            replyCurrentUDPPortToServer(socket_file_descriptor, udpPort);
        } else if (strncmp(recvline, "chat_init_with_client", 21) == 0) {
            printf("recvline -> %s\n", recvline);
            getChatPeerInfo(recvline, chatObject);
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

    ChatObject chatObject;
    int sockToSendMessagefd = socket(AF_INET, SOCK_DGRAM, 0);

    for ( ; ; ) {
        FD_SET(socket_file_descriptor, &rset);
        FD_SET(STDIN_FILENO, &rset);

        if (chatObject.inChatWithAnotherClient) {
            FD_SET(udpSockFileDescriptor, &rset);
            maxfdp = max(max(STDIN_FILENO, socket_file_descriptor), udpSockFileDescriptor) + 1;
        } else {
            maxfdp = max(STDIN_FILENO, socket_file_descriptor) + 1;
        }

        //maxfdp = max(STDIN_FILENO, socket_file_descriptor) + 1;
        select(maxfdp, &rset, NULL, NULL, NULL);

        if (FD_ISSET(socket_file_descriptor, &rset)) {
            readMessageFromServer(socket_file_descriptor, currentUdpPort,
                                  &chatObject);
        }

        if (FD_ISSET(udpSockFileDescriptor, &rset)) {
            readMessageFromServer(udpSockFileDescriptor, currentUdpPort,
                                  &chatObject);
        }

        if (FD_ISSET(STDIN_FILENO, &rset)) {
            if (!chatObject.inChatWithAnotherClient) {
                sendMessageToServer(socket_file_descriptor);
            } else {
                sendMessageToAnotherClient(&chatObject, sockToSendMessagefd);
            }
        }
    }
    
    exit(0);
}
