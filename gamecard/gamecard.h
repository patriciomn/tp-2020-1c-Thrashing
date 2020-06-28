#ifndef GAMECARD_H_
#define GAMECARD_H_

#include "../utils/utils.c"
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
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <netdb.h>
#include <sys/socket.h>
#include <pthread.h>
//#include "sockets.c"

#define IP_SERVIDOR		 "127.0.0.3"
#define PUERTO_SERVIDOR  "4446"

#define MAGIC_NUMBER "TALL_GRASS"
//#define RUTA_BITMAP  "/home/utnso/Escritorio/tall_grass/fs/metadata/bitmap.bin"
#define METADATA_DIR "/metadata"
#define METADATA_FILE "/metadata.txt"
#define BLOCKS_DIR "/blocks"
#define FILES_DIR "/files"
#define POKEMON_DIR "/pokemon/"
#define METADATA_TXT_PATH "/metadata/metadata.txt"
#define BITMAP_PATH "/metadata/bitmap.bin"
#define BITMAP_FILE "/bitmap.bin"
#define BITS 8
#define POKEMON_FILE_EXT ".txt"
#define TXT_FILE_EXT ".txt"
#define DIRECTORIO_POKEMON "pokemon"
#define BITMAP_FS "/home/utnso/desktop/tall-grass/Metadata/bitmap.bin"


#define GUION "-"
#define IGUAL "="

t_log *logger;
t_bitarray *bitarray;			// variable global bitmap, para manejar el bitmap siempre utilizamos esta variable
t_config *config_tall_grass;

char *ruta_archivo_bitmap;

void *bitmap_memoria;	// puntero para el mmap


enum op_bitmap { SET = 1, CLEAN = 2};

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
};

typedef struct {
	int ack_new;
	int ack_catch;
	int ack_get;
}ack_t;


typedef struct{
	int posX;
	int posY;
	int cant;
}posYcant;

typedef struct{
	int id_mensaje;
	char* name;
	t_list* posiYcant;
}rtaGet;

ack_t acks_gamecard;

FILE *bitmapFilePointer;

// el pid del proceso

pid_t pid_gamecard;

// Los siguientes structs son para recibir mensajes del gameboy

struct metadata_info *metadataTxt; // este puntero es para el metadata.txt que ya existe en el fs
struct config_tallGrass *datos_config; // este struct es para almacenar los datos de las config. Tambien lo utilizamos para setear los valores de metadata.txt cuando se crea la primera vez

// Hilos para gamecard
pthread_t servidor_gamecard; // este hilo es para iniciar el gamecard como servidor e interacturar con gameboy si el broker cae
pthread_t cliente_gamecard; // este hilo es para iniciar gamecard como cliente del broker

pthread_t servidor_gamecard;

pthread_t thread_new_pokemon;		// hilo para recibir mensajes de la cola new_pokemon
pthread_t thread_catch_pokemon;		// hilo para recibir mensajes de la cola catch_pokemon
pthread_t thread_get_pokemon;		// hilo para recibir mensajes de la cola get_pokemon

//FUNCIONES

int main ();

void verificar_punto_de_montaje();
void iniciar_logger_config();
void obtener_datos_archivo_config();
void verificar_metadata_fs(char *path_pto_montaje);
void crear_pto_de_montaje(char *path_pto_montaje);
void crear_directorio_files(char *path_pto_montaje);
void crear_directorio_blocks(char *path_pto_montaje);
void crear_metadata_tall_grass(char *path_pto_montaje);
void crear_archivos_metadata(char *path_metadata);
int existe_archivo_pokemon(char *path);
char* crear_directorio_pokemon(char *path_pto_montaje, char* pokemon);

void crear_pokemon(new_pokemon *newPokemon, char *path_directorio_pokemon, int nro_bloque_libre);

void crear_metadata_pokemon(char *path_pokeflie, char *pokemon);

void escribir_linea_en_archivo(FILE *punteroArchivo, int cantidad_bloques_necesarios, char *ruta_directorio_pokemon, char *linea);

//bool hay_cantidad_de_bloques_libres(char *path_dir_pokemon, char *datos_a_agregar);

bool hay_espacio_ultimo_bloque(char *path_dir_pokemon, char *datos_a_agregar);

void buscar_linea_en_el_archivo(new_pokemon *newPokemon, char *path_directorio_pokemon);

void insertar_linea_en_archivo(new_pokemon *newPokemon, char *path_directorio_pokemon, char *linea);

void escribir_archivo(char *path_archivo, char *linea, char *modo);

void modificar_linea_pokemon(char *fileMemory, char *viejaLinea, char *lineaActualizada, int posicionLinea, char *path_directorio_pokemon, char *pokemon);

void escribir_blocks(int ultimo_bloque, int nuevo_bloque, char *linea);

void escribir_block(int ultimo_bloque, char *linea);

void actualizar_bloque(char *mapped, int desplazamiento, char *bloque);

void modificar_linea_en_archivo(char *mapped_file, new_pokemon *newPokemon, char *ruta_directorio_pokemon, char *coordenada);

void actualizar_contenido_blocks(char *path_directorio_pokemon, char *mapped);

void agregar_bloque_metadata_pokemon(char *path_pokemon_metadata_file, int nro_bloque);
int obtener_bloque_libre();
void crear_bitmap_bin(char *path_bitmap, int size_bitmap);
char *crear_nuevo_path(char* path_anterior, char *archivo);
void escribir_datos_bloque(char *path_blocks_dir, char *datos_a_agregar, int nro_bloque);

char *read_file_into_buf (char *source, FILE *fp);

int tipo_mensaje(char* tipo_mensaje);

void cambiar_archivo_a_directorio(char *file_memory, char *path_archivo_pokemon, char *path_directorio_pokemon);

bool ultimo_bloque_queda_en_cero(int bytes_a_mover, char *path_directorio_pokemon);

void modificar_archivo_pokemon_catch_con_linea(char *fileMemory, char *viejaLinea, char *lineaActualizada, int posicionLinea, char *path_directorio_pokemon, char *pokemon);

void borrar_ultimo_bloque_metadata_blocks(char *ruta_directorio_pokemon, int nro_bloque);

//void operacion_new_pokemon(new_pokemon *newPokemon);
void operacion_get_Pokemon(get_pokemon *GETpokemon);

void modificar_bitmap_crear_blocks(int cantidad_bloques_necesarios, char *ruta_directorio_pokemon, char *linea);
void escribir_bitmap_metadata_block(char *ruta_directorio_pokemon, char *linea, int desplazamiento);

// Funciones Extras

void cambiar_valor_metadata(char *ruta_directorio_pokemon, char *campo, char *valor); //  la ruta tiene que ser: [pto_de_montaje]/files/[nombre_del_pokemon]
char* get_valor_campo_metadata(char *ruta_dir_pokemon, char *campo);
int ultimo_bloque_array_blocks(char *path_directorio_pokemon);
int valor_campo_size_metadata(char *ruta_dir_pokemon);
char** get_array_blocks_metadata(char *path_directorio_pokemon);
char* get_linea_nueva_cantidad(char *linea, char *coordenada, int cantidad_a_agregar);
int cantidad_de_bloques(char **arrayBlocks);
int get_posicion_linea_en_archivo(char *linea, char *mmaped);
int get_tamanio_linea(char *mapped);
int fileSize(char* file);
char* get_linea_nueva_cantidad_catch(char *linea, char *coordenada);
void borrar_archivo(char *nombre, char flag);
void cambiar_metadata_archivo_a_directorio(char *path_directorio_pokemon);
void limpiar_bloque_bitmap(int nro_bloque);

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------

// Sockets

// Funciones

void suscripcion_colas_broker();
void suscribirse_a_new_pokemon();
void suscribirse_a_catch_pokemon();
void suscribirse_a_get_pokemon();

void iniciar_servidor(void);
void esperar_cliente(int socket_servidor);
void atender_peticion(int socket_cliente, int cod_op);

// sockets para recibir los mensajes
int socket_cliente_np;
int socket_cliente_cp;
int socket_cliente_gp;


#endif /* GAMECARD_H_ */
