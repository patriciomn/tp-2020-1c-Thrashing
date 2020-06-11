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

CAMBIAR ACA:	SUSCRIBE: SUSCRITO | SIZE | QUEUE_ID | PID  Dejar solo 12 bytes para la suscripcion. En caso de ser la primera suscripcion mandar un -1 en PID
	-> respondo ack con pid que genero y guardo para identificar al proceso (ACK | PID) 

	NEW : NEW_POKEMON  | SIZE | name_size | name | posx | posy | cantidad
	APPEARED: APPEARED_POKEMON | SIZE | CORRELATIONID | name_size | name | posx | posy 

	CATCH: CATCH_POKEMON  | SIZE | name_size | name | posx | posy
	CAUGHT: CAUGHT_POKEMON | SIZE | CORRELATIONID  | caught

	GET: GET_POKEMON  | SIZE | name_size | name
	LOCALIZED: LOCALIZED_POKEMON | SIZE | CORRELATIONID  | name_size | name | posx | posy | cantidad_posiciones

	ACK: ACK | ID broker responde

a PARTIR DE ACA HAY QUE AGREGAR EL SIZE

	FORMA DE RECEPCION DE MENSAJES DE LA COLA:

	NEW : NEW_POKEMON | SIZE |  ID | -1   | name_size | name | posx | posy | cantidad
	APPEARED: APPEARED_POKEMON  | SIZE | ID | CORRELATIONID  | name_size | name | posx | posy 

	CATCH: CATCH_POKEMON | SIZE | ID  | -1 | name_size | name | posx | posy
	CAUGHT: CAUGHT_POKEMON  | SIZE | ID |  CORRELATIONID |  caught

	GET: GET_POKEMON | SIZE | ID | -1  | name_size | name
	LOCALIZED: LOCALIZED_POKEMON | SIZE | ID | CORRELATIONID  | name_size | name | posx | posy | cantidad_posiciones

	ACK RECEPCION: ACK | SIZE | PID | QUEUE_ID | ID  broker recibe

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
	void * package;
} package;

typedef struct{
	int size;
	package* stream;
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
	int name_size;
	char* name;
	position pos;

}catch_pokemon;

typedef struct{
	int caught; // 1 o 0 en funcion de si se atrapo o no (respectivamente)
}caught_pokemon;

typedef struct{
	int name_size;
	char* name;
}get_pokemon;

typedef struct{ //Este no lo entendi
	int name_size;
	char* name;
	int cantidad_posiciones;
	position pos[];
	
}localized_pokemon;

void* serializar_paquete(t_paquete* paquete, int bytes);
void* serializar_paq(t_paquete* paquete, int * bytes);
void* serializar_any(void* paquete, int * bytes, int cod_op);
void* serializar_new(new_pokemon* paquete, int * bytes);
void* serializar_appeared(appeared_pokemon* paquete, int * bytes);
void* serializar_catch(catch_pokemon* paquete, int * bytes);
void* serializar_caught(caught_pokemon* paquete, int * bytes);
void* serializar_get(get_pokemon* paquete, int * bytes);
void* serializar_localized(localized_pokemon* paquete, int * bytes);
new_pokemon* deserializar_new(void* buffer) ;
appeared_pokemon* deserializar_appeared(void* buffer) ;
catch_pokemon* deserializar_catch(void* buffer);
caught_pokemon* deserializar_caught(void* buffer);
get_pokemon* deserializar_get(void* buffer) ;
localized_pokemon* deserializar_localized(void* buffer) ;
