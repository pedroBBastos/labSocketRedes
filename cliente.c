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

int main(int argc, char **argv) {
    int    sock_file_descriptor, n;
    char   recvline[MAXLINE + 1];
    char   error[MAXLINE + 1];
    struct sockaddr_in servaddr;
    char text_to_server[500];

    if (argc != 3) {
        strcpy(error,"uso: ");
        strcat(error,argv[0]);
        strcat(error," <Server-IPaddress> <Server-Port>");
        perror(error);
        exit(1);
    }

    if ( (sock_file_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
        perror("inet_pton error");
        exit(1);
    }

    if (connect(sock_file_descriptor, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("connect error");
        exit(1);
    }

    while ( (n = read(sock_file_descriptor, recvline, MAXLINE)) > 0) {
        recvline[n] = 0;
        if (fputs(recvline, stdout) == EOF) {
            perror("fputs error");
            exit(1);
        }

        printf("Digite seu texto: \n");
        scanf("%[^\n]s", text_to_server);

        //Envia mensagem para o servidor
        if(send(sock_file_descriptor, text_to_server, 500, 0) < 0) {
            puts("Send failed");
            return 1;
        }
    }

    if (n < 0) {
        perror("read error");
        exit(1);
    }


    char local_socket_ip[16];
    unsigned int local_socket_port;
    struct sockaddr_in local_addr;

    bzero(&local_addr, sizeof(local_addr));
    socklen_t len = sizeof(local_addr);
    
    //obtendo informações do socket local.
    //sock_file_descriptor foi criado previamente para se comunicar com o servidor
    getsockname(sock_file_descriptor, (struct sockaddr *) &local_addr, &len);

    // função inet_ntop -> para converter de endereço binário para formato texto
    inet_ntop(AF_INET, &local_addr.sin_addr, local_socket_ip, sizeof(local_socket_ip));

    // função ntohs -> conversão endereço de rede para host
    local_socket_port = ntohs(local_addr.sin_port);

    printf("Endereco IP socket local: %s\n", local_socket_ip);
    printf("Porta socket local: %u\n", local_socket_port);

    exit(0);
}
