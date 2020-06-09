
#ifndef CONEXIONES_H_
#define CONEXIONES_H_

#include"../utils/utils.c"
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
#include<semaphore.h>



typedef struct{
	int cliente_fd;
	int pid;
}suscriber;

typedef struct{
    int id;
    int correlation_id;
    void* message;
	t_list* enviados; //ACA SOLO METO PIDS
	t_list* recibidos; //IDEM ENVIADOS
}queue_item;

t_list* queues[6];
t_list* suscribers[6];

t_log* logger;
t_config* config;
pthread_t thread;

sem_t semSend;




void terminar_broker(t_log* logger, t_config* config);
void iniciar_broker(void);
int get_id(void);
int generate_pid(void);
void build_queues(void);
void build_suscribers(void);
void start_sender_thread(void);
void sender_thread(void);
void iniciar_servidor(void);
void esperar_cliente(int socket_servidor);
void serve_client(int* socket);
void process_request(int cod_op, int cliente_fd);
void * recibir_mensaje(int socket_cliente, int* size);
void atender_suscripcion(int cliente_fd, void * msg );
void atender_ack(int cliente_fd, void * msg);
void enviar_cacheados(suscriber* sus, int queue_id);
t_paquete* crear_paquete(int op, queue_item* queue_item);
#endif /* CONEXIONES_H_ */
