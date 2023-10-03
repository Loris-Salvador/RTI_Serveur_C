#include "socket.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h> // pour memset
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int ServerSocket(int port)
{
    int s;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Erreur de socket()");
        exit(1);
    }

    //cheat//

    int option = 1;

    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    /////////

    struct addrinfo *results;
    struct addrinfo hints;
    memset(&hints,0,sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV; // pour une connexion passive

    char port_str[10];
    sprintf(port_str, "%d", port);


    if (getaddrinfo(NULL,port_str,&hints,&results) != 0)
    {
        close(s);
        exit(1);
    }


    if (bind(s,results->ai_addr,results->ai_addrlen) < 0)
    {
        perror("Erreur de bind()");
        exit(1);
    }

    return s;
}
int Accept(int sEcoute,char *ipClient)
{
    if(listen(sEcoute, SOMAXCONN) != 0)
    {
        perror("Erreur listen");
        exit(1);
    }

    struct sockaddr adrClient;
    socklen_t adrClientLen = sizeof(adrClient); // Initialisation de la taille

    int sService;

    if ((sService = accept(sEcoute, &adrClient, &adrClientLen)) == -1) {
        perror("Erreur accept");
        exit(1);
    }


    char port[NI_MAXSERV];
    
    getnameinfo(&adrClient,adrClientLen,ipClient,NI_MAXHOST,port,NI_MAXSERV,NI_NUMERICSERV | NI_NUMERICHOST);
    printf("Client connecte --> Adresse IP: %s\n",ipClient);

    return sService;



}
int ClientSocket(char* ipServeur,int portServeur)
{
    int sClient;

    if ((sClient = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Erreur de socket()");
        exit(1);
    }


    //cheat///

    int option = 1;
    if (setsockopt(sClient, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");


    /////////////


    struct addrinfo hints;
    struct addrinfo *results;
    memset(&hints,0,sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;


    char port[10];

    sprintf(port, "%d", portServeur);

    if (getaddrinfo(ipServeur,port,&hints,&results) != 0)
        exit(1);

    if (connect(sClient,results->ai_addr,results->ai_addrlen) == -1)
    {
        perror("Erreur de connect()");
        exit(1);
    }
    printf("connect() reussi !\n");


    return sClient;
}
int Send(int sSocket,char* data,int taille)
{
    if (taille > TAILLE_MAX_DATA)
        return -1;

    char trame[TAILLE_MAX_DATA+2];
    memcpy(trame,data,taille);
    //trame[taille] = '\0';
    trame[taille] = '<';
    trame[taille+1] = '}';



    return write(sSocket,trame,taille+2)-2;

}
int Receive(int sSocket,char* data)
{
    bool fini = false;
    int nbLus, i = 0;
    char lu1,lu2;

    while(!fini)
    {
        if ((nbLus = read(sSocket,&lu1,1)) == -1)
            return -1;

        if (nbLus == 0)
            return i; // connexion fermee par client

        if (lu1 == '<')
        {
            if ((nbLus = read(sSocket,&lu2,1)) == -1)
                return -1;

            if (nbLus == 0)
                return i; // connexion fermee par client

            if (lu2 == '}')
                fini = true;
            else
            {
                data[i] = lu1;
                data[i+1] = lu2;
                i += 2;
            }
        }
        else
        {
            data[i] = lu1;
            i++;
        }
    }
    data[i] = '\0';
    return i;

}

void CloseSocket(int socket)
{
    close(socket);
}
