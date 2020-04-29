#include "gamecard.h"

int main () {

    iniciar_logger_config();

    obtener_datos_archivo_config();

    verificar_punto_de_montaje();

    bitarray_destroy(bitarray);
    log_destroy(logger);
    config_destroy(config_tall_grass);
    free(metadataTxt);
    //free(datos_config->ip_broker);
    //free(datos_config->pto_de_montaje);
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

void iniciar_logger_config() {
	datos_config = malloc(sizeof(struct config_tallGrass));
	 metadataTxt = malloc(sizeof(struct metadata_info));

	logger = log_create("Tall_Grass_Logger","TG",1,LOG_LEVEL_INFO);
	config_tall_grass = config_create("/home/utnso/tp-2020-1c-Thrashing/gamecard/tall_grass.config");
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

// Nota: Revisar memory leaks :-(
// Nota2: Para un futuro lejano hacer un dump del bitmap

void verificar_metadata_txt(char *path_pto_montaje) {

	//
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
    t_config *metadata_txt_datos = config_create(path_metadata_txt);
    metadataTxt->block_size = config_get_int_value(metadata_txt_datos,"BLOCK_SIZE");
    log_info(logger,"OBTENIENDO BLOCK_SIZE NUMBER METADATA.TXT: %d",metadataTxt->block_size);
    metadataTxt->blocks = config_get_int_value(metadata_txt_datos,"BLOCKS");
    log_info(logger,"OBTENIENDO CANTIDAD DE BLOCKS NUMBER METADATA.TXT: %d",metadataTxt->blocks);

    config_destroy(metadata_txt_datos);
    fclose(filePointer);
    free(path_metadata_txt);


    // creamos un nuevo string para armar el path de la metadata
    size_new_string = strlen(path_pto_montaje) + strlen(BITMAP_PATH) + 1;
    char *path_bitmap_bin = malloc(size_new_string);
    strcpy(path_bitmap_bin, path_pto_montaje);
    string_append(&path_bitmap_bin, BITMAP_PATH);

    log_info(logger,"OBTENER BITARRAY DEL ARCHIVO bitmap.bin");
    // abrimos el archivo bitmap.bin para obtener el bitmap
    FILE *bitmap_fp = fopen(path_bitmap_bin, "rb+");
    if(bitmap_fp == NULL) {
    	log_error(logger, "ERROR AL ABRIR EL ARCHIVO bitmap.bin");
    	exit(1);
    }

    fseek(bitmap_fp, 0, SEEK_END);				// posiciono el puntero hasta el final del archivo (EOF)
    int size_bitmap = (int) ftell(bitmap_fp);	// obtengo la posicion actual del puntero y lo casteo a un int. Lo hago para sacar el tamaño del archivo
    fseek(bitmap_fp, 0, SEEK_SET);				// vuelvo a posicionar el puntero al inicio del archivo
    //log_info(logger, "tamaño bitmap:%d",size_bitmap);

    void *buffer_bitmap = malloc(size_bitmap);

    if(fread(buffer_bitmap, sizeof(char), size_bitmap, bitmap_fp) == 0) {
    	log_error(logger, "NO SE HA PODIDO LEER EL CONTENIDO DEL ARCHIVO bitmap.bin");
    	exit(1);
    }

    bitarray = bitarray_create_with_mode(buffer_bitmap, size_bitmap, LSB_FIRST);

    for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++) {
        printf("%d",bitarray_test_bit(bitarray,i));
    }
    printf("\n");

    fclose(bitmap_fp);
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

void crear_metadata_tall_grass(char *path_pto_montaje) { //incluir en bitmap, supongo que en el primer bit
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

    //creamos un nuevo string que va a ser el path del metadata.txt
	int size_new_string = strlen(path_metadata) + strlen(METADATA_FILE) + 1;
    char *path_metadata_file = malloc(size_new_string);
    strcpy(path_metadata_file, path_metadata);
    string_append(&path_metadata_file, METADATA_FILE);

    FILE *metadataPointerFile = fopen(path_metadata_file,"w+");
    if(metadataPointerFile == NULL) {
    	log_error(logger, "ERROR AL CREAR EL ARCHIVO metadata.txt");
    	exit(1);
    }

    char *info_metadataTxt = string_new();
    string_append(&info_metadataTxt, "BLOCK_SIZE=");
    string_append(&info_metadataTxt, string_itoa(datos_config->size_block));
    string_append(&info_metadataTxt, "\n");
    string_append(&info_metadataTxt, "BLOCKS=");
    string_append(&info_metadataTxt, string_itoa(datos_config->blocks));
    string_append(&info_metadataTxt, "\n");
    string_append(&info_metadataTxt, "MAGIC_NUMBER=TALL_GRASS");
    string_append(&info_metadataTxt, "\n");

    if(fwrite(info_metadataTxt, string_length(info_metadataTxt), 1, metadataPointerFile) == 0) {
    	log_error(logger, "ERROR AL ESCRIBIR EL ARCHIVO metadata.txt");
    	exit(1);	// despues ver de implementar un goto en vez de exit()
    }

    free(info_metadataTxt);
    fclose(metadataPointerFile);

    //creamos un nuevo string que va a ser el path del bitmap.bin
    size_new_string = strlen(path_metadata) + strlen(BITMAP_FILE) + 1; // uso la misma variable que utilice en metadata.txt
    char *path_bitmap_file = malloc(size_new_string);
    strcpy(path_bitmap_file, path_metadata);
    string_append(&path_bitmap_file, BITMAP_FILE);

    // creamos el archivo bitmap.bin
    FILE *bitmapFilePointer = fopen(path_bitmap_file, "wb+");
    if(bitmapFilePointer == NULL) {
    	log_error(logger, "ERROR AL CREAR EL ARCHIVO bitmap.bin");
    }

    int size_bytes_bitmap = (datos_config->blocks / BITS);
    void *newArrayBit = malloc(size_bytes_bitmap);
    t_bitarray *bitArray = bitarray_create_with_mode(newArrayBit, size_bytes_bitmap, LSB_FIRST);

    for(int i = 0 ; i < bitarray_get_max_bit(bitArray) ; i++) {
    	bitarray_clean_bit(bitArray,i);
    }

    //solo seteo dos bloques a modo de prueba, despues lo borro
    bitarray_set_bit(bitArray,0);
    bitarray_set_bit(bitArray,1);

    //escribimos el bitarray en el archivo
    if(fwrite(bitArray->bitarray, sizeof(char), size_bytes_bitmap, bitmapFilePointer) == 0) {
    	log_error(logger, "NO SE PUDO ESCRIBIR EL BITARRAY EN EL ARCHIVO bitmap.bin");
    	exit(1);
    }

    fclose(bitmapFilePointer);
    bitarray_destroy(bitArray);

    free(path_metadata_file);
    free(path_bitmap_file);
}

void crear_archivos_pokemon(char *path_pokefile) {

	int size_new_string = strlen(path_pokefile) + strlen(POKEMON_FILE) + 1;
    char *path_poke_file = malloc(size_new_string);
    strcpy(path_poke_file, path_pokefile);
    string_append(&path_poke_file, POKEMON_FILE);

    FILE *pointerFile = fopen(path_poke_file,"r");//con leer alcanza y sobra. porque la parte de poner posiciones las hacemos con NEW
    if(pointerFile == NULL) {
    	log_error(logger, "ERROR AL CREAR EL ARCHIVO POKEMON");
    	exit(1);
    }
    fclose(pointerFile);

 /*   //ahora agregarlo al bitmap
    size_new_string = strlen(path_pokefile) + strlen(BITMAP_FILE) + 1;
    char *path_bitmap_file = malloc(size_new_string);
    strcpy(path_bitmap_file, path_pokefile);
    string_append(&path_bitmap_file, BITMAP_FILE);
    }
¿Como llego al bit que representa al archivo?
    bitarray_set_bit(bitArray,indice que representa al archivo);

    //escribimos el bitarray en el archivo
    if(fwrite(bitArray->bitarray, sizeof(char), size_bytes_bitmap, bitmapFilePointer) == 0) {
    	log_error(logger, "NO SE PUDO ESCRIBIR EL BITARRAY EN EL ARCHIVO bitmap.bin");
    	exit(1);
    }

    fclose(bitmapFilePointer);
    bitarray_destroy(bitArray);

    free(path_pokefile_file);
    free(path_bitmap_file);*/
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


void new_pokemon(char* pokemon,int posx,int posy,int cant){}

void catch_pokemon(char* pokemon,int posx,int posy){}

void get_pokemon(char*pokemon){}

