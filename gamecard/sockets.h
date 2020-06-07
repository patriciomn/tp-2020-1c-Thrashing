#ifndef SOCKETS_H_
#define SOCKETS_H_

#include "gamecard.h"

typedef struct {
	int posX;
	int posY;
}Position;

typedef struct {
	int id_mensaje;
	char *nombre;
	Position posicion;
	int cantidad;
}NPokemon;

typedef struct {
	int id_mensaje;
	char *nombre;
	Position posicion;
}CPokemon;

typedef struct {
	int id_mensaje;
	char *nombre;
}GPokemon;

void suscripcion_colas_broker();
int crear_conexion();
void iniciar_servidor(void);
void esperar_cliente(int socket_servidor);
void atender_peticion(int socket_cliente, int cod_op);

void deserealizar_new_pokemon_gameboy(void *stream, NPokemon *newPokemon);
void deserealizar_catch_pokemon_gameboy(void *stream, CPokemon *catchPokemon);
void deserealizar_get_pokemon_gameboy(void *stream, GPokemon *getPokemon);

int socket_cliente_np;
int socket_cliente_cp;
int socket_cliente_gp;



#endif /* SOCKETS_H_ */
