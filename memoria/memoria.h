#ifndef MEMORIA_H_
#define MEMORIA_H_

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<commons/log.h>
#include<sys/socket.h>
#include<unistd.h>
#include<netdb.h>
#include<commons/collections/list.h>
#include<commons/string.h>
#include<pthread.h>
#include<commons/config.h>
#include<signal.h>
#include<math.h>
#include<commons/temporal.h>
#include<semaphore.h>

enum TIPO{
	NEW_POKEMON = 1,
	APPEARED_POKEMON = 2,
	CATCH_POKEMON = 3,
	CAUGHT_POKEMON = 4,
	GET_POKEMON = 5,
	LOCALIZED_POKEMON = 6,
	SUSCRITO = 7,
	ACK = 8,
};

enum OPERACION{
	SUSCRIPCION = 8,
	MENSAJE = 9,
};
//

typedef struct{
	int size;
	void* stream;
} t_buffer;

typedef struct{
	enum TIPO codigo_operacion;
	t_buffer* buffer;
} t_paquete;

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
	uint32_t pid;
	uint32_t socket;
}suscriber;

typedef struct{
	uint32_t tipo_queue;
	t_list* suscriptors;
	t_list* mensajes;
	uint32_t id;
}mq;

typedef struct{
	uint32_t id;
	uint32_t id_correlacional;
	enum TIPO tipo_msg;
	t_list* suscriptors_enviados;
	t_list* suscriptors_ack;
}msg;

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
#endif
