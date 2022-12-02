#!/bin/bash
echo "------- rm server-output ---------"
rm -f ./server-output
echo "------- client.c ---------"
gcc -Wall cliente.c -o cliente
echo "------- servidor.c -------"
gcc -Wall servidor.c -o servidor
echo "------- singleProcessServer.c -------"
gcc -Wall singleProcessServer.c -o singleProcessServer
echo "------- anotherServer.c -------"
gcc -Wall anotherServer.c -o anotherServer
echo "------- anotherClient.c -------"
gcc -Wall anotherClient.c -o anotherClient
