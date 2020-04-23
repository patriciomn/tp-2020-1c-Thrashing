#ifndef TEAM_H_
#define TEAM_H_

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
#include<sys/time.h>

#define IP "127.0.0.2"
#define PUERTO "4445"
#define CANT 50


enum ESTADO{
	NEW,
	BLOCKED,
	READY,
	EXEC,
	EXIT,
};

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
};


typedef struct{
	int size;
	void* stream;
} t_buffer;

typedef struct{
	enum TIPO codigo_operacion;
	t_buffer* buffer;
} t_paquete;

typedef struct{
	char* name;
	int posx;
	int posy;
	t_list* pokemones;
	t_list* objetivos;
	enum ESTADO estado;
	pthread_t hilo;
	int cant_ciclo;
}entrenador;

typedef struct{
	char* name;
	t_list* entrenadores;
	t_list* objetivos;
	int cant_ciclo;
}team;

typedef struct{
	char* name;
	int posx;
	int posy;
}pokemon;

typedef struct{
	char* log_file;
	int retardo_cpu;
	int tiempo_reconexion;
	char* ip_broker;
	char* puerto_broker;
	char* equipo_name;
	char* algoritmo;
	char* posiciones;
	char* pokemones;
	char*objetivos;
}config_team;


int conexion_broker;
t_log* logger;
t_config* config;
t_list* poks_requeridos;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
team* equipo;
config_team* datos_config;
t_list* id_mensajes;
pthread_t servidor;
pokemon* pok_elegido;
sem_t semExec;
sem_t semPoks;
sem_t semHilo;


void crear_team(char* teamConfig);
void iniciar_servidor(void);
void esperar_cliente(int);
void* recibir_buffer(int socket_cliente, int* size);
int recibir_operacion(int);
void process_request(int cod_op, int cliente_fd);
void serve_client(int *socket);
void* serializar_paquete(t_paquete* paquete, int bytes);
void devolver_mensaje(void* payload, int size, int socket_cliente);
void appeared_pokemon(int cliente_fd);
void catch_pokemon(entrenador*,pokemon*);
t_config* leer_config(char* config);
void crear_entrenador(char*,int posx,int posy,char* pokemones,char* objetivos);
int cant_pokemones(char**);
int verificar_cantidad_pokemones(entrenador* entrenador);
int cant_especie_team(team*,char* especie);
float distancia_pokemon(entrenador* entrenador,pokemon*);
void iniciar_config(char* teamConfig);
void get_pokemon(char*pokemon);
void enviar_mensaje_get_pokemon(char* pokemon,int socket_cliente);
void enviar_info_suscripcion(char* process,int tipo,int socket_cliente);
void localized_pokemon(int cliente_fd);
int crear_conexion(char *ip, char* puerto);
void recibir_confirmacion_suscripcion(int cliente_fd);
void suscribirse_broker(char* name,int tipo);
pokemon* crear_pokemon(char* name);
void set_pokemon(pokemon* pok,int posx,int posy);
void ejecutar_entrenador(entrenador*);
void activar_entrenador(entrenador* entre);
entrenador* find_entrenador_name(char* name);
void bloquear_entrenador(entrenador* entre);
void despertar_entrenador(entrenador* entre);
void salir_entrenador(entrenador* entre);
void move_entrenador(entrenador* entre,int,int);
void iniciar_team(char* teamConfig);
bool contener_pokemon(entrenador* entre,char* name);
bool cumplir_objetivo_entrenador(entrenador* entre);
int cant_especie_pokemon(entrenador* entre,char* name);
int cant_especie_objetivo(entrenador* entre,char* name);
bool cumplir_objetivo_team();
entrenador* planificar_entrenador(pokemon* pok);
int cant_especie_objetivo_team(char* name);
entrenador* find_entrenador_cerca(pokemon* pok);
void remove_pokemon_map(pokemon* pok);
void end_of_quantum_handler();
void enviar_mensaje_catch_pokemon(char* pokemon,int socket_cliente);
void reintento_conectar_broker();
void reconexion();
bool requerir_atrapar(char* pok);
char* recibir_id_mensaje(int cliente_fd);
t_list* especies_objetivo_team();
bool hay_repetidos(t_list* especies);
void crear_hilo(entrenador*);
bool necesitar_pokemon(entrenador* entre,char* name);
bool retencion_y_espera(char* pok);
bool equipo_en_blocked();
bool deadlock_entrenador(entrenador*);
void salir_grupo();
void liberar_conexion(int socket_cliente);
void pokemon_a_atrapar();
pokemon* pokemon_no_necesario(entrenador* entre);
void intercambiar_pokemon(entrenador* entre1,entrenador* entre2);
void execution();
bool cpu_libre();
void sumar_ciclos(entrenador* entre,int ciclos);
bool poseer_exceso_noNecesario(entrenador* entre);
bool espera_circular(entrenador*);
bool pokemon_exceso(entrenador* entre,char* name);
bool detectar_deadlock();

#endif /* TEAM_H_ */