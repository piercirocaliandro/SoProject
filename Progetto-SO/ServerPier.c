/*

  TCPSERVER.C
  ==========

*/
/*please compile with -pthread */


#include <sys/socket.h>       /*  socket definitions        */
#include <sys/types.h>        /*  socket types              */
#include <arpa/inet.h>        /*  inet (3) funtions         */
#include <unistd.h>           /*  misc. UNIX functions      */

#include "helper.h"           /*  our own helper functions  */
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include "manage_client.h"
		

#define MAX_LINE	1000
struct    sockaddr_in servaddr;  /*  socket address structure  */
struct	  sockaddr_in their_addr;
#define MAX_CHAT	(10000);
chats *chat_head;
clients *mem; //usata per passare i nuovi membri della chat al thread corretto
key_t key = 1234;
int gen_id;
int num_chats;
int ParseCmdLine(int argc, char *argv[], char **szPort);

void *gestisci_user(void *argu);
void *thread_chat(void *arg);

void *evadi_richiesta(void *arg){
	char buffer[MAX_LINE];      /*  character buffer          */
	long conn_s = (long)arg;
	struct sembuf _sembuf;
	/* connessione col primo utente */
	printf("server: connessione da %s\n", inet_ntoa(their_addr.sin_addr));
	int j;
	clients *new_cl = malloc(sizeof(clients)); //nuovo client,che gestisce la testa
	chats *new_chat = malloc(sizeof(chats));
	new_chat->p_chat = NULL;
	new_chat->n_chat = NULL;
	new_cl->next = NULL;
	new_cl->previous = NULL;
	int ret;
	get_token(gen_id,0,1);
	Readline(conn_s,buffer,MAX_LINE-1);
	if(strcmp(buffer, "quit\n") == 0){
		printf("Failed \n");
		free(new_chat);
		free(new_cl);
		set_token(gen_id,0,1);
	}					
	if(strcmp(buffer, "create\n") == 0) //utente richiede di creare una nuova chat
	{
		printf("Richiesta di create\n");
		print_names(conn_s,chat_head);
		ret = crea_chat(conn_s,new_cl,&new_chat,&chat_head,key);
		key += 1;
		if(ret == -1)
		{
			printf("Failed \n");
			free(new_chat);
			free(new_cl);
			set_token(gen_id,0,1);
		}
		printf("Nuova chat creata : %s",new_chat->chat_name);
		num_chats += 1;
		memset(buffer,0,sizeof(char)*(strlen(buffer)+1));
		Readline(conn_s,buffer,MAX_LINE-1);
		if(strcmp(buffer, "quit\n") == 0){
			printf("Disconnessione %lu",conn_s);
			free(new_chat);
			free(new_cl);
			num_chats -= 1;
			set_token(gen_id,0,1);
			pthread_exit(NULL);
		}
		for(j = 0;j < strlen(buffer);j++)
		{
			new_cl->who[j] = buffer[j];
		}
		printf("Utente connesso : %s",new_cl->who);
		pthread_t tid;
		if(pthread_create(&tid,NULL,(void*)thread_chat, (void *)new_cl) != 0)
		{
			printf("Errore pthread create \n");
			exit(-1);
		}
		set_token(gen_id,0,1);
		memset(buffer,0,sizeof(char)*(strlen(buffer)+1));
		pthread_exit(NULL);
	}		
	else if(strcmp(buffer, "join\n") == 0) //utente chiede join di una chat
	{
		if(num_chats == 0){
			write(conn_s, "Err",4);
			set_token(gen_id,0,1);
			pthread_exit(NULL);
		}
		print_names(conn_s,chat_head);
		printf("Richiesta di join \n");
		memset(buffer,0,sizeof(char)*(strlen(buffer)+1));
		ret = check_insert(conn_s,chat_head,new_cl);
		if(ret != -1)
		{
			mem = new_cl;
			set_token(ret,0,1);
			get_token(gen_id,1,1); //il thread aspetta che il client sia inserito nella chat corretta,poi rifa la signal
			set_token(gen_id,0,1);
		}
		else if(ret == -1){
			printf("Disconnessione %lu",conn_s);
			free(new_chat);
			free(new_cl);
			set_token(gen_id,0,1);
			pthread_exit(NULL);
		}
		pthread_exit(NULL);
	}
}

void *thread_chat(void *arg){
	printf("Nuovo gestore di chat attivato \n");
	char my_chat[40];
	struct sembuf _sembuf;
	clients *cl_head;
	cl_head = (clients *)arg;
	pthread_t _tid;
	int sid;
	int i;
	chats *chat_index;
	chats *this_chat;
	chat_index = chat_head;
	while(chat_index != NULL)
	{
		if(strcmp(cl_head->my_chat,chat_index->chat_name) == 0)
		{
			sid = chat_index->sid;
			this_chat = chat_index;
		}	
	chat_index = chat_index->n_chat;
	}
	if(pthread_create(&_tid,NULL,(void*)gestisci_user,(void*)cl_head) != 0)
	{
		printf("Thread gestore : errore create \n");
		pthread_exit(NULL);
	}
	while(1)
	{
		get_token(sid,0,1);
		if(strcmp(cl_head->close_msg, "head_quit") == 0){
			if(cl_head->next == NULL){ //va cancellata la chat
				delete_chat(&chat_head, my_chat);
				if(close(cl_head->conn_s) == -1){
					printf("Errore close ! \n");
					pthread_exit(NULL);
				}
				num_chats -= 1;
				free(cl_head);
				printf("E' stata chiusa la chat %s",cl_head->my_chat);
				pthread_exit(NULL);
			}
			else{
				char buffer[100];
				strcpy(buffer,"E' uscito l'utente : ");
				strcat(buffer,cl_head->who);
				strcat(buffer,"Il nuovo admin è :");
				strcat(buffer,(cl_head->next)->who);
				clients *index;
				index = cl_head->next;
				while(index != NULL){
					Writeline(index->conn_s,buffer,strlen(buffer));
					index = index->next;
				}
				memset(buffer,0,sizeof(char)*(strlen(buffer)+1));
				cl_head = cl_head->next;
				cl_head->previous = NULL;
			}
		}
		else if(strcmp(this_chat->close_msg, "server_close") == 0){
			send_close_message(cl_head);
			free_list(&cl_head);
			set_token(gen_id,0,1);			
		}
		else if(strcmp(cl_head->close_msg, "close") == 0)
		{
			num_chats -= 1;
			//Writeline(cl_head->conn_s,"end_chat",9);
			/* if(close(cl_head->conn_s) == -1)
			{
				printf("Errore close #1 \n");
				pthread_exit(NULL);
			}*/
			printf("E' stata chiusa la chat %s",cl_head->my_chat);
			free(cl_head);
			pthread_exit(NULL);
		}
		else if(mem != NULL)
		{
			printf("Inserimento\n");
			clients *new_cl = malloc(sizeof(clients));
			new_cl = mem;
			insert_user(&cl_head,&new_cl);		
			if(pthread_create(&_tid,NULL,(void*)gestisci_user,(void*)new_cl) != 0)
			{
				printf("Errore creazione thread figlio \n");
				pthread_exit(NULL);
			}
			set_token(gen_id,1,1);
			mem = NULL;
		}
	}
}

void *gestisci_user(void *argu){
	clients *my_t = malloc(sizeof(clients));
	my_t = (clients *)(argu); //typecast
	printf("Attivato il gestore del client %s",my_t->who);
	clients *index;
	struct sembuf _sembuf;
	int sem_id;
	char send_name[60];
	int k;
	int conn_s = my_t->conn_s;
	chats *chat_index;
	int ret;
	chat_index = chat_head;
	while(chat_index != NULL)
	{
		if(strcmp(chat_index->chat_name,my_t->my_chat) == 0)
		{
			sem_id = chat_index->sid;
			break;
		}
	chat_index = chat_index->n_chat;
	}
	for(k = 0;my_t->who[k] != '\n';k++)
	{
		send_name[k] = my_t->who[k];
	}
	strcat(send_name,": ");
	while(1){
		Readline(my_t->conn_s,my_t->msg,MAX_LINE-1);
		if(strcmp(my_t->msg, "quit\n") == 0){
			get_token(gen_id,0,1);
			memset(send_name,0,sizeof(char)*(strlen(send_name)+1));
			if(my_t->previous == NULL){
				set_token(sem_id,0,1);
				strcpy(my_t->close_msg, "head_quit");
				set_token(gen_id,0,1);
				pthread_exit(NULL);
			}
			if(my_t->previous == NULL && my_t->next == NULL){
				strcpy(my_t->close_msg, "close");
				delete_chat(&chat_head, my_t->my_chat);
				set_token(gen_id,0,1);
			}
			else{
				ret = delete_user(&my_t);
				if(ret == 1)
				{
					delete_chat(&chat_head,my_t->my_chat);
					strcpy(my_t->close_msg, "close");
					set_token(sem_id,0,1);			
				}
				else{
					memset(my_t->who,0,sizeof(char)*(strlen(my_t->who)+1));
					memset(my_t->my_chat,0,sizeof(char)*(strlen(my_t->my_chat)+1));
					free(my_t);
				}
				set_token(gen_id,0,1);
				pthread_exit(NULL);
			}
		}
		else if(strcmp(my_t->msg, "close\n") == 0 && my_t->previous == NULL){
			get_token(gen_id,0,1);
			memset(send_name,0,sizeof(char)*(strlen(send_name)+1));
			delete_chat(&chat_head, my_t->my_chat);
			close_users(my_t);
			strcpy(my_t->close_msg, "close");
			set_token(gen_id,0,1);
			set_token(sem_id,0,1);		
			pthread_exit(NULL);
		}
		if(strcmp(my_t->close_msg, "end_chat") == 0){
			if(close(conn_s) == -1)
			{
				printf("Thread : errore close \n");
				pthread_exit(NULL);
			}
			memset(send_name,0,sizeof(char)*(strlen(send_name)+1));
			free(my_t);
			set_token(gen_id,0,1);	
			pthread_exit(NULL);
		}
		index = my_t;
		while(index->previous != NULL)
		{
			index = index->previous;
		}
		while(index != NULL)
		{
			if(index->conn_s != my_t->conn_s)
			{
				Writeline(index->conn_s,send_name,strlen(send_name));
				Writeline(index->conn_s,my_t->msg,strlen(my_t->msg));
			}
		index = index->next;	
		}
	memset(my_t->msg,0,sizeof(char)*(strlen(my_t->msg)+1));
	}
}

void close_handler(int sigo){
	printf("\nChiusura server...\n");
	if(num_chats == 0)
	{
		printf("Uscita ! \n");
		exit(0);
	}
	struct sembuf _sembuf;
	chats * index;
	int sid;
	get_token(gen_id,0,1);
	chats *free_index;
	printf("Segnalazione di chiusura a tutti i client connessi... \n");
	index = chat_head;
	while(index != NULL)
	{
		sid = index->sid;
		strcpy(index->close_msg,"server_close");
		free_index = index;
		index = index->n_chat;
		free(free_index);
		set_token(sid,0,1);
	}
	get_token(gen_id,0,num_chats);
	printf("Uscita ! \n");
	exit(0);
}

int main(int argc, char *argv[]) {
	
	pthread_t tid;
	char     *endptr;                /*  for strtol()              */
    int 	  sin_size;   
    short int port;                  /*  port number               */
	int       list_s;                /*  listening socket          */
	char buffer[MAX_LINE];
	/*  Get command line arguments  */
	
	ParseCmdLine(argc, argv, &endptr);
	port = strtol(endptr, &endptr, 0);
	if ( *endptr )
	{
	    fprintf(stderr, "server: porta non riconosciuta.\n");
	    exit(EXIT_FAILURE);
	}
    
	printf("Server in ascolto sulla porta %d\n",port);
	
	/*  Create the listening socket  */

    if ( (list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
	fprintf(stderr, "server: errore nella creazione della socket.\n");
	exit(EXIT_FAILURE);
    }


    /*  Set all bytes in socket address structure to
        zero, and fill in the relevant data members   */

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);

    /*  Bind our socket addresss to the 
	listening socket, and call listen()  */

    if ( bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
		fprintf(stderr, "server: errore durante la bind.\n");
		exit(EXIT_FAILURE);
    }

    if ( listen(list_s, LISTENQ) < 0 ) {
		fprintf(stderr, "server: errore durante la listen.\n");
		exit(EXIT_FAILURE);
    }

    
    /*  Enter an infinite loop to respond to client requests  */
    
	long       conn_s;                /*  connection socket         */
	mem = malloc(sizeof(clients));
	mem = NULL;
	chat_head = malloc(sizeof(chats));
	chat_head = NULL;
	chats *chat_head = malloc(sizeof(chats));
	chat_head = NULL;
	/* Il semaforo ha due token : visto l'accesso concorrente di molti client,il secondo token 
	serve per segnalare al thread evadi_richiesta che l'utente è stato aggiunto alla chat corretta */
	gen_id = semget(IPC_PRIVATE,2,IPC_CREAT | 0666);
	if(gen_id == -1)
	{
		printf("Errore semget \n");
		exit(-1);
	}
	if(semctl(gen_id,0,SETVAL,1) == -1)
	{
		printf("Errore semctl \n");
		exit(-1);
	}
	if(semctl(gen_id,1,SETVAL,0) == -1){
		printf("Errore sul secondo token in semctl \n");
		exit(-1);
	}
	signal(SIGINT,close_handler);
    while ( 1 ) {
		chats *new_chat = malloc(sizeof(chats));
		new_chat->n_chat = NULL;
		/*  Wait for a connection, then accept() it  */
		sin_size = sizeof(struct sockaddr_in);
		if ( (conn_s = accept(list_s, (struct sockaddr *)&their_addr, &sin_size) ) < 0 ) {
		    fprintf(stderr, "server: errore nella accept.\n");
	    	    exit(EXIT_FAILURE);
		}
		if(pthread_create(&tid,NULL,(void*)evadi_richiesta,(void*)conn_s) != 0)
		{
			printf("Errore server : impossibile creare il thread \n");
			exit(-1);
		}
	}
}


int ParseCmdLine(int argc, char *argv[], char **szPort)
{
    int n = 1;

    while ( n < argc )
	{
		if ( !strncmp(argv[n], "-p", 2) || !strncmp(argv[n], "-P", 2) )
			*szPort = argv[++n];
		else 
			if ( !strncmp(argv[n], "-h", 2) || !strncmp(argv[n], "-H", 2) ) {
			    printf("Sintassi:\n\n");
	    		    printf("    server -p (porta) [-h]\n\n");
			    exit(EXIT_SUCCESS);
			}
		++n;
    }
    if (argc==1) {
	printf("Sintassi:\n\n");
    	printf("    server -p (porta) [-h]\n\n");
	exit(EXIT_SUCCESS);
    }
    return 0;
}
