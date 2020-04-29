#include"../utils/utils.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<commons/log.h>
#include<math.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/collections/list.h>
#include<commons/collections/queue.h>
#include<commons/config.h>
#include<commons/string.h>
#include<pthread.h>
#include<assert.h>
#include<signal.h>

#define IP "127.0.0.1"
#define PUERTO "4444"

typedef struct{
	bool ack;
	int id;
}sent;

typedef struct{
	int cliente_fd;
	sent * sended;
}suscriber;

typedef struct{
    int id;
    int correlation_id;
    void* message;
}queue_item;

t_queue * queues[6];

suscriber * suscribers[6];
t_log* logger;
t_config* config;
pthread_t thread;

void terminar_broker(t_log* logger, t_config* config);
void iniciar_broker(void);
void* serializar_paquete(t_paquete* paquete, int * bytes);
int get_id(void);
int get_correlation_id(void);
void build_queues(void);
void build_suscribers(void);
void start_sender_thread(void);
void sender_thread(void);