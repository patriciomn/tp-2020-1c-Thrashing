enum TIPO{
	NEW_POKEMON = 1,
	APPEARED_POKEMON = 2,{}
	CATCH_POKEMON = 3,
	CAUGHT_POKEMON = 4,
	GET_POKEMON = 5,
	LOCALIZED_POKEMON = 6,
	SUSCRITO = 7,
};
typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	enum TIPO codigo_operacion;
	t_buffer* buffer;
} t_paquete;


typedef struct{
	char* name;
	int posx;
	int posy;
}pokemon;


typedef struct{
	char* name;
}get_pokemon;