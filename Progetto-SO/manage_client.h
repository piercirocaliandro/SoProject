#include <unistd.h>

typedef struct actived_client {
	char who[40];
	char my_chat[40];
	long conn_s;
	struct actived_client *next;
	struct actived_client *previous;
	char msg[4096];
	char close_msg[15];
} clients;

typedef struct active_chats
{
	char chat_name[40];
	int sid;
	struct active_chats *n_chat;
	struct active_chats *p_chat;
	char close_msg[14];
}chats;



int crea_chat(int sockd,clients *new_cl,chats **new_chat,chats **chat_head,key_t key);
void print_names(int sockd, chats *chat_head);
int check_insert(int sockd,chats *chat_head,clients *new_cl);
void insert_user(clients **client_head,clients **new_client);
int delete_user(clients **my_t);
int delete_chat(chats **chat_head,char *chat_name);
void send_close_message(clients *my_t);
void free_list(clients **cl_head);
void close_users(clients *my_t);