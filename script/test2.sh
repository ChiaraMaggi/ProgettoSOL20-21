#!/bin/bash
#prefix="valgrind --leak-check=full ./client -f ./SOLsocket.sk -p"
prefix="./client -f ./SOLsocket.sk -p"

#serie di istruzioni di fila per testare politica di rimpiazzo
$prefix -w ./test1 -W test3/esempio1,test3/esempio2,test3/subtest3/esempio3 -w ./test2 -d dir4 -R 5
