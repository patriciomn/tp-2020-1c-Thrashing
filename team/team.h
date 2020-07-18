#ifndef TEAM_H_
#define TEAM_H_

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
#include<sys/time.h>

#define IP "127.0.0.3"
#define PUERTO "4446"
#define CICLO_ACCION 1
#define CICLOS_INTERCAMBIAR 5
#define CANT_ENTRE 100

enum ESTADO{
	NEW,
	BLOCKED,
	READY,
	EXEC,
	EXIT,
};

typedef struct{
	char* name;
	int posx;
	int posy;
	bool espera_caught;
	bool requerido;
}pokemon;

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
	int cant_cambio_contexto;
	int quantum;
	pokemon* pok_atrapar;
}entrenador;

typedef struct{
	pid_t pid;
	entrenador* exec;
	entrenador* blocked;
	t_list* entrenadores;
	t_list* objetivos;
	t_list* exit;
	int cant_ciclo;
	t_list* poks_requeridos;
	t_list* cola_ready;
	t_list* cola_deadlock;
	t_list* metrica_deadlock;
}team;

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
	int id;
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

void iniciar_config(char* teamConfig);
void iniciar_team(char* teamConfig);
void crear_team();
bool cumplir_objetivo_team();
void iniciar_team(char* teamConfig);
void ejecutar_equipo();
void salir_equipo();

void crear_entrenador(int,int posx,int posy,char* pokemones,char* objetivos);
void ejecutar_entrenador(entrenador*);
void activar_entrenador(entrenador* entre);
void bloquear_entrenador(entrenador* entre);
void despertar_entrenador(entrenador* entre);
void salir_entrenador(entrenador* entre);
void move_entrenador(entrenador* entre,int,int);
void movimiento_ejex_entrenador(entrenador* entre,int posx);
void movimiento_ejey_entrenador(entrenador* entre,int posx);
bool atrapar_pokemon(entrenador*,pokemon*);
void intercambiar_pokemon(entrenador* entre1,entrenador* entre2);
void actuar_intercambio(entrenador* entre1,entrenador* entre2,pokemon* pok1,pokemon* pok2);
void agregar_eliminar_pokemon(entrenador* entre,pokemon* pok_agregar,pokemon* pok_eliminar);
void actuar_entrenador_sin_desalojo(entrenador* entre);
void actuar_entrenador_con_desalojo(entrenador* entre);
void desalojar_entrenador(entrenador* entre);
bool con_estimacion_menor(entrenador* entre);

pokemon* crear_pokemon(char* name);
void set_pokemon(pokemon* pok,int posx,int posy);

void cantidad_deadlocks();
void detectar_deadlock();
bool verificar_pokemon_exceso_no_necesario(entrenador* entre);
bool verificar_espera_circular();
bool pokemon_exceso(entrenador* entre,char* name);
bool verificar_deadlock_equipo();
pokemon* pokemon_retenido_espera(entrenador* entre1,entrenador* entre2);
pokemon* pokemon_a_intercambiar(entrenador* entre);
entrenador* entrenador_bloqueado_deadlock();
bool contener_pokemon(entrenador* entre,char* name);
bool cumplir_objetivo_entrenador(entrenador* entre);
int cant_especie_pokemon(entrenador* entre,char* name);
int cant_especie_objetivo(entrenador* entre,char* name);
int cant_pokemones(char**);
bool verificar_cantidad_pokemones(entrenador* entrenador);
int cant_especie_team(team*,char* especie);
float distancia_pokemon(entrenador* entrenador,pokemon*);
int cant_especie_objetivo_team(char* name);
void remove_pokemon_requeridos(pokemon* pok);
bool requerir_atrapar(char* pok);
t_list* especies_objetivo_team();
bool hay_repetidos(t_list* especies);
void crear_hilo_entrenador(entrenador*);
bool necesitar_pokemon(entrenador* entre,char* name);
bool equipo_en_blocked();
bool verificar_deadlock_entrenador(entrenador*);
pokemon* pokemon_no_necesario(entrenador* entre);
bool verificar_cpu_libre();
void sumar_ciclos(entrenador* entre,int ciclos);
bool verificar_espera_caught(entrenador* entre);
int cant_entrenadores(char** posiciones);
int cant_requerida_pokemon(char* name);
entrenador* algoritmo_deadlock(t_list* cola);
int cant_especie_obtenida_team(char* name);

double estimacion(entrenador* entre);
void algoritmo_largo_plazo(pokemon* pok);
entrenador* algoritmo_corto_plazo();
entrenador* algoritmo_fifo(t_list* cola_ready);
entrenador* algoritmo_round_robin(t_list* cola_ready);
entrenador* algoritmo_sjf(t_list* cola_ready);
msg* crear_mensaje(int id,int tipo,pokemon*);

void iniciar_servidor(void);
void esperar_cliente(int);
void process_request(int cod_op, int cliente_fd);
void serve_client(int *socket);
void suscribirse_appeared();
void suscribirse_localized();
void suscribirse_caught();
void suscribirse_broker();
void enviar_mensajes_get_pokemon();
void enviar_mensajes_get_pokemon();
void enviar_get_pokemon_broker(char* pokemon);
void enviar_mensaje_get_pokemon(char* pokemon,int socket_cliente);
void enviar_mensaje_catch_pokemon(char* pokemon,int,int,int socket_cliente);
void recibir_caught_pokemon();
void recibir_appeared_pokemon();
void recibir_localized_pokemon();
void recibir_appeared_pokemon_gameboy(int cliente_fd);
void end_of_quantum_handler();
void reintento_conectar_broker();
void conectar_broker();
void recibir_mensajes();

#endif /* TEAM_H_ */
