#!/bin/bash
#prefix="valgrind --leak-check=full ./bin/client -f ./sock -p -t 200"
prefix="./client -f ./SOLsocket.sk -p"

#stampa help
$prefix -h

#scrittura di n file all'interno una directory con altre directory all'interno
$prefix -w ./test,3

#scrittura di tutti i file all'interno una directory con altre directory all'interno
$prefix -w ./test

#scrittura di file all'interno di una cartella inesistente
$prefix -w ./pluto

#scrittura di file separati da virgola
$prefix -W test2/prova1,files2/prova2,files2/prova3

#scrittura di file inesistente su disco
$prefix -W pippo

#scrittura di un file già esistente per verificare che non sia sovrascritto (la richiesta deve fallire)
$prefix -W /test/esempio1

#lettura di un file e scrittura in una cartella su disco
$prefix -d readdir1 -r test/esempio2

#lettura di tutti i file del server
$prefix -d readdir2 -R 

#lettura di N file del server
$prefix -d readdir3 -R 3

#lettura di un file inesistente (la richiesta deve fallire)
$prefix -r pippo

#applicazione remove a un file
$prefix -c test/esempio1,test/esempio2

#applicazione remove a un file non presente (la richiesta deve fallire)
$prefix -c test/esempio3

killall -s SIGHUP memcheck-amd64-