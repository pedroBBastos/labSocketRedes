#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

#define LISTENQ 10
#define MAXDATASIZE 500

#define MAXLINE 4096

/*****************************************************************************
 * Util methods/structs                                                      *
 *****************************************************************************/

int referenceIDForCLients = 0;

typedef struct {
    int clientID;
    int connfd;
    char client_ip[16];
    unsigned int client_socket_port;
} ClientInformation;

typedef struct ClientNode_struct {
    ClientInformation clientInformation;
    struct ClientNode_struct* nextNode;
} ClientNode;

typedef struct {
    ClientNode* head;
    ClientNode* tail;
    int size;
} ClientLinkedList;

ClientInformation getClientInformation(int connfd, struct sockaddr_in peeraddr) {
    ClientInformation clientInfo;

    char local_socket_ip[16];
    inet_ntop(AF_INET, &peeraddr.sin_addr, local_socket_ip, sizeof(local_socket_ip));
    unsigned int local_socket_port = ntohs(peeraddr.sin_port);

    strcpy(clientInfo.client_ip, local_socket_ip);
    clientInfo.client_socket_port = local_socket_port;
    clientInfo.clientID = referenceIDForCLients++;
    clientInfo.connfd = connfd; // saving descriptor

    return clientInfo;
}

void initiateClientLinkedList(ClientLinkedList* clientLinkedList) {
    clientLinkedList->head = NULL;
    clientLinkedList->tail = NULL;
    clientLinkedList->size = 0;
}

void addNewClientToClientList(ClientInformation clientInfo,
                              ClientLinkedList* clientLinkedList) {
    if(clientLinkedList->size >= FD_SETSIZE) {
        perror("too many clients");
        exit(1);
    }

    ClientNode* newClientNode = (ClientNode*)malloc(sizeof(ClientNode));
    newClientNode->clientInformation = clientInfo;
    newClientNode->nextNode = NULL;

    if (clientLinkedList->head == NULL) {
        clientLinkedList->head = newClientNode;
        clientLinkedList->tail = newClientNode;
    } else {
        clientLinkedList->tail->nextNode = newClientNode;
        clientLinkedList->tail = newClientNode;
    }
}

void removeClientFromClientList(ClientInformation clientInfo,
                                ClientLinkedList* clientLinkedList) {
    ClientNode* previousNode = NULL;
    ClientNode* currentNode = clientLinkedList->head;
    while(currentNode != NULL) {
        if (currentNode->clientInformation.clientID == clientInfo.clientID) {
            printf("Removing item - clientID == %d\n", currentNode->clientInformation.clientID);
            if (previousNode != NULL) {
                previousNode->nextNode = currentNode->nextNode;
            } else {
                clientLinkedList->head = currentNode->nextNode;
            }
            free(currentNode);
            break;
        }
        previousNode = currentNode;
        currentNode = currentNode->nextNode;
    }
    clientLinkedList->size--;
}

void printClientsList(ClientLinkedList* clientLinkedList) {
    ClientNode* currentNode = clientLinkedList->head;
    printf("----------------------------\n");
    while(currentNode != NULL) {
        printf("client id: %d\n", currentNode->clientInformation.clientID);
        currentNode = currentNode->nextNode;
    }
    printf("----------------------------\n");
}

/*****************************************************************************
 * checkProgramInput                                                         *
 *****************************************************************************/

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

/*****************************************************************************
 * server initialization                                                     *
 *****************************************************************************/

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


void sendListOfCommandsToClient(ClientInformation clientInfo) {
    char   buf[MAXDATASIZE];
    snprintf(buf, sizeof(buf), 
		    "Hello! These are the commands available:\n \
            --list-connected-clients\n \
            --chat-with <client-id>\n \
            --exit\n");
    write(clientInfo.connfd, buf, strlen(buf));
}

/*****************************************************************************
 * method to send all clients available to new client                        *
 *****************************************************************************/

void sendClientsAvailableToNewClient(ClientInformation clientInfo,
                                     ClientLinkedList* clientLinkedList) {
    char message[MAXLINE + 1];
    ClientNode* currentNode = clientLinkedList->head;

    strcpy(message,"Clients available to chat: \n");

    while(currentNode != NULL) {
        char clientIdString[5];
        snprintf(clientIdString, 5, "%d", currentNode->clientInformation.clientID);
        strcat(message, clientIdString);
        if (currentNode->nextNode != NULL)
            strcat(message, ", ");
        currentNode = currentNode->nextNode;
    }

    strcat(message, "\n");
    printf("%s\n", message);
    write(clientInfo.connfd, message, MAXLINE + 1);
}

/*****************************************************************************
 * method to handle new connection                                           *
 *****************************************************************************/

void handleNewConnection(ClientLinkedList* clientLinkedList, fd_set* allset,
                         int* maxfd, int listenfd) {
    printf("New connection!!! \n");

    struct sockaddr_in cliaddr;
    socklen_t clilen;

    clilen = sizeof(cliaddr);
    int connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);

    getpeername(connfd , (struct sockaddr*) &cliaddr , (socklen_t*) &clilen);
    ClientInformation clientInfo = getClientInformation(connfd, cliaddr);

    printf("Client IP Address: %s\n", clientInfo.client_ip);
    printf("Client local socket port: %d\n", clientInfo.client_socket_port);

    // sendClientsAvailableToNewClient(clientInfo, clientLinkedList);
    addNewClientToClientList(clientInfo, clientLinkedList);
    sendListOfCommandsToClient(clientInfo);

    
    FD_SET(connfd, allset); /* add new descriptor to set */
    if (connfd > *maxfd) {
        *maxfd = connfd; /* for select */
    }
}

/*****************************************************************************
 * method to handle client message                                           *
 *****************************************************************************/

void handleClientMessage(ClientInformation clientInformation,
                         char message[MAXLINE]) {
    int clientIdToChat = atoi(message);
    printf("client id chosen -> %d\n", clientIdToChat);

    // tirar da lista o cliente atual e o indicado por este da lista
    // enviar para ambos mensagem de que podem se falar???
}

/*****************************************************************************
 * method to check all clients for data                                      *
 *****************************************************************************/

void checkAllClientsForData(ClientLinkedList* clientLinkedList, fd_set* rset,
                            fd_set* allset) {
    char buf[MAXLINE];

    ClientNode* currentNode = clientLinkedList->head;
    while(currentNode != NULL) {
        int sockfd = currentNode->clientInformation.connfd;
        if (FD_ISSET(sockfd, rset)) {
            int n;
            if ((n = read(sockfd, buf, MAXLINE)) == 0) { /* connection closed by client */
                printf("Connection closed by client!!! \n");
                close(sockfd);
                FD_CLR(sockfd, allset);

                ClientNode* nodeToErase = currentNode;
                currentNode = currentNode->nextNode;
                removeClientFromClientList(nodeToErase->clientInformation, clientLinkedList);
                continue;
            } else {
                printf("recebido do cliente %s\n", buf);
                // write(sockfd, buf, n);  // envia de volta para o cliente sua mensagem...
                handleClientMessage(currentNode->clientInformation, buf);
            }
        }

        currentNode = currentNode->nextNode;
    }
}

/*****************************************************************************
 * main method                                                               *
 *****************************************************************************/

int main (int argc, char **argv) {
    
    checkProgramInput(argc, argv);

    int listenfd = initiateServer(argv[1]);
    int maxfd = listenfd;

    fd_set rset, allset;
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    ClientLinkedList clientLinkedList;
    initiateClientLinkedList(&clientLinkedList);

    for ( ; ; ) {
        rset = allset; /* structure assignment */
        select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (FD_ISSET(listenfd, &rset)) { /* new client connection */
            handleNewConnection(&clientLinkedList, &allset, &maxfd, listenfd);
        }
        checkAllClientsForData(&clientLinkedList, &rset, &allset);
    }
}