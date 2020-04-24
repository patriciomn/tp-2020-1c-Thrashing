#include "gamecard.h"

int main () {

    datos_config = malloc(sizeof(struct config_tallGrass));
    metadataTxt = malloc(sizeof(struct metadata_info));
    metadataTxt->magic_number = malloc(strlen("TALL_GRASS") + 1);

    //creamos el archivo de log, creamos el config y sacamos los datos
    logger = log_create("Tall_Grass_Logger","TG",1,LOG_LEVEL_INFO);
    config_tall_grass = config_create("/home/utnso/tp-2020-1c-Thrashing/gamecard/tall_grass.config");

    obtener_datos_archivo_config();

    verificar_punto_de_montaje();

    log_destroy(logger);
    config_destroy(config_tall_grass);
    free(metadataTxt);
    free(metadataTxt->magic_number);
    free(datos_config->ip_broker);
    free(datos_config->pto_de_montaje);
    free(datos_config);

    return 0;
}

void verificar_punto_de_montaje() {
	DIR *rta = opendir(datos_config->pto_de_montaje);
	if(rta == NULL) {
		log_warning(logger,"DIRECTORIO %s NO EXISTE",datos_config->pto_de_montaje);
	    log_info(logger,"CREANDO DIRECTORIO %s",datos_config->pto_de_montaje);
	    crear_pto_de_montaje(datos_config->pto_de_montaje);
	} else {
	    log_info(logger,"DIRECTORIO %s ENCONTRADO",datos_config->pto_de_montaje);
	    //verificamos el bitmap, el archivo metadata.txt, los archivos pokemones, entre otras cosas.
	    verificar_metadata_txt(datos_config->pto_de_montaje);
	}
}

void obtener_datos_archivo_config() {
	datos_config->tiempo_reintento_conexion = config_get_int_value(config_tall_grass, "TIEMPO_DE_REINTENTO_CONEXION");
	datos_config->tiempo_reintento_operacion = config_get_int_value(config_tall_grass,"TIEMPO_DE_REINTENTO_OPERACION");

	int size_ip = strlen(config_get_string_value(config_tall_grass, "IP_BROKER")) + 1;
	datos_config->ip_broker = malloc(size_ip);
	memcpy(datos_config->ip_broker, config_get_string_value(config_tall_grass, "IP_BROKER"), size_ip);

	int size_pto_montaje = strlen(config_get_string_value(config_tall_grass, "PUNTO_MONTAJE_TALLGRASS")) + 1;
	datos_config->pto_de_montaje = malloc(size_pto_montaje);
	memcpy(datos_config->pto_de_montaje, config_get_string_value(config_tall_grass, "PUNTO_MONTAJE_TALLGRASS"), size_pto_montaje);

	datos_config->blocks = config_get_int_value(config_tall_grass, "BLOCKS");
	datos_config->size_block = config_get_int_value(config_tall_grass, "SIZE_BLOCK");
}

// Nota: para abrir archivos usar los sgtes permisos: O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP
// Nota2: Revisar memory leaks :-(

void verificar_metadata_txt(char *path_pto_montaje) {
	t_config *metadata_txt_datos;

    int size_new_string = strlen(path_pto_montaje) + strlen(METADATA_TXT_PATH) + 1;
    char *path_metadata_txt = malloc(size_new_string);
    strcpy(path_metadata_txt, path_pto_montaje);
    string_append(&path_metadata_txt, METADATA_TXT_PATH);

    FILE *filePointer = fopen(path_metadata_txt,"r");
    if(filePointer == NULL) {
        log_error(logger,"ERROR AL ABRIR EL ARCHIVO metadata.txt");
        exit(1);
    }

    //obtenemos los valores del archivo metadata.txt
    metadata_txt_datos = config_create(path_metadata_txt);
    metadataTxt->block_size = config_get_int_value(metadata_txt_datos,"BLOCK_SIZE");
    log_info(logger,"OBTENIENDO BLOCK_SIZE NUMBER METADATA.TXT: %d",metadataTxt->block_size);
    metadataTxt->blocks = config_get_int_value(metadata_txt_datos,"BLOCKS");
    log_info(logger,"OBTENIENDO CANTIDAD DE BLOCKS NUMBER METADATA.TXT: %d",metadataTxt->blocks);
    memcpy(metadataTxt->magic_number, config_get_string_value(metadata_txt_datos,"MAGIC_NUMBER"), strlen("TALL_GRASS")+1);
    log_info(logger,"OBTENIENDO MAGIC NUMBER METADATA.TXT: %s",metadataTxt->magic_number);

    //obtenemos el bitarray del archivo bitmap.bin
    struct stat *buf = malloc(sizeof(struct stat)); // este struct lo voy a asociar con el bitmap ya existente y sacar el tamaño para el parametro size de mmap

    size_new_string = strlen(path_pto_montaje) + strlen(BITMAP_PATH) + 1;
    char *path_bitmap_bin = malloc(size_new_string);
    strcpy(path_bitmap_bin, path_pto_montaje);
    string_append(&path_bitmap_bin, BITMAP_PATH);

    int fd_bitmap = open(path_bitmap_bin, O_RDWR); // unico caso de usar una syscall para facilitar el uso del bitmap
    if(fd_bitmap == -1) {
        log_error(logger,"ERROR CON EL ARCHIVO bitmap.bin");
        exit(1);
    }
    fstat(fd_bitmap,buf);

    int size_file = buf->st_size;
    printf("Tamaño bitmap (bytes)%li\n",buf->st_size);

    log_info(logger,"OBTENER BITARRAY DEL ARCHIVO bitmap.bin");
    char *bitmap_memory = mmap(NULL , size_file, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bitmap, 0); // averiguar si hay otro modo de lidiar con el bitmap con fopen
    if(bitmap_memory == NULL) {
        log_error(logger,"ERROR MMAP bitmap.bin");
        exit(1);
    }

    log_info(logger,"COPIAR EL BITARRAY DEL ARCHIVO bitmap.bin");
    bitarray = bitarray_create_with_mode(bitmap_memory, size_file, LSB_FIRST);
    printf("\n");
    for(int i = 0 ; i < metadataTxt->blocks ; i++) {
        printf("%d",bitarray_test_bit(bitarray,i));
    }
    printf("\n");

    bitarray_destroy(bitarray);
    munmap(bitmap_memory,buf->st_size);
    config_destroy(metadata_txt_datos);
    close(fd_bitmap);
    free(path_metadata_txt);
    free(path_bitmap_bin);
}

// Funciones para crear el pto de montaje y otros directorios

void crear_pto_de_montaje(char *path_pto_montaje) {
    int rta_mkdir = mkdir(path_pto_montaje,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO %s CREADO", path_pto_montaje);
        crear_metadata_tall_grass(path_pto_montaje);
        crear_directorio_files(path_pto_montaje);
        crear_directorio_blocks(path_pto_montaje);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_pto_montaje);
    }
}

void crear_directorio_files(char *path_pto_montaje) {
    int size_new_string = strlen(path_pto_montaje) + strlen(FILES_DIR) + 1;
    char *path_files = malloc(size_new_string);
    strcpy(path_files, path_pto_montaje);
    string_append(&path_files, FILES_DIR);

    int rta_mkdir = mkdir(path_files, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO %s CREADO", path_files);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_files);
    }

    free(path_files);
}

void crear_directorio_blocks(char *path_pto_montaje) {
    int size_new_string = strlen(path_pto_montaje) + strlen(BLOCKS_DIR) + 1;
    char *path_blocks = malloc(size_new_string);
    strcpy(path_blocks, path_pto_montaje);
    string_append(&path_blocks, BLOCKS_DIR);

    int rta_mkdir = mkdir(path_blocks, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO %s CREADO", path_blocks);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_blocks);
    }

    free(path_blocks);
}

void crear_metadata_tall_grass(char *path_pto_montaje) {
    int size_new_string = strlen(path_pto_montaje) + strlen(METADATA_DIR) + 1;
    char *path_metadata = malloc(size_new_string);
    strcpy(path_metadata, path_pto_montaje);
    string_append(&path_metadata, METADATA_DIR);

    int rta_mkdir = mkdir(path_metadata,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO %s CREADO",path_metadata);
        crear_archivos_metadata(path_metadata);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_metadata);
    }

    free(path_metadata);
}

void crear_archivos_metadata(char *path_metadata) {
    //creamos el archivo metadata.txt
    //creamos un nuevo string que va a ser el path del metadata.txt
    FILE *metadataPointerFile;
	int size_new_string = strlen(path_metadata) + strlen(METADATA_FILE) + 1;
    char *path_metadata_file = malloc(size_new_string);
    strcpy(path_metadata_file, path_metadata);
    string_append(&path_metadata_file, METADATA_FILE);

    t_config *metadata_info = config_create(path_metadata_file);

    //int fd = open(path_metadata_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    metadataPointerFile = fopen(path_metadata_file,"a");
    if(metadataPointerFile == NULL) {
    	log_error(logger,"ERROR AL CREAR EL ARCHIVO metadata.txt");
    	exit(1);
    } else {
        metadata_info = config_create(path_metadata_file);

        config_set_value(metadata_info, "BLOCK_SIZE", string_itoa(datos_config->size_block));
        config_set_value(metadata_info, "BLOCKS", string_itoa(datos_config->blocks));
        config_set_value(metadata_info, "MAGIC_NUMBER", "TALL_GRASS");
        config_save(metadata_info);
    }

    //creamos el bitmap.bin
    //creamos un nuevo string que va a ser el path del bitmap.bin
    size_new_string = strlen(path_metadata) + strlen(BITMAP_FILE) + 1;
    char *path_bitmap_file = malloc(size_new_string);
    strcpy(path_bitmap_file, path_metadata);
    string_append(&path_bitmap_file, BITMAP_FILE);

    metadataTxt->blocks = config_get_int_value(config_tall_grass,"BLOCKS"); //revisar metadataTxt->blocks
    int size_bytes = (datos_config->blocks / BITS) + 1; //redonedeamos para arriba los bytes necesarios. Si por ejemplo tengo que hacer un bitarray de 10 bloques, el tamaño va a ser de 2 bytes (10/8 = 1,25 => 2 )
    printf("%d",size_bytes);
    printf("\n");

    int fd_bitmap = open(path_bitmap_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if(fd_bitmap == -1) {
        log_error(logger,"ERROR AL CREAR EL ARCHIVO bitmap.bin");
        exit(1);
    }
    write(fd_bitmap,"",size_bytes); //dejamos preparado el archivo para luego meterle el vector de bits (bitarray)

    //montamos el archivo en memoria
    void *file_memory = mmap(NULL, size_bytes, PROT_WRITE, MAP_SHARED, fd_bitmap, 0);
    if(file_memory == NULL) {
        log_error(logger,"ERROR MMAP bitmap.bin");
        exit(1);
    }

    //una vez montado en memoria, "enlazamos" void *file_memory al bitarray y seteamos todos los bloques en 0
    bitarray = bitarray_create_with_mode(file_memory, size_bytes, LSB_FIRST);
    for(int i = 0 ; i < metadataTxt->blocks ; i++) {
        bitarray_clean_bit(bitarray, i);
    }

    //solo seteo dos bloques a modo de prueba
    bitarray_set_bit(bitarray,0);
    bitarray_set_bit(bitarray,1);

    fclose(metadataPointerFile);
    //close(fd_bitmap);
    //munmap(file_memory, size_bytes);
    config_destroy(metadata_info);
    free(path_metadata_file);
    free(path_bitmap_file);
}

int tipo_mensaje(char* tipo_mensaje){ //robado de Gameboy, robar es malo
	log_info(logger,"TIPO_MENSAJE: %s",tipo_mensaje);
	if(strcmp(tipo_mensaje,"NEW_POKEMON") == 0){
		return NEW_POKEMON;
	}
	else if(strcmp(tipo_mensaje,"CATCH_POKEMON") == 0){
		return CATCH_POKEMON;
	}
	else if(strcmp(tipo_mensaje,"GET_POKEMON") == 0){
		return GET_POKEMON;
	}
	return -1;
}

//estan con void por las dudas, nada particular
void new_pokemon(char* pokemon,int posx,int posy,int cant){
}

void catch_pokemon(char* pokemon,int posx,int posy){
}

void get_pokemon(char*pokemon){
}

//metadataTxt->blocks = config_get_int_value(config_tall_grass,"BLOCKS");
