#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "./LibSocket/socket.h"
#include "OVESP/OVESP.h"
#include <mysql.h>


#define NB_THREADS_POOL 2
#define TAILLE_FILE_ATTENTE 1



struct SocketClient {
    int socket;
    struct SocketClient* next;
};
typedef struct SocketClient SocketClient;


int nbClientFile = 0;
int file = 0;
pthread_mutex_t mutexNbClientFile;
pthread_mutex_t mutexDB;
pthread_cond_t condSocketsAcceptees;




void HandlerSIGINT(int s);
void TraitementConnexion(int sService);
void* FctThreadClient(void* p);

void DestructeurVS(void *p);



int sEcoute;
MYSQL* connexion;

SocketClient* current;
SocketClient* last;

pthread_key_t cle;


int main(int argc,char* argv[])
{

  if (argc != 2)
  {
    printf("Erreur...\n");
    printf("USAGE : Serveur portServeur\n");
    exit(1);
  }

  pthread_mutex_init(&mutexNbClientFile,NULL);
  pthread_mutex_init(&mutexDB,NULL);
  pthread_cond_init(&condSocketsAcceptees,NULL);



  struct sigaction A;
  A.sa_flags = 0;
  sigemptyset(&A.sa_mask);
  A.sa_handler = HandlerSIGINT;


  if (sigaction(SIGINT,&A,NULL) == -1)
  {
    perror("Erreur de sigaction");
    exit(1);
  }

  if ((sEcoute = ServerSocket(atoi(argv[1]))) == -1)
  {
    perror("Erreur de ServeurSocket");
    exit(1);
  }

  
  //CONNEXION DB

  connexion = mysql_init(NULL);
  if(mysql_real_connect(connexion,"192.168.203.1","LA","LA","PSLA_RTI",0,0,0) == NULL)
  {
    fprintf(stderr,"(SERVEUR) Erreur de connexion à la base de données...\n");
    exit(1);  
  }

  //cle
 	pthread_key_create(&cle, DestructeurVS);



  printf("Création du pool de threads.\n");
  pthread_t th[NB_THREADS_POOL];

  for (int i=0 ; i<NB_THREADS_POOL ; i++)
    pthread_create(&th[i],NULL,FctThreadClient,NULL);


  

  int sService;
  char ipClient[50];
  printf("Demarrage du serveur.\n");

  current = (SocketClient *) malloc(sizeof(SocketClient));
  current->next = NULL; // Set next to NULL
  last = current;



  while(1)
  {
    pthread_mutex_lock(&mutexNbClientFile);


    printf("Attente d'une connexion...\n");


    do
    {
      pthread_mutex_unlock(&mutexNbClientFile);
      if ((sService = Accept(sEcoute,ipClient)) == -1)
      {
        perror("Erreur de Accept");
        close(sEcoute);
        exit(1);
      }
      pthread_mutex_lock(&mutexNbClientFile);
    }
    while(nbClientFile == TAILLE_FILE_ATTENTE + NB_THREADS_POOL); //enlever requete inutile



    if(nbClientFile >= NB_THREADS_POOL)
    {
        char requete[200];
        int nbEcrits;

        strcpy(requete, "LOGIN#FILE#Placer en file d'attente");

        if ((nbEcrits = Send(sService,requete,strlen(requete))) < 0)
        {
          perror("Erreur de Send");
          close(sService);
          HandlerSIGINT(0);
        }
    }



    last->socket = sService;
    last->next = (SocketClient *) malloc(sizeof(SocketClient));
    last = last->next;
    nbClientFile++;
    file++;

    printf("Connexion acceptée : IP=%s socket=%d\n",ipClient,sService);




    pthread_mutex_unlock(&mutexNbClientFile);
    pthread_cond_signal(&condSocketsAcceptees);

  }
}
void* FctThreadClient(void* p)
{
 int sService;
 
 while(1)
 {
    printf("\t[THREAD %p] Attente socket...\n",pthread_self());
    pthread_mutex_lock(&mutexNbClientFile);

    while (file == 0)
    {
      pthread_cond_wait(&condSocketsAcceptees,&mutexNbClientFile);
    }




    SocketClient* previous = current;
    sService = current->socket;
    current = current->next;
    file--;
    previous->next = NULL; // Set next to NULL
    free(previous);

    

    
      
    pthread_mutex_unlock(&mutexNbClientFile);
    
    printf("\t[THREAD %p] Je m'occupe de la socket %d\n",pthread_self(),sService);
    TraitementConnexion(sService);
 }
}

void TraitementConnexion(int sService)
{
  char requete[200], reponse[200];
  int nbLus, nbEcrits;
  bool onContinue = true;



  while (onContinue)
  {
    printf("\t[THREAD %p] Attente requete...\n",pthread_self());

    printf("\n1 sService: %d  , requete: %s",sService,requete);

    if ((nbLus = Receive(sService,requete)) < 0)
    {
      printf("\n2 sService: %d  , requete: %s",sService,requete);
      perror("Erreur de Receive Traitement connexion");
      close(sService);
      HandlerSIGINT(0);
    }

    if (nbLus == 0)
    {
      printf("\t[THREAD %p] Fin de connexion du client.\n",pthread_self());
      close(sService);
      return;
    }

    requete[nbLus] = 0;
    printf("\t[THREAD %p] Requete recue = %s\n",pthread_self(),requete);


    pthread_mutex_lock(&mutexDB);
    onContinue = OVESP(requete,reponse,sService);
    pthread_mutex_unlock(&mutexDB);



    if ((nbEcrits = Send(sService,reponse,strlen(reponse))) < 0)
    {
      perror("Erreur de Send");
      close(sService);
      HandlerSIGINT(0);
    }

    printf("\t[THREAD %p] Reponse envoyee = %s\n",pthread_self(),reponse);


    if(!onContinue)
    {
      close(sService);
      pthread_mutex_lock(&mutexNbClientFile);
      nbClientFile--;
      pthread_mutex_unlock(&mutexNbClientFile);
    }
  }
}


void HandlerSIGINT(int s) {
    printf("\nArret du serveur.\n");
    close(sEcoute);

    while (current != NULL) {
        SocketClient* temp = current;
        current = current->next;
        free(temp);
    }

    current = NULL;

    pthread_mutex_lock(&mutexDB);
    mysql_close(connexion);
    pthread_mutex_unlock(&mutexDB);

    exit(0);
}


void DestructeurVS(void* p)
{
	printf("Destructeur variable specifique\n");

	fflush(stdout);
	free(p);

}