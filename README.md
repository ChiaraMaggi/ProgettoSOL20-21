# ProgettoSOL20-21
Lo studente deve realizzare un file storage server in cui la memorizzazione dei file avviene in memoria principale. La capacità dello storage è fissata all’avvio e non varia dinamicamente durante l’esecuzione del server. Per poter leggere, scrivere o eliminare file all’interno del file storage, il client deve connettersi al server ed utilizzare una API che dovrà essere sviluppata dallo studente. Il server esegue tutte le operazioni
richieste dai client operando sempre e solo in memoria principale e mai sul disco.
La capacità del file storage, unitamente ad altri parametri di configurazione, è definita al momento dell’avvio del server tramite un file di configurazione testuale.
Il file storage server è implementato come un singolo processo multi-threaded in grado di accettare connessioni da multipli client. Il processo server dovrà essere in grado di gestire adeguatamente alcune decine di connessioni contemporanee da parte di più client.
Ogni client mantiene una sola connessione verso il server sulla quale invia una o più richieste relative ai file memorizzati nel server, ed ottiene le risposte in accordo al protocollo di comunicazione “richiesta-risposta”. Un file è identificato univocamente dal suo path assoluto.

-> Comandi da eseguire nella directory principale :

make : costruisce gli eseguibili "server" e "client"

make test1 : avvia il primo test

make test2 : avvia il secondo test

make clean : elimina gli eseguibili "server" e "client"

make cleanall : elimina tutti i file generati da make (eseguibili, oggetto, temporanei, librerie, ...)



-> Comandi server :

-f config.txt : specifica il file di config da usare



->Comandi client :

-h : lista operazioni client

-f filename : connettiti al socket filename

-w dirname[,n=0] : scrivi sul server n file contenuti nella directory dirname, se n=0 o non specificato scrivili tutti

-W file1[,file2] : scrivi i file sul server

-r file1[,file2] : leggi i file dal server

-R [n=0] : leggi n file dal server, se n=0 o non specificato leggili tutti

-d dirname : salva i file letti nella directory dirname del client

-t time : ritardo in ms tra le richieste al server

-c file1[,file2] : elimina i file dal server

-p : abilita stampe su stdout per ogni operazione



->Struttura directory :

configfile : contiene configtest1.txt e configtest2.txt per i due test

test : cartella con file utilizzati per i test (con sottocartella)

test2 : cartella con i file utilizzati per test (senza sottocartella)

test3 : cartella con file utilizzati per i test (con sottocartella)

dir1/2/3/4 : cartelle che vengono create dai test
