/* questo file contiene tutte le funzioni utili alla gestione dei clients da parte del server */


#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */
#include <unistd.h>           /*  misc. UNIX functions      */
#include "helper.h"           /*  our own helper functions  */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <errno.h>
#include <sys/sem.h>
#include "manage_client.h"

#define MAX_LINE (1000)

int crea_chat(int sockd,clients *new_cl,chats **new_chat,chats **chat_head,key_t key){
    chats *_index;
    char buffer[MAX_LINE];
    int sid;
    int j;
	Readline(sockd,buffer,MAX_LINE-1);
	if(strcmp(buffer, "quit\n") == 0){
		return -1;
	}
	_index = *chat_head;
	while(_index != NULL)
	{
	    if(strcmp(_index->chat_name,buffer) == 0) //caso in cui la chat è già esistente
		{
		    write(sockd,"Errore",7);
			if(close(sockd) == -1)
			{
				printf("Errore close !\n");
				exit(-1);
			}
			return -1;
		}
		_index = _index->n_chat;
	}
	write(sockd,"Ok",3);
	new_cl->conn_s = sockd;
	sid = semget(key,1,IPC_CREAT | 0666);
	if(sid == -1)
	{
		printf("Thread : errore semget \n");
		return -1;
	}
	if(semctl(sid,0,SETVAL,0) == -1)
	{
		printf("Thread : errore semctl \n");
		return -1;
	}
    (*new_chat)->sid = sid;
	for(j = 0;j < strlen(buffer);j++) // copia del nome della chat mantenuta dal thread
	{
		(*new_chat)->chat_name[j] = buffer[j];	
		new_cl->my_chat[j] = buffer[j];
	}
    /* inserimento della nuova chat in lista */
	if((*chat_head) == NULL)
	{
		*chat_head = *new_chat;
	}
	else
	{
		_index = *chat_head;
		while(_index->n_chat != NULL)
		{
			_index = _index->n_chat;
		}
		_index->n_chat = *new_chat;
		(*new_chat)->p_chat = _index;
	}
    return 0;
}


int check_insert(int sockd,chats *chat_head,clients *new_cl){
    char buffer[MAX_LINE];
    Readline(sockd,buffer,MAX_LINE-1);
	if(strcmp(buffer, "quit\n") == 0){
		return -1;
	}
	chats *n_ind;
	n_ind = chat_head;
	while(n_ind != NULL)
	{
		if(strcmp(n_ind->chat_name,buffer) == 0)
		{
			int i;
			new_cl->conn_s = sockd;
			for(i = 0;i < strlen(buffer);i++)
			{
				new_cl->my_chat[i] = buffer[i];
			}
			memset(buffer,0,sizeof(char)*(strlen(buffer)+1));
			write(sockd,"Ok",3);
			Readline(sockd,buffer,MAX_LINE-1);
			if(strcmp(buffer, "quit\n") == 0){
				return -1;
			}
			printf("Nuovo utente connesso : %s",buffer);
			for(i = 0;i < strlen(buffer);i++)
			{
				new_cl->who[i] = buffer[i];
			}
			memset(buffer,0,sizeof(char)*(strlen(buffer)+1));
			return n_ind->sid;
        }
		n_ind = n_ind->n_chat;	
	}
	write(sockd,"Errore",7);
	return -1;
}


void insert_user(clients **client_head,clients **new_client){
	clients *index;
	index = *client_head;
	while(index->next != NULL)
	{
		index = index->next;
	}
	index->next = *new_client;
	(*new_client)->previous = index;
	(*new_client)->next = NULL;
}


int delete_user(clients **my_t){
	if((*my_t)->previous == NULL && (*my_t)->next == NULL) //uscita dell'ultimo utente dalla chat
	{	
		return 1;	
	}
	char adios[50];
	strcpy(adios,"E' uscito l'utente :");
	strcat(adios,(*my_t)->who);
	clients *index;
	index = (*my_t);
	while(index->previous != NULL)
	{
		index = index->previous;
	}
	while(index != NULL)
	{
		if(index->conn_s != (*my_t)->conn_s)
		{
			Writeline(index->conn_s,adios,(strlen(adios)));
		}
		index = index->next;	
	}
	memset(adios,0,sizeof(char)*(strlen(adios)+1));
	if(close((*my_t)->conn_s) == -1)
	{
		printf("Errore nella close dell'utente %s",(*my_t)->who);
		exit(-1);
	}
	if((*my_t)->next == NULL) //esce l'ultimo utente
	{
		index = (*my_t)->previous;
		index->next = NULL;
		return 0;
	}
	else
	{
		clients *index2;
		index = (*my_t)->previous;
		index2 = (*my_t)->next;
		index->next = index2;
		index2->previous = index;
		return 0;	
	}
return 0;	
}


int delete_chat(chats **chat_head,char *chat_name){
	if((*chat_head)->n_chat == NULL && (*chat_head)->p_chat == NULL) //unica chat attiva
	{
		(*chat_head) = NULL;
		return 0;
	}
	chats *index;
	index = (*chat_head);
	while(index != NULL)
	{
		if(strcmp(index->chat_name,chat_name) == 0)
		{
			if(index->p_chat == NULL)
			{
				(*chat_head) = (*chat_head)->n_chat;
				(*chat_head)->p_chat = NULL;
				free(index);
				return 0;
			}
			else if(index->n_chat == NULL)
			{
				chats *index2;
				index2 = index->p_chat;
				index2->n_chat = NULL;
				free(index);
				return 0;
			}
			else
			{
				chats *index2;
				chats *index3;
				index2 = index->p_chat;
				index3 = index->n_chat;
				index2->n_chat = index3;
				index3->p_chat = index2;
				free(index);
				return 0;
			}
		}
		index = index->n_chat;	
	}
return 0;	
}


void send_close_message(clients *my_t){
	clients *index;
	index = my_t;
	while(index != NULL){
		Writeline(index->conn_s, "end_chat", 9);
		strcpy(index->close_msg, "end_chat");
		index = index->next;
	}
}


void free_list(clients **cl_head){
	clients *index;
	clients *free_index;
	index = *cl_head;
	while(index != NULL){
		free_index = index;
		index = index->next;
		free(free_index);
	}
}

void close_users(clients *my_t){
	clients *ind;
	ind = my_t->next;
	while(ind != NULL){
		strcpy(ind->close_msg, "end_chat");
		if(close(my_t->conn_s) == -1){
			printf("Errore closee \n");
			exit(-1);
		}
		//Writeline(ind->conn_s, "end_chat", 9);
		ind = ind->next;
	}
}

void print_names(int sockd, chats *chat_head){
	chats *index = chat_head;
	char buffer[MAX_LINE];
	if(index == NULL)
	{
		Writeline(sockd,"Attualmente non ci sono chat attive\n",37);
	}
	while(index != NULL)
	{
		strcat(buffer,index->chat_name);
		strcat(buffer,"\n");
	index = index->n_chat;	
	}
	Writeline(sockd,buffer,strlen(buffer));
	memset(buffer,0,sizeof(char)*(strlen(buffer)+1));	
}

void get_token(int sem_key,int n_sem,int val){
	struct sembuf _sembuf;
	_sembuf.sem_num = n_sem;
	_sembuf.sem_op = -val;
	_sembuf.sem_flg = 0;
	if(semop(sem_key, &_sembuf,1) == -1)
	{
		printf("Thread : errore semop \n");
		exit(-1);
	}
}

void set_token(int sem_key,int n_sem,int val){
	struct sembuf _sembuf;
	_sembuf.sem_num = n_sem;
	_sembuf.sem_op = val;
	_sembuf.sem_flg = 0;
	if(semop(sem_key, &_sembuf,1) == -1)
	{
		printf("Thread : errore semop \n");
		exit(-1);
	}
}