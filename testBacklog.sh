#!/bin/bash

# ./cliente 0.0.0.0 44333 &

for counter in $(seq 1 10); do 
	echo "Connecting to server";
	./cliente 0.0.0.0 44333 &
done

netstat -taulpn | grep 44333 | grep "\./servidor"
