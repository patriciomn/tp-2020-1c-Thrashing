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
/*

	FORMA DE ENVIO DE MENSAJES:

	SUSCRIBE: SUSCRITO | QUEUE_ID | PID  Dejar solo 12 bytes para la suscripcion. En caso de ser la primera suscripcion mandar un -1 en PID
	-> respondo ack con pid que genero y guardo para identificar al proceso (ACK | PID) 

	NEW : NEW_POKEMON  | SIZE | name_size | name | posx | posy | cantidad
	APPEARED: APPEARED_POKEMON | SIZE | CORRELATIONID | name_size | name | posx | posy 

	CATCH: CATCH_POKEMON  | SIZE | name_size | name | posx | posy
	CAUGHT: CAUGHT_POKEMON | SIZE | CORRELATIONID  | caught

	GET: GET_POKEMON  | SIZE | name_size | name
	LOCALIZED: LOCALIZED_POKEMON | SIZE | CORRELATIONID  | name_size | name | posx | posy | cantidad_posiciones

	ACK: ACK | ID broker responde



	FORMA DE RECEPCION DE MENSAJES DE LA COLA:

	NEW : NEW_POKEMON | ID   | name_size | name | posx | posy | cantidad
	APPEARED: APPEARED_POKEMON  | ID | CORRELATIONID  | name_size | name | posx | posy 

	CATCH: CATCH_POKEMON | ID   | name_size | name | posx | posy
	CAUGHT: CAUGHT_POKEMON  | ID | CORRELATIONID |  caught

	GET: GET_POKEMON |ID   | name_size | name
	LOCALIZED: LOCALIZED_POKEMON | ID | CORRELATIONID  | name_size | name | posx | posy | cantidad_posiciones

	ACK RECEPCION: ACK | PID | QUEUE_ID | ID  broker recibe

*/

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

typedef struct{
	int posx;
	int posy;
}position;

typedef struct{
	int id_mensaje;
	int name_size;
	char* name;
	position pos;
	int cantidad;
}new_pokemon;

typedef struct{
	int name_size;
	char* name;
	position pos;
}appeared_pokemon;

typedef struct{
	int id_mensaje;
	int name_size;
	char* name;
	position pos;
}catch_pokemon;

typedef struct{
	bool caught;
}caught_pokemon;

typedef struct{
	int id_mensaje;
	int name_size;
	char* name;
}get_pokemon;

typedef struct{
	int name_size;
	char* name;
	int cantidad_posiciones;
	t_list* posiciones;
}localized_pokemon;

void* serializar_paquete(t_paquete* paquete, int bytes);
new_pokemon* deserializar_new(void* buffer) ;
appeared_pokemon* deserializar_appeared(void* buffer) ;
catch_pokemon* deserializar_catch(void* buffer);
caught_pokemon* deserializar_caught(void* buffer);
get_pokemon* deserializar_get(void* buffer) ;
localized_pokemon* deserializar_localized(void* buffer) ;
void enviar_ack(int tipo,int id,pid_t pid,int cliente_fd);
t_list* recibir_paquete(int socket_cliente);
void enviar_info_suscripcion(int tipo,int socket_cliente,pid_t pid);
void* recibir_buffer(int socket_cliente, int* size);
int crear_conexion(char *ip, char* puerto);
int recibir_confirmacion_suscripcion(int cliente_fd,int);
void liberar_conexion(int socket_cliente);
bool check_socket(int sock);
int recibir_id_mensaje(int cliente_fd);
char* get_cola(uint32_t);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
