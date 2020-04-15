#ifndef GAMEBOY_H_
#define GAMEBOY_H_

#include<stdio.h>
#include<stdlib.h>
#include<commons/log.h>
#include<commons/string.h>
#include<commons/config.h>
#include<readline/readline.h>


#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>

# define no_argument        0    //不需要参数
# define required_argument  1    //必须指定参数
# define optional_argument  2	//参数可选

enum TIPO{
	NEW_POKEMON = 1,
	APPEARED_POKEMON = 2,
	CATCH_POKEMON = 3,
	CAUGHT_POKEMON = 4,
	GET_POKEMON = 5,
	LOCALIZED_POKEMON = 6,
	SUSCRITO = 7,
};

enum TIPO_PROCESO{
	TEAM = 8,
	GAMECARD = 9,
	GAMEBOY = 10,
	BROKER,
};

typedef struct
{
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

t_log* logger;
t_config* config;
int conexion;

t_log* iniciar_logger(void);
t_config* leer_config(void);
void iniciar_gameboy();
int getopt_long_only (int argc, char *const *argv, const char *shortopts, const struct option *longopts, int *indexptr);
void conectar_proceso(int proceso);
int proceso(char* proceso);
int tipo_mensaje(char* tipo_mensaje);
void terminar_gameboy(int, t_log*, t_config*);
void new_pokemon(char* pokemon,int posx,int posy,int cant);
void catch_pokemon(char* pokemon,int posx,int posy);
void appeared_pokemon(enum TIPO_PROCESO pro,char* pokemon,int posx,int posy,int id_mensaje);
void caught_pokemon(int id_mensaje,int ok_fail);
void get_pokemon(char*pokemon);

int crear_conexion(char* ip, char* puerto);
void enviar_mensaje_new_pokemon(char* pokemon,int posx,int posy,int cant, int socket_cliente);
void enviar_mensaje_catch_pokemon(char* pokemon,int posx,int posy,int socket_cliente);
void enviar_mensaje_appeared_pokemon(enum TIPO_PROCESO pro,char* pokemon,int posx,int posy,int id_mensaje, int socket_cliente);
void enviar_mensaje_caught_pokemon(int id_mensaje,int ok_fail,int socket_cliente);
void enviar_mensaje_get_pokemon(char* pokemon,int socket_cliente);
char* recibir_mensaje(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void liberar_conexion(int socket_cliente);
void* serializar_paquete(t_paquete* paquete, int bytes);
void delChar(char* s,char c);

#endif /* GAMEBOY_H_ */
