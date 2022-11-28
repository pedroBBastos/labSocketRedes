#!/bin/bash

for counter in $(seq 1 20); do 
	echo "Connecting to server";
	./cliente 0.0.0.0 44333 &
done
