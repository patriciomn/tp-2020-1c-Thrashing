#ifndef GAMECARD_H_
#define GAMECARD_H_

#include <commons/bitarray.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/log.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <netdb.h>
#include <sys/socket.h>
#include <pthread.h>

//#include "utils.h"

#define MAGIC_NUMBER "TALL_GRASS"
//#define RUTA_BITMAP  "/home/utnso/Escritorio/tall_grass/fs/metadata/bitmap.bin"
#define METADATA_DIR "/metadata"
#define METADATA_FILE "/metadata.txt"
#define BLOCKS_DIR "/blocks"
#define FILES_DIR "/files"
#define POKEMON_DIR "/Pokemon"
#define METADATA_TXT_PATH "/metadata/metadata.txt"
#define BITMAP_PATH "/metadata/bitmap.bin"
#define BITMAP_FILE "/bitmap.bin"
#define BITS 8
#define POKEMON_FILE_EXT ".txt"
#define BITMAP_FS "/home/utnso/desktop/tall-grass/Metadata/bitmap.bin"

#define GUION "-"
#define IGUAL "="


t_log *logger;
t_bitarray *bitarray;			// variable global bitmap, para manejar el bitmap siempre utilizamos esta variable
t_config *config_tall_grass;

struct metadata_info {
    int block_size;
    int blocks;
};

struct config_tallGrass {
    int tiempo_reintento_conexion;
    int tiempo_reintento_operacion;
    char *ip_broker;
    char *pto_de_montaje;
    int puerto_broker;
    int size_block;
    int blocks;
};

struct mensaje {
    int id;
    void* message;
} ;

//cambio los valores de pokemon segun la carpeta utils que va a utilizar(supongo) el broker
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

int socket_cliente_np;
int socket_cliente_cp;
int socket_cliente_gp;

struct metadata_info *metadataTxt; // este puntero es para el metadata.txt que ya existe en el fs
struct config_tallGrass *datos_config; // este struct es para almacenar los datos de las config. Tambien lo utilizamos para setear los valores de metadata.txt cuando se crea la primera vez


//FUNCIONES

int main ();

void verificar_punto_de_montaje();
void iniciar_logger_config();
void obtener_datos_archivo_config();
void verificar_metadata_txt(char *path_pto_montaje);
void crear_pto_de_montaje(char *path_pto_montaje);
void crear_directorio_files(char *path_pto_montaje);
void crear_directorio_blocks(char *path_pto_montaje);
void crear_metadata_tall_grass(char *path_pto_montaje);
void crear_archivos_metadata(char *path_metadata);
int existe_archivo_pokemon(char *path);
void crear_archivos_pokemon(char *path_pokefile, int posX, int posY, int cantidad);
void crear_metadata_pokemon(char *path_pokeflie);
int filesize (FILE* archivo );
void agregar_bloque_metadata_pokemon(char *path_pokemon_metadata_file, int nro_bloque);
int obtener_bloque_libre();
void crear_bitmap_bin(char *path_bitmap, int size_bitmap);
char *crear_nuevo_path(char* path_anterior, char *archivo);
void escribir_datos_bloque(char *path_blocks_dir, char *datos_a_agregar, int nro_bloque);


int tipo_mensaje(char* tipo_mensaje);
void new_pokemon(char* pokemon,int posx,int posy,int cant);
void catch_pokemon(char* pokemon,int posx,int posy);
void get_pokemon(char*pokemon);

// Funciones Sockets

int crear_conexion_broker();
void enviar_mensaje_suscripcion(enum TIPO cola, int socket_cliente);
void* serializar_paquete_suscripcion(t_paquete* paquete, int bytes);

#endif /* GAMECARD_H_ */
