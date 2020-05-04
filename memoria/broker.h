#ifndef BROKER_H_
#define BROKER_H_

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
	SUSCRIPTOR = 7,
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


void iniciar_broker();
void iniciar_colas_mensaje();
void iniciar_servidor(void);
void esperar_cliente(int);
void* recibir_buffer(int socket_cliente, int* size);
int recibir_operacion(int);
void process_request(int cod_op, int cliente_fd);
void serve_client(int *socket);
void* serializar_paquete(t_paquete* paquete, int bytes);
void mensaje_new_pokemon(void*,int cliente_fd);
void mensaje_catch_pokemon(void*buf,int cliente_fd);
void mensaje_get_pokemon(void*,int cliente_fd);
void mensaje_localized_pokemon(void*,int cliente_fd);
void mensaje_caught_pokemon(void*,int cliente_fd);
void mensaje_appeared_pokemon(void*,int cliente_fd);
void localized_pokemon(suscriber* sus);
void recibir_confirmacion_mensaje(suscriber* sus);
void enviar_confirmacion_suscripcion(char* confirmacion,int socket_cliente);
void atender_procesos(int,int cliente_fd);
void enviar_id_mensaje(msg* mensaje,int socket_cliente);
t_config* leer_config(char* config);
void iniciar_config(char* broker_config);
void atender_mensajes(int socket,int,int cod,void*);
int recibir_valor(int conexion);
void suscripcion_new_pokemon(suscriber* sus);
void suscripcion_appeared_pokemon(suscriber* sus);
void suscripcion_catch_pokemon(suscriber* sus);
void suscripcion_caught_pokemon(suscriber* sus);
void suscripcion_get_pokemon(suscriber* sus);
void suscripcion_localized_pokemon(suscriber* sus);
void suscribir_cola(int tipo_cola,suscriber* sus);
msg* crear_mensaje(int tipo_msg,int socket,int nro_id);
void iniciar_semaforos();
void suscripcion_mensajes(suscriber* sus,int tipo);
void suscriptor_gameboy(int);
void recibir_confirmacion(int cliente_fd);
void enviar_confirmacion(int);
void devolver_mensaje(void* payload, int size,int tipo, int socket_cliente);
void crear_buffer(t_paquete* paquete);
t_paquete* crear_paquete(int);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void* recibir_mensaje(int socket_cliente);
//--------------------------------------------------------------------------------------------------------
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
#endif /* BROKER_H_ */
