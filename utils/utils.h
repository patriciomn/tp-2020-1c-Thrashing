#include<stdio.h>
#include<stdlib.h>
#include<string.h>


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
	enum TIPO queue_id;
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