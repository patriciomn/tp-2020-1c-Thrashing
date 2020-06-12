#ifndef GAMECARD_H_
#define GAMECARD_H_

#include <commons/bitarray.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>
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
#include <uuid/uuid.h>
#include "sockets.h"

//#include "../utils/utils.h"

#define IP_SERVIDOR		 "127.0.0.3"
#define PUERTO_SERVIDOR  "4446"

#define MAGIC_NUMBER "TALL_GRASS"
//#define RUTA_BITMAP  "/home/utnso/Escritorio/tall_grass/fs/metadata/bitmap.bin"
#define METADATA_DIR "/metadata"
#define METADATA_FILE "/metadata.txt"
#define BLOCKS_DIR "/blocks"
#define FILES_DIR "/files"
#define POKEMON_DIR "/files/"
#define METADATA_TXT_PATH "/metadata/metadata.txt"
#define BITMAP_PATH "/metadata/bitmap.bin"
#define BITMAP_FILE "/bitmap.bin"
#define BITS 8
#define POKEMON_FILE_EXT ".txt"
#define BITMAP_FS "/home/utnso/desktop/tall-grass/Metadata/bitmap.bin"
#define PATH_POKECARPETA "/home/utnso/desktop/tall-grass/files/"

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
	int tiempo_retardo_operacion;
    char *ip_broker;
    char *pto_de_montaje;
    int puerto_broker;
    int size_block;
    int blocks;
};

struct dataServer{
	char *ipGamecard;
	int puertoGamecard;
};

struct mensaje {
    int id;
    void* message;
} ;

// Los siguientes structs son para recibir mensajes del gameboy

struct metadata_info *metadataTxt; // este puntero es para el metadata.txt que ya existe en el fs
struct config_tallGrass *datos_config; // este struct es para almacenar los datos de las config. Tambien lo utilizamos para setear los valores de metadata.txt cuando se crea la primera vez

// Hilos para gamecard
pthread_t servidor_gamecard; // este hilo es para iniciar el gamecard como servidor e interacturar con gameboy si el broker cae
pthread_t cliente_gamecard; // este hilo es para iniciar gamecard como cliente del broker

pthread_t thread_new_pokemon;		// hilo para recibir mensajes de la cola new_pokemon
pthread_t thread_catch_pokemon;		// hilo para recibir mensajes de la cola catch_pokemon
pthread_t thread_get_pokemon;		// hilo para recibir mensajes de la cola get_pokemon

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
char* crear_directorio_pokemon(char *path_pto_montaje, char* pokemon);
void crear_archivos_pokemon(char *path_pokefile, int posX, int posY, int cantidad);
void crear_metadata_pokemon(char *path_pokeflie);

void agregar_bloque_metadata_pokemon(char *path_pokemon_metadata_file, int nro_bloque);
int obtener_bloque_libre();
void crear_bitmap_bin(char *path_bitmap, int size_bitmap);
char *crear_nuevo_path(char* path_anterior, char *archivo);
void escribir_datos_bloque(char *path_blocks_dir, char *datos_a_agregar, int nro_bloque);
int fileSize(char* file);
FILE* existePokemon(char* nombrePokemon);
char *read_file_into_buf (char **filebuf, long *fplen, FILE *fp);

int tipo_mensaje(char* tipo_mensaje);
void newPokemon(char* pokemon,int posx,int posy,int cant);
void catchPokemon(char* pokemon,int posx,int posy);
rtaGet* getPokemon(int idMensaje, char* pokemon);



#endif /* GAMECARD_H_ */
