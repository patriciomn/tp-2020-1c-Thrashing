#ifndef GAMEBOY_H_
#define GAMEBOY_H_

#include"utils.h"
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

enum TIPO_PROCESO{
	SUSCRIPTOR = 9,
	TEAM = 10,
	GAMECARD = 11,
	GAMEBOY = 12,
	BROKER = 13,
};

struct option{
	const char* name;
	int has_arg;
	int* flag;
	int val;
};

void iniciar_gameboy(int argc,char* argv[]);
void conectar_proceso(int proceso);
int proceso(char* proceso);
int tipo_mensaje(char* tipo_mensaje);
void terminar_gameboy(int, t_log*, t_config*);
void suscriptor(int tipo);
void enviar_mensaje_new_pokemon(char* pokemon,int posx,int posy,int cant,int,int socket_cliente);
void enviar_mensaje_new_pokemon_broker(char* pokemon,int posx,int posy,int cant,int socket_cliente);
void enviar_mensaje_catch_pokemon(char* pokemon,int posx,int posy,int,int socket_cliente);
void enviar_mensaje_appeared_pokemon(char* pokemon,int posx,int posy,int socket_cliente);
void enviar_mensaje_appeared_pokemon_broker(char* pokemon,int posx,int posy,int id_correlacional, int socket_cliente);
void enviar_mensaje_caught_pokemon(int id_mensaje,bool ok_fail,int socket_cliente);
void enviar_mensaje_get_pokemon(char* pokemon,int,int socket_cliente);
void enviar_mensaje_get_pokemon_broker(char* pokemon,int socket_cliente);
void enviar_mensaje_catch_pokemon_broker(char* pokemon,int posx,int posy,int socket_cliente);
void recibir_get_pokemon();
void recibir_new_pokemon();
void recibir_appeared_pokemon();
void recibir_catch_pokemon();
void recibir_caught_pokemon();
void recibir_localized_pokemon();
void recibir_mensajes(int);

#endif /* GAMEBOY_H_ */
