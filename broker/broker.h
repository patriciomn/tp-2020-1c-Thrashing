#ifndef BROKER_H_
#define BROKER_H_

#include"utils.h"
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
	uint32_t tamanio_min_particion;
	char* algoritmo_memoria;
	char* algoritmo_reemplazo;
	char* algoritmo_particion_libre;
	uint32_t frecuencia_compactacion;
	char* log_file;
}config_broker;

typedef struct{
	int cliente_fd;
	pid_t pid;
}suscriber;

typedef struct{
	suscriber* sus;
	int cod_op;
}parametros;

typedef struct _cache{
	uint32_t libre;
	uint32_t size;
	uint32_t buddy_size;
	uint32_t id_particion;
	uint32_t id_mensaje;
	uint32_t tipo_cola;
	void* inicio;
	void* fin;
	int start;
	int end;
	time_t tiempo_inicial;
	time_t tiempo_actual;
	int intervalo;
}particion;

typedef struct{
	uint32_t id;
	uint32_t id_correlacional;
	int tipo_msg;
	t_list* suscriptors_enviados;
	t_list* suscriptors_ack;
	void* msj;
}mensaje;

typedef struct{
	uint32_t tipo_queue;
	t_list* suscriptors;
	t_list* mensajes;
	uint32_t id;
}mq;

void iniciar_config_broker(char* broker_config);
void terminar_broker(t_log* logger, t_config* config);
void iniciar_broker(void);
void iniciar_colas_mensaje();
mq* crear_cola_mensaje(int tipo);
void iniciar_servidor(void);
void esperar_cliente(int socket_servidor);
void serve_client(int* socket);
void process_request(int cod_op, int cliente_fd);
void * recibir_mensaje(int socket_cliente);
void atender_suscripcion(int cliente_fd );
void atender_ack(suscriber* sus);
void enviar_id(mensaje *item,int socket_cliente);
void mensaje_new_pokemon(void* msg,int cliente_fd);
void mensaje_appeared_pokemon(void* msg,int cliente_fd);
void mensaje_catch_pokemon(void* msg,int cliente_fd);
void mensaje_caught_pokemon(void* msg,int cliente_fd);
void mensaje_get_pokemon(void* msg,int cliente_fd);
void mensaje_localized_pokemon(void* msg,int cliente_fd);
mensaje* crear_mensaje(int tipo_msg,int socket,int nro_id,void* msg);
void agregar_paquete(t_paquete* enviar,particion* aux,suscriber* sus,uint32_t tipo);
void enviar_mensajes(suscriber* sus,int tipo_cola);
void enviar_confirmacion_suscripcion(suscriber* sus);
mq* cola_mensaje(uint32_t tipo);
void borrar_mensaje(mensaje* m);
bool confirmado_todos_susciptors_mensaje(mensaje* m);
bool enviado_ack_suscriptor(mensaje* m,suscriber* sus);
//--------------------------------------------------------------------------------------------------------
void sig_handler();
void iniciar_memoria();
void iniciar_config(char* broker_config);
void iniciar_semaforos();
void iniciar_cache();
particion* malloc_cache(size_t);
void display_cache();
void handler_dump(int signo);
void free_cache();
void* memcpy_cache(particion*,uint32_t id_buf,uint32_t tipo_cola,void* destion,void* buf,uint32_t);
void compactar_particiones_dinamicas();
void consolidar_buddy_system();
particion* algoritmo_particion_libre(uint32_t size);
particion* particiones_dinamicas(uint32_t size);
uint32_t calcular_size_potencia_dos(uint32_t size);
uint32_t log_dos(uint32_t size);
void algoritmo_reemplazo();
particion* buddy_system(uint32_t size);
void delete_particion(particion* borrar);
void limpiar_cache();
void consolidar_particiones_dinamicas();
#endif /* CONEXIONES_H_ */
