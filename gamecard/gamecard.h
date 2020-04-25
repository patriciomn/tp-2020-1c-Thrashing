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

#define MAGIC_NUMBER "TALL_GRASS"
//#define RUTA_BITMAP  "/home/utnso/Escritorio/tall_grass/fs/metadata/bitmap.bin"
#define METADATA_DIR "/metadata"
#define METADATA_FILE "/metadata.txt"
#define BLOCKS_DIR "/blocks"
#define FILES_DIR "/files"
#define METADATA_TXT_PATH "/metadata/metadata.txt"
#define BITMAP_PATH "/metadata/bitmap.bin"
#define BITMAP_FILE "/bitmap.bin"
#define BITS 8

void crear_pto_de_montaje(char *path);
void crear_metadata_tall_grass(char *path);
void crear_archivos_metadata(char *path);
void crear_bitmap_bin(char *path_bitmap, int size_bitmap);
char *crear_nuevo_path(char* path_anterior, char *archivo);
void crear_directorio_files(char *path_pto_montaje);
void verificar_metadata_txt(char *path_pto_montaje);
void crear_directorio_blocks(char *path_pto_montaje);
void iniciar_logger_config();

t_log *logger;
t_bitarray *bitarray;
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
} ; // Tiene sentido plantear algo asi para despues responder con el id??

enum TIPO{
	NEW_POKEMON = 1,
	CATCH_POKEMON = 2,
	GET_POKEMON = 3,
	};

struct metadata_info *metadataTxt; // este puntero es para el metadata.txt que ya existe en el fs
struct config_tallGrass *datos_config; // este struct es para almacenar los datos de las config. Tambien lo utilizamos para setear los valores de metadata.txt cuando se crea la primera vez


//FUNCIONES

int main ();
void obtener_datos_archivo_config();
void verificar_punto_de_montaje();
void verificar_metadata_txt(char *path_pto_montaje);
void crear_pto_de_montaje(char *path_pto_montaje);
void crear_directorio_files(char *path_pto_montaje);
void crear_directorio_blocks(char *path_pto_montaje);
void crear_metadata_tall_grass(char *path_pto_montaje);
void crear_archivos_metadata(char *path_metadata);

int tipo_mensaje(char* tipo_mensaje);
void new_pokemon(char* pokemon,int posx,int posy,int cant);
void catch_pokemon(char* pokemon,int posx,int posy);
void get_pokemon(char*pokemon);



#endif /* GAMECARD_H_ */
