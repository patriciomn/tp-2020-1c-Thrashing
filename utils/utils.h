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

//LOS INT HAY QUE CAMBIARLOS POR UINT_32

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

typedef struct{ //Este no lo entendi
	int name_size;
	char* name;
	position pos;
	int cantidad_posiciones;
}localized_pokemon;

typedef struct{
	int name_size;
	char* name;
}get_pokemon;

typedef struct{
	int name_size;
	char* name;
	position pos;
}appeared_pokemon;

typedef struct{
	int caught; // 1 o 0 en funcion de si se atrapo o no (respectivamente)
}caught_pokemon;
