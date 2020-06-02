#ifndef GAMEBOY_H_
#define GAMEBOY_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>
#include<commons/collections/list.h>
#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>
#include<sys/time.h>

enum TIPO{

	//QUEUE_ID
	NEW_POKEMON = 1,
	APPEARED_POKEMON = 2,
	CATCH_POKEMON = 3,
	CAUGHT_POKEMON = 4,
	GET_POKEMON = 5,
	LOCALIZED_POKEMON = 6,

	//ACTION
	SUSCRITO = 7,
	ACK = 8,
};

enum TIPO_PROCESO{
	TEAM = 10,
	GAMECARD = 11,
	GAMEBOY = 12,
	BROKER,
};

typedef struct{
	int size;
	void* stream;
} t_buffer;

typedef struct{
	enum TIPO codigo_operacion;
	t_buffer* buffer;
} t_paquete;

struct option{
	const char* name;
	int has_arg;
	int* flag;
	int val;
};

typedef struct{
	int posx;
	int posy;
}position;

t_log* logger;
t_config* config;
int conexion;

t_log* iniciar_logger(void);
t_config* leer_config(void);
void iniciar_gameboy(int argc,char* argv[]);
void conectar_proceso(int proceso);
int proceso(char* proceso);
int tipo_mensaje(char* tipo_mensaje);
void terminar_gameboy(int, t_log*, t_config*);
void suscriptor(int tipo);
int crear_conexion(char* ip, char* puerto);
void enviar_mensaje_new_pokemon(char* pokemon,int posx,int posy,int cant,int,int socket_cliente);
void enviar_mensaje_new_pokemon_broker(char* pokemon,int posx,int posy,int cant,int socket_cliente);
void enviar_mensaje_catch_pokemon(char* pokemon,int posx,int posy,int,int socket_cliente);
void enviar_mensaje_appeared_pokemon(char* pokemon,int posx,int posy,int socket_cliente);
void enviar_mensaje_appeared_pokemon_broker(char* pokemon,int posx,int posy,int id_correlacional, int socket_cliente);
void enviar_mensaje_caught_pokemon(int id_mensaje,int ok_fail,int socket_cliente);
void enviar_mensaje_get_pokemon(char* pokemon,int,int socket_cliente);
void enviar_mensaje_get_pokemon_broker(char* pokemon,int socket_cliente);
void enviar_mensaje_catch_pokemon_broker(char* pokemon,int posx,int posy,int socket_cliente);
void enviar_info_suscripcion(int tipo,int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void liberar_conexion(int socket_cliente);
void* serializar_paquete(t_paquete* paquete, int bytes);
void recibir_confirmacion_suscripcion(int cliente_fd);
void* recibir_buffer(int socket_cliente, uint32_t* size);
void enviar_ack(int id,int socket_cliente);
t_paquete* crear_paquete(int accion);
t_list* recibir_paquete(int socket_cliente);
void recibir_get_pokemon();
void recibir_new_pokemon();
void recibir_appeared_pokemon();
void recibir_catch_pokemon();
void recibir_caught_pokemon();
void recibir_mensajes(int);

#endif /* GAMEBOY_H_ */
