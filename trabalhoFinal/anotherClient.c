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

ChatObject getNewChatObject() {
    ChatObject chatObject;
    chatObject.inChatWithAnotherClient = 0;
    chatObject.peerId = -1;
    strcpy(chatObject.peerIPAddress, "");
    chatObject.peerUDPPort = 0;
    return chatObject;
}

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
    int server_socket_fdescriptor;
    struct sockaddr_in servaddr;
    if ( (server_socket_fdescriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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

    if (connect(server_socket_fdescriptor, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("connect error");
        exit(1);
    }

    return server_socket_fdescriptor;
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

    bzero(&chatObject->peeraddr, sizeof(chatObject->peeraddr));
    chatObject->peeraddr.sin_family = AF_INET;
    chatObject->peeraddr.sin_port = htons(chatObject->peerUDPPort);
    inet_pton(AF_INET, chatObject->peerIPAddress, &chatObject->peeraddr.sin_addr);

    printf("[chat-mode] You are now entering in chat mode with client %d..\n", chatObject->peerId);
    printf("[chat-mode] If you want to get out from the chat and return to server, type 'finalizar_chat'\n");
}

void sendFinishedChatoMessageToServer(int server_socket_fdescriptor) {
    char text_to_server[500];
    bzero(&text_to_server, sizeof(text_to_server));

    strcpy(text_to_server, "finished_chat_with_peer");

    //Envia mensagem para o servidor
    if(send(server_socket_fdescriptor, text_to_server, 500, 0) < 0) {
        puts("Send failed");
        exit(1);
    }
}

void sendMessageToAnotherClient(ChatObject* chatObject, int sockToSendMessagefd,
                                int server_socket_fdescriptor) {
    // printf("To send message to another client\n");

    char text_to_peer[500];
    bzero(&text_to_peer, sizeof(text_to_peer));

    fgets(text_to_peer, 500, stdin);

    char text_to_check[500];
    strcpy(text_to_check, text_to_peer);
    text_to_check[strcspn(text_to_check, "\n")] = 0;

    if (strcmp(text_to_check, "finalizar_chat") == 0) {
        sendFinishedChatoMessageToServer(server_socket_fdescriptor);
        chatObject->inChatWithAnotherClient = 0;
    }

    sendto(sockToSendMessagefd, text_to_peer, strlen(text_to_peer), 0, 
            (struct sockaddr*) &chatObject->peeraddr, sizeof(chatObject->peeraddr));
}

/*****************************************************************************
 * method to send message from stdin to server                               *
 *****************************************************************************/

void sendMessageToServer(int server_socket_fdescriptor) {
    // printf("Sending message to server\n");

    char text_to_server[500];
    bzero(&text_to_server, sizeof(text_to_server));

    fgets(text_to_server, 500, stdin);
    text_to_server[strcspn(text_to_server, "\n")] = 0;

    //Envia mensagem para o servidor
    if(send(server_socket_fdescriptor, text_to_server, 500, 0) < 0) {
        puts("Send failed");
        exit(1);
    }
}

void replyCurrentUDPPortToServer(int server_socket_fdescriptor,
                                 unsigned int udpPort) {
    char text_to_server[500];
    bzero(&text_to_server, sizeof(text_to_server));

    strcpy(text_to_server, "my_udp_port_is ");
    char myUdpPortString[sizeof(unsigned int)*8+1];
    snprintf(myUdpPortString, sizeof(unsigned int)*8+1, "%u", udpPort);
    strcat(text_to_server, myUdpPortString);

    //Envia mensagem para o servidor
    if(send(server_socket_fdescriptor, text_to_server, 500, 0) < 0) {
        puts("Send failed");
        exit(1);
    }
}

/*****************************************************************************
 * method to read message from the other client wich is chatting with me     *
 *****************************************************************************/

void readMessageFromChatPeer(int udp_socket_file_descriptor, ChatObject* chatObject,
                             int server_socket_fdescriptor) {
    char recvline[MAXLINE + 1];
    bzero(&recvline, sizeof(recvline));

    if (read(udp_socket_file_descriptor, recvline, MAXLINE + 1) == 0) {
        perror("Server terminated prematurely!!");
        exit(1);
    } else {

        char text_to_check[500];
        strcpy(text_to_check, recvline);
        text_to_check[strcspn(text_to_check, "\n")] = 0;

        if (strcmp(text_to_check, "finalizar_chat") == 0) {
            // TODO: Mandar para o servidor que est??-se finalizando o chat
            printf("[chat-mode] Chat ended by peer client! Returning to server command mode.. \n");
            sendFinishedChatoMessageToServer(server_socket_fdescriptor);
            chatObject->inChatWithAnotherClient = 0;
        } else {
            printf("[Client id:%d]: %s", chatObject->peerId, recvline);
        }
    }
}

/*****************************************************************************
 * method to read message from server                                        *
 *****************************************************************************/

void readMessageFromServer(int server_socket_fdescriptor, unsigned int udpPort,
                           ChatObject* chatObject) {
    char recvline[MAXLINE + 1];
    bzero(&recvline, sizeof(recvline));

    // printf("Got message from server\n");

    if (read(server_socket_fdescriptor, recvline, MAXLINE + 1) == 0) {
        perror("Server terminated prematurely!!");
        exit(1);
    } else {
        if (strncmp(recvline, "give_me_your_udp_port", 21) == 0) {
            replyCurrentUDPPortToServer(server_socket_fdescriptor, udpPort);
        } else if (strncmp(recvline, "chat_init_with_client", 21) == 0) {
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
    int server_socket_fdescriptor, maxfdp;
    struct sockaddr_in udpaddr;
    int udpSockFileDescriptor;
    unsigned int currentUdpPort;

    checkProgramInput(argc, argv);

    server_socket_fdescriptor = conectToServer(argv[1], argv[2]);
    udpSockFileDescriptor = initiateUDPSocket(&udpaddr);
    currentUdpPort = retrieveCurrentUDPPort(udpSockFileDescriptor);

    fd_set rset;
    FD_ZERO(&rset);

    ChatObject chatObject = getNewChatObject();
    int sockToSendMessagefd = socket(AF_INET, SOCK_DGRAM, 0);

    for ( ; ; ) {
        FD_SET(server_socket_fdescriptor, &rset);
        FD_SET(STDIN_FILENO, &rset);

        if (chatObject.inChatWithAnotherClient) {
            FD_SET(udpSockFileDescriptor, &rset);
            maxfdp = max(max(STDIN_FILENO, server_socket_fdescriptor), udpSockFileDescriptor) + 1;
        } else {
            maxfdp = max(STDIN_FILENO, server_socket_fdescriptor) + 1;
        }

        select(maxfdp, &rset, NULL, NULL, NULL);

        if (FD_ISSET(server_socket_fdescriptor, &rset)) {
            readMessageFromServer(server_socket_fdescriptor, currentUdpPort,
                                  &chatObject);
        }

        if (FD_ISSET(udpSockFileDescriptor, &rset)) {
            readMessageFromChatPeer(udpSockFileDescriptor, &chatObject, server_socket_fdescriptor);
        }

        if (FD_ISSET(STDIN_FILENO, &rset)) {
            if (!chatObject.inChatWithAnotherClient) {
                sendMessageToServer(server_socket_fdescriptor);
            } else {
                sendMessageToAnotherClient(&chatObject, sockToSendMessagefd, server_socket_fdescriptor);
            }
        }
    }
    
    exit(0);
}
