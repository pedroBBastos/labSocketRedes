#!/bin/bash
echo "------- rm server-output ---------"
rm -f ./server-output
echo "------- client.c ---------"
gcc -Wall cliente.c -o cliente
echo "------- servidor.c -------"
gcc -Wall servidor.c -o servidor
