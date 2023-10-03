#ifndef OVESP_H
#define OVESP_H
#include <mysql.h>
#include <pthread.h>
#include <mysql.h>


typedef struct
{
  int   id;
  char  intitule[20];
  float prix;
  int   stock;  
  char  image[20];
} ARTICLE;


extern pthread_key_t cle;
extern MYSQL* connexion;

bool OVESP(char* requete, char* reponse,int socket, ARTICLE** panier);


#endif