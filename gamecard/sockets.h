#ifndef SOCKETS_H_
#define SOCKETS_H_

#include "gamecard.h"

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

typedef struct{
	int id;
	int correlation_id;
	int size;
	void* stream;
} t_buffer;

typedef struct{
	enum TIPO codigo_operacion;
	t_buffer* buffer;
} t_paquete;


typedef struct {
	int posX;
	int posY;
}Position;

typedef struct {
	int id_mensaje;
	int size_nombre;
	char *nombre;
	Position posicion;
	int cantidad;
}NPokemon;

typedef struct {
	int id_mensaje;
	int size_nombre;
	char *nombre;
	Position posicion;
}CPokemon;

typedef struct {
	int id_mensaje;
	int size_nombre;
	char *nombre;
}GPokemon;

typedef struct{
	int name_size;
	char* name;
}get_pokemon;


typedef struct{
	Position pos;
	int cant;
}posiYcant;

typedef struct{
	int id_mensaje;
	char* name;
	posiYcant pos[];
}rtaGet;

void suscripcion_colas_broker();
void suscribirse_a_new_pokemon();
void suscribirse_a_catch_pokemon();
void suscribirse_a_get_pokemon();

int crear_conexion();
void iniciar_servidor(void);
void esperar_cliente(int socket_servidor);
void atender_peticion(int socket_cliente, int cod_op);
void enviar_mensaje_suscripcion(enum TIPO cola, int socket_cliente, pid_t pid);
void* serializar_paquete(t_paquete* paquete, int bytes);

void recibir_mensajes_new_pokemon();
void recibir_mensajes_catch_pokemon();
void recibir_mensajes_get_pokemon();

t_list* recibir_paquete(int socket_cliente);
void* recibir_buffer(int socket_cliente, uint32_t* size);
void enviar_ack(int tipo,int id,pid_t pid,int cliente_fd);

void deserealizar_new_pokemon_gameboy(void *stream, NPokemon *newPokemon);
void deserealizar_catch_pokemon_gameboy(void *stream, CPokemon *catchPokemon);
void deserealizar_get_pokemon_gameboy(void *stream, GPokemon *getPokemon);

get_pokemon* deserializar_get(void* buffer);

int socket_cliente_np;
int socket_cliente_cp;
int socket_cliente_gp;

#endif /* SOCKETS_H_ */
