#include "OVESP.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <mysql.h>




bool is_Log_In_BD(char* user, MYSQL* connexion);
bool OVESP_Login(char* user, char* password, MYSQL* connexion);
void add_Client_In_BD(char* user, char* password, MYSQL* connexion);

void OVESP_Consult(int idArticle, ARTICLE* art);

void OVESP_Achat(int idArticle, int quantite, ARTICLE* art);

int OVESP_Caddie(ARTICLE** panier);

bool OVESP_Cancel(int idArticle, MYSQL* connexion);

void OVESP_CancelAll();

void OVESP_Confirmer(MYSQL*, char*);

void OVESP_Logout();


ARTICLE ** caddie;



//***** Parsing de la requete et creation de la reponse *************
bool OVESP(char* requete, char* reponse,int socket, ARTICLE** cadd, MYSQL* connexion)
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;

    caddie = cadd;
    // ***** Récupération nom de la requete *****************
    char *ptr = strtok(requete,"#");

    // ***** LOGIN ******************************************
    if (strcmp(ptr,"LOGIN") == 0) 
    {
        char user[50], password[50];

        strcpy(user,strtok(NULL,"#"));
        strcpy(password,strtok(NULL,"#"));

        int newclient = atoi(strtok(NULL,"#"));

        
        printf("\t[THREAD %p] LOGIN de %s\n",pthread_self(),user);
        
        if(!newclient) // Client existant dans la BD
        {
            if (OVESP_Login(user,password, connexion))
            {
                sprintf(reponse,"LOGIN#OK#Client Log");
            } 
            else
            {
                if(is_Log_In_BD(user, connexion))
                    sprintf(reponse,"LOGIN#BAD#Mauvais mot de passe !");
                else
                    sprintf(reponse,"LOGIN#BAD#Client n'existe pas !");

                return false;
            }
        }
        else if (!is_Log_In_BD(user, connexion)) // New Client qui n'est pas dejà dans la BD
        {
            add_Client_In_BD(user, password, connexion);
            sprintf(reponse,"LOGIN#OK#Nouveau Client");
        }
        else // Ce "user" est déjà utilisé
        {
            sprintf(reponse,"LOGIN#BAD#Nom de Client deja utilise !");
            return false;

        }
            
    
    }
    // ***** LOGOUT *****************************************
    if (strcmp(ptr,"LOGOUT") == 0)
    {
        printf("\t[THREAD %p] LOGOUT\n",pthread_self());
        sprintf(reponse,"LOGOUT#OK");
        return false;
    }

    // ***** CONSULT *****************************************
    else if (strcmp(ptr,"CONSULT") == 0)
    {

        printf("\t[THREAD %p] CONSULT\n",pthread_self());
        // Acces BD
        sprintf(requete_sql,"select * from ARTICLE");

        int idArticle = atoi(strtok(NULL,"#"));
        
        if(mysql_query(connexion,requete_sql) != 0)
        {
            fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
            exit(1);
        }

        if((resultat = mysql_store_result(connexion))==NULL)
        {
            fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
            exit(1);
        }

        while ((ligne = mysql_fetch_row(resultat)) != NULL && atoi(ligne[0]) != idArticle);//recherche du bon article en fct de l'id
                             
        if(ligne != NULL && atoi(ligne[0]) == idArticle)
        {
            sprintf(reponse,"CONSULT#%s#%s#%s#%s#%s",ligne[0],ligne[1],ligne[3],ligne[4],ligne[2]);
        }
        else
        {
            sprintf(reponse,"CONSULT#-1#NotFound");
        }           
    }

    // ***** ACHAT *****************************************
    else if (strcmp(ptr,"ACHAT") == 0)
    {
        printf("\t[THREAD %p] ACHAT\n",pthread_self());

        sprintf(requete_sql,"select * from ARTICLE");
        
        if(mysql_query(connexion,requete_sql) != 0)
        {
            fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
            exit(1);
        }


        if((resultat = mysql_store_result(connexion))==NULL)
        {
            fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
            exit(1);
        }

        int idArticle = atoi(strtok(NULL,"#"));

        int j;
        for(j=0; caddie[j] != NULL && caddie[j]->id != idArticle; j++);



        if(j == 10)
        {
            sprintf(reponse,"ACHAT#0#0#plus de place dans le panier");
            return 1;
        }

        while ((ligne = mysql_fetch_row(resultat)) != NULL && atoi(ligne[0]) != idArticle);//recherche du bon article en fct de l'id

        if(ligne != NULL && atoi(ligne[0]) == idArticle)// est-ce qu'on la trouvé ?           
        {
            int stock=atoi(ligne[3]);
            int quantite = atoi(strtok(NULL,"#"));

            if(quantite<=stock)
            {
                sprintf(requete_sql,"update ARTICLE set stock=%d where id=%d", stock-quantite, atoi(ligne[0]));
                 
                if(mysql_query(connexion,requete_sql) != 0)
                {
                    fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
                    exit(1);
                }

                sprintf(reponse,"ACHAT#%s#%d#%s#%s",ligne[0],quantite,ligne[1],ligne[2]);

                if(caddie[j] != NULL && caddie[j]->id == idArticle)
                {
                    caddie[j]->stock = caddie[j]->stock + quantite;
                }
                else
                {


                    ARTICLE art;

                    art.id = atoi(ligne[0]);
                    strcpy(art.intitule, ligne[1]);
                    art.stock = quantite;
                    strcpy(art.image, ligne[4]);
                    art.prix = atof(ligne[2]);

                    int i;
                    for (i = 0; caddie[i] != NULL && i<10; i++);

                    if(caddie[i] == NULL && i<10)//Vérification inutile mais on sait jamais
                    {
                        caddie[i] = (ARTICLE *)malloc(sizeof(ARTICLE));
                        *caddie[i] = art;
                    }
                }

            }
            else
                sprintf(reponse,"ACHAT#%s#0",ligne[0]);
        }
        else
            sprintf(reponse,"ACHAT#-1");

    }

    // ***** CADDIE *****************************************
    else if (strcmp(ptr,"CADDIE") == 0)
    {
        printf("\t[THREAD %p] CADDIE\n",pthread_self());

        char chaine[500];
        memset(chaine, 0, sizeof(chaine));

        sprintf(reponse,"CADDIE#OK"); // Chaine pour stocker les éléments concaténés
        
        int i;

        for (i = 0; caddie[i] != NULL; i++) {
            sprintf(chaine+ strlen(chaine), "#%d#%s#%d#%s#%.2f", caddie[i]->id, caddie[i]->intitule, caddie[i]->stock, caddie[i]->image, caddie[i]->prix);
        }

        sprintf(reponse+ strlen(reponse), "#%d",i);
        strcat(reponse, chaine);
    }

    // ***** CANCEL *****************************************
    else if (strcmp(ptr,"CANCEL") == 0)
    {
        printf("\t[THREAD %p] CANCEL\n",pthread_self());

        int indice = atoi(strtok(NULL,"#"));

        if(OVESP_Cancel(indice, connexion))
            sprintf(reponse,"CANCEL#OK");
        else
            sprintf(reponse,"CANCEL#BAD");
    }

    // ***** CANCEL ALL *****************************************

    else if (strcmp(ptr,"CANCEL ALL") == 0)
    {
        printf("\t[THREAD %p] CANCEL ALL\n",pthread_self());

        int i;

        while(caddie[0] != NULL)
            OVESP_Cancel(0, connexion);

        sprintf(reponse,"CANCEL ALL#OK");
    }

    //************* CONFIRMER ALL **********************************
    else if(strcmp(ptr, "CONFIRMER") == 0)
    {

        printf("\t[THREAD %p] CONFIRMER\n",pthread_self());

        char user[30];

        strcpy(user,strtok(NULL,"#"));


        OVESP_Confirmer(connexion, user);

        sprintf(reponse, "CONFIRMER#OK");



        

    }

    return true;
}


bool OVESP_Cancel(int indice, MYSQL* connexion)
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;


    sprintf(requete_sql,"select * from ARTICLE");
        
    if(mysql_query(connexion,requete_sql) != 0)
    {
        fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
        exit(1);
    }

    if((resultat = mysql_store_result(connexion))==NULL)
    {
        fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
        exit(1);
    }


    while ((ligne = mysql_fetch_row(resultat)) != NULL && atoi(ligne[0]) != caddie[indice]->id);//recherche du bon article en fct de l'id


    if(ligne != NULL && atoi(ligne[0]) == caddie[indice]->id)// est-ce qu'on la trouvé ?           
    {
        int stock=atoi(ligne[3]);

        sprintf(requete_sql,"update ARTICLE set stock=%d where id=%d", stock+caddie[indice]->stock, caddie[indice]->id);
                 
        if(mysql_query(connexion,requete_sql) != 0)
        {
            fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
            exit(1);
        }

        free(caddie[indice]);

        int j;
        for(j = indice ; j<9 && caddie[j+1] != NULL ; j++)
            caddie[j] = caddie[j+1];

        caddie[j] = NULL;

        return true;
    }
    else
        return false;
}  



bool is_Log_In_BD(char* user, MYSQL* connexion)
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;

    printf("\nuser : \n");

    puts(user);

    sprintf(requete_sql, "SELECT * FROM USER WHERE USERNAME LIKE '%s'", user);

        
    if(mysql_query(connexion,requete_sql) != 0)
    {
        fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
        return false;
    }


    if((resultat = mysql_store_result(connexion))==NULL)
    {
        fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
        return false;
    }


    ligne = mysql_fetch_row(resultat);

    if(ligne == NULL)
    {
        return false;
    }

    char user_bd[200];

    strcpy(user_bd, ligne[1]);

    if(strcmp(user_bd, user)==0)
    {
        return true;
    }


    return false;
}
bool OVESP_Login(char* user, char* password, MYSQL* connexion)
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;
    char password_bd[200];


    sprintf(requete_sql, "SELECT * FROM USER WHERE USERNAME LIKE '%s'", user);

        
    if(mysql_query(connexion,requete_sql) != 0)
    {
        fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
        return false;
    }


    if((resultat = mysql_store_result(connexion))==NULL)
    {
        fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
        return false;
    }


    ligne = mysql_fetch_row(resultat);

    if(ligne == NULL)
    {
        return false;
    }

    printf("\n ligne 1 : %s\n", ligne[1]);
    puts(password);

    strcpy(password_bd, ligne[1]);

    if(strcmp(password_bd, password)==0)
    {
        return true;
    }


    return false;
}

void add_Client_In_BD(char* user, char* password, MYSQL* connexion)
{
    char requete_sql[200];

    sprintf(requete_sql, "INSERT INTO USER (USERNAME, PASSWORD) VALUES ('%s', '%s')", user, password);

        
    if(mysql_query(connexion,requete_sql) != 0)
    {
        fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
    }
}

void OVESP_Confirmer(MYSQL* connexion, char* user)
{
    MYSQL_ROW ligne;
    MYSQL_ROW row;
    char requete_sql[200];
    MYSQL_RES  *resultat;

    int idClient = -1, idFacture = -1;
    float montant = 0;

    sprintf(requete_sql, "SELECT * FROM USER WHERE USERNAME LIKE '%s'", user);




    if(mysql_query(connexion,requete_sql) != 0)
    {
        fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
        exit(1);
    }


    if((resultat = mysql_store_result(connexion))==NULL)
    {
        fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
        exit(1);
    }

    ligne = mysql_fetch_row(resultat);

    idClient = atoi(ligne[0]);

    
    for(int i=0; caddie[i] != NULL; i++)
    {
        montant = montant + caddie[i]->prix * caddie[i]->stock;
    }


    sprintf(requete_sql, "INSERT INTO FACTURE (ID_CLIENT, MONTANT) VALUES (%d, %f)", idClient, montant);




    if(mysql_query(connexion,requete_sql) != 0)
    {
        fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
        exit(1);
    }

        if (mysql_query(connexion, "SELECT ID FROM FACTURE ORDER BY ID DESC LIMIT 1") != 0) {
        fprintf(stderr, "Erreur lors de la récupération de la colonne : %s\n", mysql_error(connexion));
        mysql_close(connexion);
        exit(1);
    }


    resultat = mysql_store_result(connexion);
    if (resultat) {
        row = mysql_fetch_row(resultat);
        idFacture = atoi(row[0]);
        
    }

    printf("idFacture = %d\n", idFacture);










    while(caddie[0] != NULL)
    {

        sprintf(requete_sql, "INSERT INTO ARTICLE_FACTURE VALUES (%d, %d, %d)",idFacture , caddie[0]->id, caddie[0]->stock);


        if(mysql_query(connexion,requete_sql) != 0)
        {
            fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
            exit(1);
        }
        



        //decale
        free(caddie[0]);

        int j;
        for(j = 0 ; j<9 && caddie[j+1] != NULL ; j++)
            caddie[j] = caddie[j+1];

        caddie[j] = NULL;
    }
    
}
