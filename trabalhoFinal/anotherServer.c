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
    int clientID; // ID único do client
    int connfd; // file descriptor da conexão TCP do cliente com o servidor
    char client_ip[16]; // endereço IP do cliente
    unsigned int client_socket_port; // porta da conexão TCP do cliente
    unsigned int client_udp_port; // porta UDP do cliente
    int in_chat; // flag para verificar se cliente está em chat ou disponível
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
    clientInfo.client_udp_port = -1;
    clientInfo.in_chat = 0;

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

    clientLinkedList->size++;
}

void removeClientFromClientListByClientID(int id,
                                          ClientLinkedList* clientLinkedList) {
    ClientNode* previousNode = NULL;
    ClientNode* currentNode = clientLinkedList->head;
    while(currentNode != NULL) {
        if (currentNode->clientInformation.clientID == id) {
            printf("Removing item - clientID == %d\n", currentNode->clientInformation.clientID);
            
            if (previousNode != NULL) { // estou na cabeça da lista?
                previousNode->nextNode = currentNode->nextNode;
            } else {
                clientLinkedList->head = currentNode->nextNode;
            }

            if (currentNode->nextNode == NULL) { //estou no fim da lista?
                clientLinkedList->tail = previousNode;
            }

            free(currentNode);
            break;
        }
        previousNode = currentNode;
        currentNode = currentNode->nextNode;
    }
    clientLinkedList->size--;
}

void removeClientFromClientList(ClientInformation clientInfo,
                                ClientLinkedList* clientLinkedList) {
    removeClientFromClientListByClientID(clientInfo.clientID, clientLinkedList);
}

ClientInformation* getClientInformationById(int id,
                                            ClientLinkedList* clientLinkedList) {
    ClientInformation* clientInfo = NULL;
    ClientNode* currentNode = clientLinkedList->head;
    while(currentNode != NULL) {
        if (currentNode->clientInformation.clientID == id) {
            clientInfo = &currentNode->clientInformation;
            break;
        }
        currentNode = currentNode->nextNode;
    }
    return clientInfo;
}

void printClientsList(ClientLinkedList* clientLinkedList) {
    ClientNode* currentNode = clientLinkedList->head;
    printf("Printing clients list ----------------------------\n");
    while(currentNode != NULL) {
        printf("client id: %d\n", currentNode->clientInformation.clientID);
        currentNode = currentNode->nextNode;
    }
    printf("End Printing clients list----------------------------\n");
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

void requestClientsUDPPort(ClientInformation clientInfo) {
    char   buf[MAXDATASIZE];
    strcpy(buf, "give_me_your_udp_port");
    write(clientInfo.connfd, buf, strlen(buf));
}

/*****************************************************************************
 * method to send list of commands to a given client                         *
 *****************************************************************************/

void sendListOfCommandsToClient(ClientInformation* clientInfo) {
    char   buf[MAXDATASIZE];
    snprintf(buf, sizeof(buf), 
		    "[server] Hello! These are the commands available:\n \
            --list-connected-clients\n \
            --chat-with <client-id>\n \
            --exit\n");
    int n = write(clientInfo->connfd, buf, strlen(buf));
    printf("n ->> %d; clientInfo.connfd ->> %d\n", n, clientInfo->connfd);
}

/*****************************************************************************
 * method to send all clients available to new client                        *
 *****************************************************************************/

void sendClientsAvailableToNewClient(ClientInformation clientInfo,
                                     ClientLinkedList* clientLinkedList) {
    char message[MAXLINE + 1];
    ClientNode* currentNode = clientLinkedList->head;

    strcpy(message,"[server] Clients available to chat: \n");

    while(currentNode != NULL) {

        if (currentNode->clientInformation.in_chat == 1) {
            currentNode = currentNode->nextNode;
            continue;
        }

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
 * method to notify all clients connected so far the connection of a new client
 *****************************************************************************/

void notifyAllClientsNewClient(ClientInformation clientInfo,
                               ClientLinkedList* clientLinkedList) {
    char message[MAXLINE + 1];

    strcpy(message,"[server] New client connected! id: ");
    char clientIdString[sizeof(clientInfo.clientID)];
    snprintf(clientIdString, sizeof(clientInfo.clientID), "%d", clientInfo.clientID);
    strcat(message, clientIdString);
    strcat(message, "\n");

    ClientNode* currentNode = clientLinkedList->head;
    while(currentNode != NULL) {
        write(currentNode->clientInformation.connfd, message, MAXLINE + 1);
        currentNode = currentNode->nextNode;
    }
}

/*****************************************************************************
 * method to handle new connection                                           *
 *****************************************************************************/

void handleNewConnection(ClientLinkedList* clientLinkedList, fd_set* allset,
                         int* maxfd, int listenfd) {
    printf("--------------------------------- \n");
    printf("New connection!!! \n");

    struct sockaddr_in cliaddr;
    socklen_t clilen;

    clilen = sizeof(cliaddr);
    int connfd = accept(listenfd, (struct sockaddr *) &cliaddr, &clilen);

    getpeername(connfd , (struct sockaddr*) &cliaddr , (socklen_t*) &clilen);
    ClientInformation clientInfo = getClientInformation(connfd, cliaddr);

    printf("Client IP Address: %s\n", clientInfo.client_ip);
    printf("Client local socket port: %d\n", clientInfo.client_socket_port);

    notifyAllClientsNewClient(clientInfo, clientLinkedList);

    addNewClientToClientList(clientInfo, clientLinkedList);
    printClientsList(clientLinkedList);
    requestClientsUDPPort(clientInfo);
    // sendListOfCommandsToClient(clientInfo);

    
    FD_SET(connfd, allset); /* add new descriptor to set */
    if (connfd > *maxfd) {
        *maxfd = connfd; /* for select */
    }
}

/*****************************************************************************
 * method to update client UDP port, which is the first thing to be done after 
 * a new client connect to the server                                        *
 *****************************************************************************/

void updateClientUDPPort(ClientInformation* clientInformation,
                         char message[MAXLINE]) {
    printf("To set client udp port....\n");
    char substring[6];
    memcpy(substring, &message[15], 5);
    substring[6] = '\0';
    clientInformation->client_udp_port = strtoul(substring, 0L, 10);
    printf("clientInformation->connfd --> %d\n", clientInformation->connfd);
    printf("FINISHED To set client udp port....\n");
}

/*****************************************************************************
 * methods to handle chat initialization between clients                     *
 *****************************************************************************/

void sendClientIsNotAvailableToChatMessage(ClientInformation clientInfo) {
    char   buf[MAXDATASIZE];
    strcpy(buf, "The requested client is not available to chat or it doesn't exist!\n");
    write(clientInfo.connfd, buf, strlen(buf));
}

void sendChatInitializationMessageToClient(ClientInformation clientA,
                                           ClientInformation clientB) {
    // chat_init_with_client  <client_id> <client_ip_address> <client_udp_port>
    char   buf[MAXDATASIZE];
    strcpy(buf, "chat_init_with_client ");

    char clientBId[5];
    snprintf(clientBId, 5, "%d ", clientB.clientID);
    strcat(buf, clientBId);

    char clientBIPAddress[17];
    snprintf(clientBIPAddress, 17, "%s ", clientB.client_ip);
    strcat(buf, clientBIPAddress);

    char clientBUdpPort[sizeof(unsigned int)*8+1];
    snprintf(clientBUdpPort, sizeof(unsigned int)*8+1, "%u", clientB.client_udp_port);
    strcat(buf, clientBUdpPort);

    printf("buf -> %s\n", buf);

    write(clientA.connfd, buf, strlen(buf));
}


void initiateChatBetweenClients(ClientInformation* clientInformation,
                                ClientLinkedList* clientLinkedList,
                                char message[MAXLINE]) {
    printf("Initiating chat between clients...");

    char substring[6];
    memcpy(substring, &message[12], 6);
    substring[6] = '\0';
    int clientIdToChatWith = strtol(substring, 0L, 10);

    printf("clientIdToChatWith -> %d\n", clientIdToChatWith);

    ClientInformation* clientToChat = 
        getClientInformationById(clientIdToChatWith, clientLinkedList);

    if (clientToChat == NULL || clientToChat->in_chat == 1) {
        sendClientIsNotAvailableToChatMessage(*clientInformation);
        return;
    }

    sendChatInitializationMessageToClient(*clientToChat, *clientInformation);
    sendChatInitializationMessageToClient(*clientInformation, *clientToChat);


    // Não devo remover totalmente os clients da lista, pq se não não consiguirei
    // ouvir a mensagem de finalização de chat de ambos para poder colocá-los
    // disponiveis para chat novamente. Preciso pensar em alguma flag...

    // removeClientFromClientList(*clientInformation, clientLinkedList);
    // removeClientFromClientListByClientID(clientIdToChatWith, clientLinkedList);

    clientInformation->in_chat = 1;
    clientToChat->in_chat = 1;
}

/*****************************************************************************
 * method to handle client message                                           *
 *****************************************************************************/

void handleClientMessage(ClientInformation* clientInformation,
                         char message[MAXLINE], ClientLinkedList* clientLinkedList) {
    printf("To handle clients message: %s\n", message);

    if (strcmp(message, "--list-connected-clients") == 0) {
        sendClientsAvailableToNewClient(*clientInformation, clientLinkedList);
    } else if (strncmp(message, "--chat-with", 11) == 0) {
        initiateChatBetweenClients(clientInformation, clientLinkedList, message);
    } else if (strcmp(message, "--exit") == 0) {
        printf("To disconnect client from server...\n");
    } else if (strncmp(message, "my_udp_port_is", 14) == 0) {
        updateClientUDPPort(clientInformation, message);
        printf("INSIDE IF....  clientInfo.connfd ->> %d\n", clientInformation->connfd);
        sendListOfCommandsToClient(clientInformation);
    } else {
        printf("Unknow command sent:\n");
        printf("    %s\n", message);
    }
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
            printf("--------------------------------- \n");
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
                handleClientMessage(&currentNode->clientInformation, buf,
                                    clientLinkedList);
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