#!/bin/bash
#prefix="valgrind --leak-check=full ./bin/client -f ./sock -p -t 200"
prefix="./client -f ./SOLsocket.sk -p -t 200"

#stampa help
$prefix -h

#scrittura di n file all'interno una directory con altre directory all'interno
$prefix -w ./test,2

#scrittura di tutti i file all'interno di una directory con altre directory all'interno
$prefix -w ./test

#scrittura di file all'interno di una cartella inesistente
$prefix -w ./pluto

#scrittura di file separati da virgola
$prefix -W test2/prova1,test2/prova2,test2/prova3

#scrittura di file inesistente su disco
$prefix -W pippo

#scrittura di un file già esistente per verificare che non sia sovrascritto (la richiesta deve fallire)
$prefix -W test/esempio1

#lettura di un file e scrittura in una cartella su disco
$prefix -d readdir1 -r test/esempio2

#lettura di tutti i file del server
$prefix -R -d readdir2 

#lettura di N file del server
$prefix -R 3 -d readdir3

#lettura di un file inesistente (la richiesta deve fallire)
$prefix -r pippo

#applicazione remove a un file
$prefix -c test/esempio1,test/subtest/esempio3

#applicazione remove a un file non presente (la richiesta deve fallire)
$prefix -c test/esempio3
