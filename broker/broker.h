
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
	char* ip_broker;
	char* puerto_broker;
	uint32_t tamanio_memoria;
	uint32_t tamanio_min_compactacion;
	char* algoritmo_memoria;
	char* algoritmo_reemplazo;
	char* algoritmo_particion_libre;
	uint32_t frecuencia_compactacion;
}config_broker;


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

typedef struct _cache{
	bool libre;
	uint32_t size;
	uint32_t id_particion;
	uint32_t id_buffer;
	uint32_t tipo_cola;
	void* inicio;
	void* fin;
	time_t tiempo;
}particion;

void iniciar_config_broker(char* broker_config);
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
void * recibir_mensaje(int socket_cliente);
void atender_suscripcion(int cliente_fd );
void atender_ack(int cliente_fd);
void enviar_cacheados(suscriber* sus, int queue_id);
t_paquete* crear_paquete(int op);
void enviar_id(queue_item *item,int socket_cliente);

//--------------------------------------------------------------------------------------------------------

void iniciar_memoria();
t_config* leer_config(char* config);
void iniciar_config(char* broker_config);
void iniciar_semaforos();
void iniciar_cache();
particion* malloc_cache(size_t);
void display_cache();
void handler_dump(int signo);
void free_cache();
void* memcpy_cache(particion*,uint32_t id_buf,uint32_t tipo_cola,void* destion,void* buf,uint32_t);
void compactar_cache();
particion* algoritmo_particion_libre(uint32_t size);
particion* particiones_dinamicas(uint32_t size);
uint32_t calcular_size_potencia_dos(uint32_t size);
uint32_t log_dos(uint32_t size);
particion* algoritmo_reemplazo();
particion* buddy_system(uint32_t size);
void delete_particion(particion* borrar);
void limpiar_cache();
#endif /* CONEXIONES_H_ */
