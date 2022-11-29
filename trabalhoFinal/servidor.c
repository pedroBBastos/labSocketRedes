#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
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

int referenceIDForCLients = 0;

/***************************************************************/

typedef struct {
    int clientID;
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
} ClientLinkedList;

/***************************************************************/

void sig_chld(int signo) {
    pid_t pid;
    int stat;
    while ( (pid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("child %d terminated\n", pid);
    return;
}

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

ClientInformation getClientInformation(int connfd, struct sockaddr_in peeraddr) {
    ClientInformation clientInfo;

    char local_socket_ip[16];
    inet_ntop(AF_INET, &peeraddr.sin_addr, local_socket_ip, sizeof(local_socket_ip));
    unsigned int local_socket_port = ntohs(peeraddr.sin_port);

    strcpy(clientInfo.client_ip, local_socket_ip);
    clientInfo.client_socket_port = local_socket_port;
    clientInfo.clientID = referenceIDForCLients++;

    return clientInfo;
}

void addNewClientToClientList(ClientInformation clientInfo,
                              ClientLinkedList* clientLinkedList) {
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
        printf("currentNode->clientInformation.clientID -> %d\n", currentNode->clientInformation.clientID);
        if (currentNode->clientInformation.clientID == clientInfo.clientID) {
            printf("Removing item\n");
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

void readClientCommandExecutionResponse(int connfd,
                                        char currentCommand[40],
                                        ClientInformation clientInfo) {
    char received_text[500];
    int n;

    FILE *out_file = fopen("clients-outputs", "a");
    if (out_file == NULL) {
        // printf("Error! Could not open file\n");
        exit(-1);
    }

    for ( ; ; ) {
        bzero(&received_text, sizeof(received_text));
        if( (n = read(connfd, received_text, 500)) > 0) {
            if(strcmp(received_text, "end-command-execution") == 0) {
                // printf("Command totally executed!!\n");
                break;
            } else {
                // printf(out_file, received_text);
            }
        } else {
            // printf("Read error! Finishing client handling \n");
            break;
        }
    }

    fclose(out_file);
}

void handleClient(int connfd, ClientInformation clientInfo) {
    // int n;

    // char commands[4][40];
    // strcpy(commands[0], "ls\0"); 
    // strcpy(commands[1], "pwd\0");
    // strcpy(commands[2], "ls -l\0");
    // strcpy(commands[3], "exit\0");

    // for (int i=0; i < 4; i++) {
    //     if ( (n = write(connfd, commands[i], strlen(commands[i]))) > 0 && 
    //             strcmp(commands[i], "exit") != 0) {
    //         // readClientCommandExecutionResponse(connfd, commands[i], clientInfo);
    //     }
    // }
}

void startListenToConnections(int listenfd, ClientLinkedList* clientLinkedList) {
    int connfd;

    __sighandler_t asd = signal(SIGCHLD, sig_chld); /* must call waitpid() */
    if(asd != NULL) {
        printf("NULL \n");    
    }

    for ( ; ; ) {

        struct sockaddr_in peeraddr;
        socklen_t peerlen;
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept");
            exit(1);
        }

        getpeername(connfd , (struct sockaddr*) &peeraddr , (socklen_t*) &peerlen);
	    ClientInformation clientInfo = getClientInformation(connfd, peeraddr);

        printf("Client IP Address: %s\n", clientInfo.client_ip);
        printf("Client local socket port: %d\n", clientInfo.client_socket_port);

        addNewClientToClientList(clientInfo, clientLinkedList);
        // printClientsList(clientLinkedList);

        pid_t pid = fork();
        if (pid == 0){
            close(listenfd);
            // // printf("[test] Inside procces (pid): %d \n", pid);
            handleClient(connfd, clientInfo);

            // printClientsList(clientLinkedList);
            removeClientFromClientList(clientInfo, clientLinkedList);
            // printClientsList(clientLinkedList);

            printf("Closing connection with client %d \n", clientInfo.clientID);
            close(connfd);
            exit(0);
        }

        close(connfd);
    }
}

int main (int argc, char **argv) {
    int listenfd;

    ClientLinkedList clientLinkedList;
    clientLinkedList.head = NULL;
    clientLinkedList.tail = NULL;

    checkProgramInput(argc, argv);
    listenfd = initiateServer(argv[1]);

    startListenToConnections(listenfd, &clientLinkedList);

    return(0);
}
