#include "OVESP.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <mysql.h>




bool is_Log_In_BD(char* user);
bool OVESP_Login(char* user, char* password);
void add_Client_In_BD(char* user, char* password);

void OVESP_Consult(int idArticle, ARTICLE* art);

void OVESP_Achat(int idArticle, int quantite, ARTICLE* art);

int OVESP_Caddie(ARTICLE** panier);

bool OVESP_Cancel(int idArticle);

void OVESP_CancelAll();

void OVESP_Confirmer(char*);


void addUserToSpecific(char user[]);

int isCaddieExist(int idClient);

int createCaddie(int idClient);

bool isCaddieFull(int idClient);




ARTICLE ** caddie;




//***** Parsing de la requete et creation de la reponse *************
bool OVESP(char* requete, char* reponse,int socket, ARTICLE** cadd)
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
            if (OVESP_Login(user,password))
            {
                sprintf(reponse,"LOGIN#OK#Client Log");
                addUserToSpecific(user);
            } 
            else
            {
                if(is_Log_In_BD(user))
                    sprintf(reponse,"LOGIN#BAD#Mauvais mot de passe !");
                else
                    sprintf(reponse,"LOGIN#BAD#Client n'existe pas !");

                return false;
            }
        }
        else if (!is_Log_In_BD(user)) // New Client qui n'est pas dejà dans la BD
        {
            add_Client_In_BD(user, password);
            sprintf(reponse,"LOGIN#OK#Nouveau Client");
            addUserToSpecific(user);
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

        void *userData = pthread_getspecific(cle);
        free(userData);

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
	    int idClient = *(int*)pthread_getspecific(cle);

        int idFacture;

        if(isCaddieFull(idClient))
        {
            {
                sprintf(reponse,"ACHAT#0#0#plus de place dans le panier");
                return 1;
            }
        }

        idFacture = isCaddieExist(idClient);


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

                sprintf(requete_sql, "INSERT INTO ARTICLE_FACTURE VALUES(%d,%d,%d)", idFacture, idArticle, quantite);

                if(mysql_query(connexion,requete_sql) != 0)
                {
                    fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
                    exit(1);
                }

                sprintf(reponse,"ACHAT#%s#%d#%s#%s",ligne[0],quantite,ligne[1],ligne[2]);

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

        int idClient = *(int*)pthread_getspecific(cle);

        int idFacture = isCaddieExist(idClient);

        if(idFacture == -1)
        {
            idFacture = createCaddie(idClient);
        }

        printf("\nClient : %d\n Facture : %d", idClient, idFacture);


        sprintf(requete_sql, "SELECT * FROM ARTICLE_FACTURE JOIN FACTURE ON ARTICLE_FACTURE.ID_FACTURE = FACTURE.ID WHERE FACTURE.CADDIE is true AND FACTURE.ID_CLIENT = %d", idClient);

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

        int i=0;

        printf("\n yo1\n");


        while((ligne = mysql_fetch_row(resultat)) != NULL)
        {

            MYSQL_ROW ligne2;
            MYSQL_RES  *resultat2;

            printf("\nClient : %d\n Facture : %d", idClient, idFacture);


            int idArticle = atoi(ligne[1]);
            int quantite = atof(ligne[2]);

            //

            sprintf(requete_sql,"SELECT * FROM article join ARTICLE_FACTURE AF on article.ID = AF.ID_ARTICLE WHERE article.ID = %d AND AF.ID_FACTURE = %d", idArticle, idFacture);

            if(mysql_query(connexion,requete_sql) != 0)
            {
                fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
                exit(1);
            }


            if((resultat2 = mysql_store_result(connexion))==NULL)
            {
                fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
                exit(1);
            }

            ligne2 = mysql_fetch_row(resultat2);

            char intitule[30];
            char image[100];
            float prix= -1.0;

            strcpy(intitule, ligne2[1]);
            strcpy(image, ligne2[4]);
            prix = atof(ligne2[2]);



            sprintf(chaine+ strlen(chaine), "#%d#%s#%d#%s#%.2f", idArticle, intitule, quantite, image, prix);


            i++;
        }

        printf("\n yo2\n");



        sprintf(reponse+ strlen(reponse), "#%d",i);
        strcat(reponse, chaine);

        puts(reponse);
    }

    // ***** CANCEL *****************************************
    else if (strcmp(ptr,"CANCEL") == 0)
    {
        printf("\t[THREAD %p] CANCEL\n",pthread_self());

        int indice = atoi(strtok(NULL,"#"));

        if(OVESP_Cancel(indice))
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
            OVESP_Cancel(0);

        sprintf(reponse,"CANCEL ALL#OK");
    }

    //************* CONFIRMER ALL **********************************
    else if(strcmp(ptr, "CONFIRMER") == 0)
    {

        printf("\t[THREAD %p] CONFIRMER\n",pthread_self());

        char user[30];

        strcpy(user,strtok(NULL,"#"));


        OVESP_Confirmer(user);

        sprintf(reponse, "CONFIRMER#OK");



        

    }

    return true;
}


bool OVESP_Cancel(int indice)
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



bool is_Log_In_BD(char* user)
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;

    sprintf(requete_sql, "SELECT * FROM CLIENT WHERE USERNAME LIKE '%s'", user);

        
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
bool OVESP_Login(char* user, char* password)
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;
    char password_bd[200];


    sprintf(requete_sql, "SELECT * FROM CLIENT WHERE USERNAME LIKE '%s'", user);

        
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

    strcpy(password_bd, ligne[1]);

    if(strcmp(password_bd, password)==0)
    {
        return true;
    }


    return false;
}

void add_Client_In_BD(char* user, char* password)
{
    char requete_sql[200];

    sprintf(requete_sql, "INSERT INTO CLIENT (USERNAME, PASSWORD) VALUES ('%s', '%s')", user, password);

        
    if(mysql_query(connexion,requete_sql) != 0)
    {
        fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
    }
}


void OVESP_Confirmer(char* user)
{
    MYSQL_ROW ligne;
    MYSQL_ROW row;
    char requete_sql[200];
    MYSQL_RES  *resultat;

    int idClient = -1, idFacture = -1;
    float montant = 0;

    sprintf(requete_sql, "SELECT * FROM CLIENT WHERE USERNAME LIKE '%s'", user);




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

void addUserToSpecific(char user[])
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;

    int idClient = -1;

    sprintf(requete_sql, "SELECT * FROM CLIENT WHERE USERNAME LIKE '%s'", user);


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

    if(idClient == -1)
    {
        printf("Probleme lors de la recuperation de l'id du client");
        exit(1);
    }


    int *pIdClient = (int*) malloc(sizeof(int));

    if (pIdClient == NULL)
    {
        printf("Allocation de la variable spécifique a échoué");
        exit(1);
    }

    *pIdClient = idClient;

    pthread_setspecific(cle, (const void*)pIdClient);



}



int isCaddieExist(int idClient)
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;

    sprintf(requete_sql, "SELECT * FROM FACTURE WHERE ID_CLIENT = %d AND CADDIE is TRUE", idClient);

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


    if((ligne = mysql_fetch_row(resultat)) == NULL)
    {
        return -1;
    }

    return atoi(ligne[0]);

}

int createCaddie(int idClient)
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;

    sprintf(requete_sql, "INSERT INTO FACTURE (ID_CLIENT, MONTANT, CADDIE) VALUES (%d, 0, TRUE) ", idClient);

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

    if((resultat = mysql_store_result(connexion))==NULL)
    {
        fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
        exit(1);
    }

    ligne = mysql_fetch_row(resultat);

    return atoi(ligne[0]);




}


bool isCaddieFull(int idClient)
{
    MYSQL_ROW ligne;
    char requete_sql[200];
    MYSQL_RES  *resultat;

    sprintf(requete_sql, "SELECT * FROM FACTURE WHERE ID_CLIENT = %d AND CADDIE is TRUE ", idClient);

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

    int idFacture = atoi(ligne[0]);

    sprintf(requete_sql, "SELECT * FROM ARTICLE_FACTURE WHERE ID_FACTURE = %d ", idFacture);


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



    int i = 0;

    while((ligne = mysql_fetch_row(resultat)) != NULL)
        i++;

    if(i == 10)
        return true;


    return false;
    


}