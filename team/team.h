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
#include <uuid/uuid.h>

#define IP "127.0.0.2"
#define PUERTO "4445"
#define CICLOS_MOVER 1
#define CICLOS_ENVIAR 1
#define CICLOS_INTERCAMBIAR 5
#define CANT_ENTRE 10


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
	ACK = 8,
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
	int posx;
	int posy;
}position;

typedef struct{
	int tid;
	int posx;
	int posy;
	int estado;
	t_list* espera_caught;
	t_list* pokemones;
	t_list* objetivos;
	pthread_t hilo;
	int arrival_time;
	int service_time;
	int start_time;
	int finish_time;
	int turnaround_time;
	double estimacion_anterior;
	int ciclos_totales;
}entrenador;

typedef struct{
	uuid_t pid;
	entrenador* exec;
	t_list* entrenadores;
	t_list* objetivos;
	int cant_ciclo;
	t_list* poks_requeridos;
	t_list* cola_ready;
}team;

typedef struct{
	char* name;
	int posx;
	int posy;
	bool espera_caught;
}pokemon;

typedef struct{
	char* log_file;
	int retardo_cpu;
	int tiempo_reconexion;
	char* ip_broker;
	char* puerto_broker;
	char* algoritmo;
	char* posiciones;
	char* pokemones;
	char*objetivos;
	double alpha;
	double estimacion_inicial;
	int quantum;
}config_team;

typedef struct{
	int get;
	int localized;
	int appeared;
	int catch;
	int caught;
}conexion;

typedef struct{
	pokemon* pok;
	uint32_t id_recibido;
	enum TIPO tipo_msg;
	void* buf_msg;
}msg;

void crear_team();
void iniciar_servidor(void);
void esperar_cliente(int);
void* recibir_buffer(int socket_cliente, int* size);
int recibir_operacion(int);
void process_request(int cod_op, int cliente_fd);
void serve_client(int *socket);
void* serializar_paquete(t_paquete* paquete, int bytes);
void devolver_mensaje(void* payload, int size, int socket_cliente);
void appeared_pokemon(int cliente_fd);
bool catch_pokemon(entrenador*,pokemon*);
t_config* leer_config(char* config);
void crear_entrenador(int,int posx,int posy,char* pokemones,char* objetivos);
int cant_pokemones(char**);
bool verificar_cantidad_pokemones(entrenador* entrenador);
int cant_especie_team(team*,char* especie);
float distancia_pokemon(entrenador* entrenador,pokemon*);
void iniciar_config(char* teamConfig);
void get_pokemon(char*pokemon);
void enviar_mensaje_get_pokemon(char* pokemon,int socket_cliente);
void enviar_info_suscripcion(int tipo,int socket_cliente);
void localized_pokemon(int cliente_fd);
int crear_conexion(char *ip, char* puerto);
void recibir_confirmacion_suscripcion(int cliente_fd);
pokemon* crear_pokemon(char* name);
void set_pokemon(pokemon* pok,int posx,int posy);
void ejecutar_entrenador(entrenador*);
void activar_entrenador(entrenador* entre);
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
entrenador* algoritmo_corto_plazo();
int cant_especie_objetivo_team(char* name);
void algoritmo_largo_plazo(pokemon* pok);
void remove_pokemon_requeridos(pokemon* pok);
void end_of_quantum_handler();
void enviar_mensaje_catch_pokemon(char* pokemon,int,int,int socket_cliente);
void reintento_conectar_broker();
void reconexion();
bool requerir_atrapar(char* pok);
int recibir_id_mensaje(int cliente_fd);
t_list* especies_objetivo_team();
bool hay_repetidos(t_list* especies);
void crear_hilo_entrenador(entrenador*);
bool necesitar_pokemon(entrenador* entre,char* name);
bool equipo_en_blocked();
bool verificar_deadlock_entrenador(entrenador*);
void salir_equipo();
void liberar_conexion(int socket_cliente);
pokemon* pokemon_a_atrapar();
pokemon* pokemon_no_necesario(entrenador* entre);
void intercambiar_pokemon(entrenador* entre1,entrenador* entre2);
void ejecutar_equipo();
bool verificar_cpu_libre();
void sumar_ciclos(entrenador* entre,int ciclos);
bool verificar_pokemon_exceso_no_necesario(entrenador* entre);
bool verificar_espera_circular(entrenador*);
bool pokemon_exceso(entrenador* entre,char* name);
bool verificar_deadlock_equipo();
msg* crear_mensaje(int id,int tipo,pokemon*);
void suscribirse_appeared();
void suscribirse_localized();
void suscribirse_caught();
void suscribirse_broker();
void enviar_mensajes_get_pokemon();
void recibir_caught_pokemon();
void recibir_appeared_pokemon();
void enviar_confirmacion(int socket_cliente);
t_list* recibir_paquete(int socket_cliente);
double estimacion(entrenador* entre);
void algoritmo_fifo(t_list* cola_ready);
void fin_de_quantum();
void algoritmo_round_robin(t_list* cola_ready);
void algoritmo_sjf_sin_desalojo(t_list* cola_ready);
void algoritmo_sjf_con_desalojo(t_list* cola_ready);
bool verificar_espera_caught(entrenador* entre);
#endif /* TEAM_H_ */
