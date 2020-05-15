#include<stdio.h>
#include<stdlib.h>
#include<string.h>
/*

	FORMA DE ENVIO DE MENSAJES:
	SUSCRIBE: SUSCRITO | QUEUE_ID

	NEW : NEW_POKEMON | CORRELATIONID | SIZE | name_size | name | posx | posy | cantidad
	APPEARED: APPEARED_POKEMON | CORRELATIONID | SIZE | name_size | name | posx | posy 
	CATCH: CATCH_POKEMON | CORRELATIONID | SIZE | name_size | name | posx | posy
	CAUGHT: CAUGHT_POKEMON | CORRELATIONID | SIZE | caught
	GET: GET_POKEMON | CORRELATIONID | SIZE | name_size | name
	LOCALIZED: LOCALIZED_POKEMON | CORRELATIONID | SIZE | name_size | name | posx | posy | cantidad_posiciones

	ACK: ACK | QUEUE_ID | ID | 	CORRELATIONID



	FORMA DE RECEPCION DE MENSAJES DE LA COLA:

	NEW : NEW_POKEMON |ID | CORRELATIONID | SIZE | name_size | name | posx | posy | cantidad
	APPEARED: APPEARED_POKEMON | ID | CORRELATIONID | SIZE | name_size | name | posx | posy 
	CATCH: CATCH_POKEMON |ID | CORRELATIONID | SIZE | name_size | name | posx | posy
	CAUGHT: CAUGHT_POKEMON |ID | CORRELATIONID | SIZE | caught
	GET: GET_POKEMON |ID | CORRELATIONID | SIZE | name_size | name
	LOCALIZED: LOCALIZED_POKEMON |ID | CORRELATIONID | SIZE | name_size | name | posx | posy | cantidad_posiciones

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
	position pos;
	int cantidad_posiciones;
}localized_pokemon;


void* serializar_paquete(t_paquete* paquete, int * bytes);
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