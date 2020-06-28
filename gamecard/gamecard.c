#include "gamecard.h"

int main () {

	pid_gamecard = getpid();

    iniciar_logger_config();

    obtener_datos_archivo_config();

    verificar_punto_de_montaje();

   suscripcion_colas_broker();

    pthread_join(thread_new_pokemon, NULL);
    pthread_join(thread_catch_pokemon, NULL);
    pthread_join(thread_get_pokemon, NULL);

///Area para probar cosas

//-----***------***-----

    munmap(bitmap_memoria, 2);
    bitarray_destroy(bitarray);
    log_destroy(logger);
    config_destroy(config_tall_grass);
    free(metadataTxt);
    free(datos_config->ip_broker);
    free(datos_config->pto_de_montaje);
    free(datos_config);

    return 0;
}

void verificar_punto_de_montaje() {
	DIR *rta = opendir(datos_config->pto_de_montaje);
	if(rta == NULL) {
        // si no existe, se crea
		log_warning(logger,"DIRECTORIO %s NO EXISTE",datos_config->pto_de_montaje);
	    log_info(logger,"CREANDO DIRECTORIO %s",datos_config->pto_de_montaje);
	    crear_pto_de_montaje(datos_config->pto_de_montaje);
	} else {
	    log_info(logger,"DIRECTORIO %s ENCONTRADO",datos_config->pto_de_montaje);
	    //verificamos el bitmap, el archivo metadata.txt, los archivos pokemones, entre otras cosas.
	    verificar_metadata_fs(datos_config->pto_de_montaje);
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
	datos_config->tiempo_retardo_operacion = config_get_int_value(config_tall_grass, "TIEMPO_RETARDO_OPERACION");

	int size_ip = strlen(config_get_string_value(config_tall_grass, "IP_BROKER")) + 1;
	datos_config->ip_broker = malloc(size_ip);
	memcpy(datos_config->ip_broker, config_get_string_value(config_tall_grass, "IP_BROKER"), size_ip);

	datos_config->puerto_broker = config_get_int_value(config_tall_grass, "PUERTO_BROKER");

	int size_pto_montaje = strlen(config_get_string_value(config_tall_grass, "PUNTO_MONTAJE_TALLGRASS")) + 1;
	datos_config->pto_de_montaje = malloc(size_pto_montaje);
	memcpy(datos_config->pto_de_montaje, config_get_string_value(config_tall_grass, "PUNTO_MONTAJE_TALLGRASS"), size_pto_montaje);

	datos_config->blocks = config_get_int_value(config_tall_grass, "BLOCKS");
	datos_config->size_block = config_get_int_value(config_tall_grass, "SIZE_BLOCK");
}

// Nota: Revisar memory leaks :-(
// Nota2: Para un futuro lejano hacer un dump del bitmap

void verificar_metadata_fs(char *path_pto_montaje) {

    int size_new_string = strlen(path_pto_montaje) + strlen(METADATA_TXT_PATH) + 1;
    char *path_metadata_txt = malloc(size_new_string);
    strcpy(path_metadata_txt, path_pto_montaje);
    string_append(&path_metadata_txt, METADATA_TXT_PATH);

    log_info(logger, "ABRIENDO METADATA FS CON RUTA <%s>", path_metadata_txt);
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

    // creamos un nuevo string para armar el path del bitmap
    size_new_string = strlen(path_pto_montaje) + strlen(BITMAP_PATH) + 1;
    char *path_bitmap_bin = malloc(size_new_string);
    strcpy(path_bitmap_bin, path_pto_montaje);
    string_append(&path_bitmap_bin, BITMAP_PATH);

    int sizeBitmap = fileSize(path_bitmap_bin);

    log_info(logger, "TAMAÑO DEL BITMAP: %d", sizeBitmap);

    int fd = open(path_bitmap_bin, O_RDWR , (mode_t)0600);
    if(fd != -1) {
    	bitmap_memoria = mmap(0, sizeBitmap, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    	if(bitmap_memoria == MAP_FAILED) {
    		log_error(logger, "ERROR AL CARGAR BITMAP A MEMORIA");
    		close(fd);
    		exit(1);
    	}

    close(fd);

    bitarray = bitarray_create_with_mode(bitmap_memoria, sizeBitmap, LSB_FIRST);

    }

    for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++) {
        printf("%d",bitarray_test_bit(bitarray,i));
    }
    printf("\n");

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

    fclose(metadataPointerFile);

    //creamos un nuevo string que va a ser el path del bitmap.bin
    size_new_string = strlen(path_metadata) + strlen(BITMAP_FILE) + 1; // uso la misma variable que utilice en metadata.txt
    char *ruta_archivo_bitmap = malloc(size_new_string);
    strcpy(ruta_archivo_bitmap, path_metadata);
    string_append(&ruta_archivo_bitmap, BITMAP_FILE);

    // creamos el archivo bitmap.bin
    //bitmapFilePointer = fopen(ruta_archivo_bitmap, "wb+");
    //if(bitmapFilePointer == NULL) {
    	//log_error(logger, "ERROR AL CREAR EL ARCHIVO bitmap.bin");
    //}

    int size_bytes_bitmap = (datos_config->blocks / BITS);

    int fd = open (ruta_archivo_bitmap, O_RDWR | O_CREAT | O_APPEND,  S_IRUSR | S_IWUSR);
    write(fd, "", size_bytes_bitmap);

    bitmap_memoria = mmap(0, size_bytes_bitmap, PROT_WRITE, MAP_SHARED, fd, 0);

    //void *newArrayBit = malloc(size_bytes_bitmap);
    bitarray = bitarray_create_with_mode(bitmap_memoria, size_bytes_bitmap, LSB_FIRST);

    close(fd);

    // limpiamos el bitarray
    for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++) {
    	bitarray_clean_bit(bitarray,i);
    }

    //escribimos el bitarray en el archivo
    //if(fwrite(bitarray->bitarray, sizeof(char), size_bytes_bitmap, bitmapFilePointer) == 0) {
    	//log_error(logger, "NO SE PUDO ESCRIBIR EL BITARRAY EN EL ARCHIVO bitmap.bin");
    	//exit(1);
    //}

    //fclose(bitmapFilePointer);

    free(path_metadata_file);
    free(info_metadataTxt);
    free(ruta_archivo_bitmap);
}

//CREO QUE ESTO ES NECESARIO, SI VES EN LA PAG 18 EL SEGUNDO EJEMPLO DE PATH
char* crear_directorio_pokemon(char *path_pto_montaje, char* pokemon) {
		
    int size_new_string = strlen(path_pto_montaje) + strlen(POKEMON_DIR)+strlen(pokemon) + 1;
    char *path_files = malloc(size_new_string);
    strcpy(path_files, path_pto_montaje);
    string_append(&path_files, POKEMON_DIR);
	string_append(&path_files, pokemon);


    int rta_mkdir = mkdir(path_files, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(rta_mkdir == 0) {
        log_info(logger,"DIRECTORIO %s CREADO", path_files);
    } else {
        log_error(logger,"ERROR AL CREAR EL DIRECTORIO %s",path_files);
    }
	return path_files;
    free(path_files);
}

//----------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------
// Operacion NEW POKEMON

void operacion_new_pokemon(new_pokemon *newPokemon) {

    char *path_directorio_pokemon = string_new();
    string_append_with_format(&path_directorio_pokemon, "%s%s%s%s",datos_config->pto_de_montaje, FILES_DIR, "/", newPokemon->name);
    log_info(logger, "EN BUSCA DEL DIRECTORIO DEL POKEMON CON PATH <%s>", path_directorio_pokemon);

    if(opendir(path_directorio_pokemon) == NULL) {
    	log_warning(logger, "EL DIRECTORIO <%s> NO EXISTE", path_directorio_pokemon);

    	//mutex_lock
    	int nro_bloque_libre = obtener_bloque_libre();
    	//mutex_unlock (cuidado, si muere el hilo me temo que no se desbloquea)

    	crear_pokemon(newPokemon, path_directorio_pokemon, nro_bloque_libre);
    	//enviar_respuesta_a_broker
    } else {

    	char *valor = get_valor_campo_metadata(path_directorio_pokemon, "DIRECTORY");
    	//char *expected = "Y";

    	if(string_equals_ignore_case(valor, "Y")) {

    		log_info(logger, "EL DIRECTORIO <%s> EXISTE Y ES SOLO UN DIRECTORIO", path_directorio_pokemon);

    		//mutex_lock
    		int nro_bloque_libre = obtener_bloque_libre();
    		// mutex_unlock

    		crear_pokemon(newPokemon, path_directorio_pokemon, nro_bloque_libre);
    		// enviar_respuesta_a_broker
    	} else {

    		log_info(logger, "EL DIRECTORIO <%s> EXISTE JUNTO CON EL ARCHIVO POKEMON", path_directorio_pokemon);
    		// el archivo existe y hay que verificar si existe la linea
    		//mutex_lock
    		char *valor_open = get_valor_campo_metadata(path_directorio_pokemon, "OPEN");
    		if(string_equals_ignore_case(valor_open, "N")) {

    			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "Y");
    			// mutex_unlock
    			buscar_linea_en_el_archivo(newPokemon, path_directorio_pokemon);

    		} else {
    			// hacer reintento de operacion
    			log_warning(logger, "HILO EN STANDBY DE OPERACION");
    			sleep(10);
    			//pthread_cancel(thread_new_pokemon);
    			//pthread_create(&atender_new_pokemon, NULL, (void *) operacion_new_pokemon, (void *) newPokemon);
    			//pthread_join(atender_new_pokemon, NULL);
    			//log_info(logger, "HILO RETOMANDO LA OPERACION");
    		}
    	}
    }
    free(path_directorio_pokemon);
}


void buscar_linea_en_el_archivo(new_pokemon *newPokemon, char *path_directorio_pokemon) { // verificamos si existe o no la linea en el archivo existente

	char *path_archivo_pokemon = string_new();
	string_append(&path_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&path_archivo_pokemon, "%s%s%s", "/", newPokemon->name, POKEMON_FILE_EXT);

	int tamanioArchivo = fileSize(path_archivo_pokemon);

	log_info(logger, "ABRIENDO ARCHIVO CON RUTA <%s>", path_directorio_pokemon);

	int fdPokemon = open(path_archivo_pokemon, O_RDONLY);

	char *file_memory = mmap(0, tamanioArchivo, PROT_READ, MAP_SHARED, fdPokemon, 0);

	char *linea = string_new();
	string_append_with_format(&linea, "%s%s%s", string_itoa(newPokemon->pos.posx), GUION, string_itoa(newPokemon->pos.posy));

	if(string_contains(file_memory, linea)) { // buscar en el archivo y modificar
		log_info(logger, "COORDENADA <%s> ENCONTRADO", linea);
		log_info(logger, "LINEA <%s>", linea);
		modificar_linea_en_archivo(file_memory, newPokemon, path_directorio_pokemon, linea);
	} else { // se agrega al final del archivo

		log_info(logger, "COORDENADA <%s> NO ENCONTRADO!", linea);
		string_append_with_format(&linea, "%s%s%s", IGUAL, string_itoa(newPokemon->cantidad), "\n");
		log_info(logger, "LINEA <%s>", linea);
		munmap(file_memory, tamanioArchivo);
		insertar_linea_en_archivo(newPokemon, path_directorio_pokemon, linea);
	}

	free(path_archivo_pokemon);
	free(linea);
}


void insertar_linea_en_archivo(new_pokemon *newPokemon, char *path_directorio_pokemon, char *linea) { // si hay espacio en el ultimo bloque, se escribe en la ultima posicion del archivo. Caso contrario se busca un nuevo bloque

	char *path_archivo_pokemon = string_new();
	string_append(&path_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&path_archivo_pokemon, "%s%s%s", "/", newPokemon->name, ".txt");

	if(hay_espacio_ultimo_bloque(path_directorio_pokemon, linea)) {
		log_info(logger, "HAY ESPACIO EN EL ULTIMO BLOQUE");
		escribir_archivo(path_archivo_pokemon, linea, "a");

		int ultimo_bloque = ultimo_bloque_array_blocks(path_directorio_pokemon);
		escribir_block(ultimo_bloque, linea);

		// actualizar size_archivo
		int viejoSize = valor_campo_size_metadata(path_directorio_pokemon);
		int nuevoSize = viejoSize + strlen(linea);

		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(nuevoSize));
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

	} else {

		// buscar un bloque para la nueva linea
		// mutex_lock
		int	nro_bloque_libre = obtener_bloque_libre();
		// mutex_unlock

		if(nro_bloque_libre == -1) {
			log_error(logger, "NO HAY SUFICIENTE ESPACIO DISPONIBLE PARA ESCRIBIR NUEVOS DATOS");
			pthread_cancel(thread_new_pokemon);
		}

		escribir_archivo(path_archivo_pokemon, linea, "a");

		int ultimo_bloque = ultimo_bloque_array_blocks(path_directorio_pokemon);

		escribir_blocks(ultimo_bloque, nro_bloque_libre, linea);

		agregar_bloque_metadata_pokemon(path_directorio_pokemon, nro_bloque_libre);

		int viejoSize = valor_campo_size_metadata(path_directorio_pokemon);
		int nuevoSize = viejoSize + strlen(linea);
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(nuevoSize));

		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");
	}
}


void escribir_archivo(char *path_archivo, char *linea, char *modo) {

	FILE *pfPokemon = fopen(path_archivo, modo);

	int size_line = strlen(linea);

	log_info(logger, "ESCRIBIENDO <%s> EN EL ARCHIVO CON RUTA <%s>", linea, path_archivo);
	if(fwrite(linea, size_line, 1, pfPokemon) == 0) {
		log_error(logger, "NO SE PUDO ESCRIBIR <%s> EN EL ARCHIVO CON RUTA <%s>", linea, path_archivo);
		exit(1);
	}

	fclose(pfPokemon);
}


void escribir_blocks(int ultimo_bloque, int nuevo_bloque, char *linea) {

	char *path_ultimo_bloque = string_new();
	string_append_with_format(&path_ultimo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", string_itoa(ultimo_bloque), ".txt");

	int tamanioArchivoBloque = fileSize(path_ultimo_bloque);

	int firstChars = datos_config->size_block - tamanioArchivoBloque;
	int start = 0;

	char *primerosChars = string_duplicate(string_substring(linea, start, firstChars)); // los primeros n caracteres a agregar en el ultimo bloque
	escribir_archivo(path_ultimo_bloque, primerosChars, "a");

	start += firstChars;

	char *path_nuevo_bloque = string_new();
	string_append_with_format(&path_nuevo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", string_itoa(nuevo_bloque), ".txt");

	int size_linea = strlen(linea);

	char *restoChars = string_duplicate(string_substring(linea, start, size_linea - start)); // los ultimos n caracteres que restan agregar
	escribir_archivo(path_nuevo_bloque, restoChars, "a");

	free(path_ultimo_bloque);
	free(restoChars);
	free(primerosChars);
}


void escribir_block(int ultimo_bloque, char *linea) {

	char *path_ultimo_bloque = string_new();
	string_append_with_format(&path_ultimo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", string_itoa(ultimo_bloque), ".txt");

	escribir_archivo(path_ultimo_bloque, linea, "a");

	free(path_ultimo_bloque);
}


char* get_valor_campo_metadata(char *ruta_dir_pokemon, char *campo) { // para todos los campos menos el array blocks

	char *path_metadata_pokemon = string_new();
	string_append(&path_metadata_pokemon, ruta_dir_pokemon);
	string_append(&path_metadata_pokemon, METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_pokemon);

	char *valor = config_get_string_value(metadata_pokemon, campo);

	//config_destroy(metadata_pokemon);
	//free(path_metadata_pokemon);

	return valor;
}


int valor_campo_size_metadata(char *ruta_dir_pokemon) { // se puede generalizar

	char *path_metadata_pokemon = string_new();
	string_append(&path_metadata_pokemon, ruta_dir_pokemon);
	string_append(&path_metadata_pokemon, METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_pokemon);

	return config_get_int_value(metadata_pokemon, "SIZE");

}

// esta funcion sirve solamente para cuando no existe el archivo pokemon (REVISAR!)

void crear_pokemon(new_pokemon *newPokemon, char *path_directorio_pokemon, int nro_bloque_libre) {

	log_info(logger, "CREANDO EL DIRECTORIO <%s>", path_directorio_pokemon);
	mkdir(path_directorio_pokemon,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

	char *linea = string_new();
	string_append_with_format(&linea, "%s%s%s%s%s", string_itoa(newPokemon->pos.posx), GUION, string_itoa(newPokemon->pos.posy), IGUAL, string_itoa(newPokemon->cantidad));
	string_append(&linea, "\n");
	log_info(logger, "LINEA A AGREGAR EN EL ARCHIVO <%s>", linea);

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", newPokemon->name, POKEMON_FILE_EXT);

	crear_metadata_pokemon(path_directorio_pokemon, newPokemon->name);

	escribir_archivo(ruta_archivo_pokemon, linea, "a");

	escribir_block(nro_bloque_libre, linea);

	agregar_bloque_metadata_pokemon(path_directorio_pokemon, nro_bloque_libre);

	cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(strlen(linea)));

	cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

	free(linea);
    free(ruta_archivo_pokemon);
}


void cambiar_valor_metadata(char *ruta_directorio_pokemon, char *campo, char *valor) {

	char *ruta_metadata_pokemon = string_new();
	string_append(&ruta_metadata_pokemon, ruta_directorio_pokemon);
	string_append(&ruta_metadata_pokemon, METADATA_FILE);

	t_config *metadata_pokemon = config_create(ruta_metadata_pokemon);

	config_set_value(metadata_pokemon, campo, valor);

	config_save(metadata_pokemon);

	config_destroy(metadata_pokemon);

	free(ruta_metadata_pokemon);
}


// nos devuelve el nro de bloque libre
int obtener_bloque_libre() {
	int bloque = -1;
	for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++ ) {
		if(bitarray_test_bit(bitarray, i) == 0) {
			log_info(logger, "NUMERO DE BLOQUE LIBRE:%d",i);
			bitarray_set_bit(bitarray, i);
			bloque = i;
			break;
		}
	}

	if(bloque == -1) {
		log_error(logger, "NO HAY SUFICIENTE ESPACIO EN EL FILE SYSTEM");
		// muere el hilo
	}

	return bloque;
}


bool hay_espacio_ultimo_bloque(char *path_dir_pokemon, char *datos_a_agregar) { // chequea si se puede agregar los datos en el ultimo bloque

	// chequea que haya espacio en el ultimo bloque
	char *path_metadata_file = string_new();
	string_append(&path_metadata_file, path_dir_pokemon);
	string_append(&path_metadata_file, METADATA_FILE);

	int ultimo_bloque = ultimo_bloque_array_blocks(path_dir_pokemon);
	log_info(logger, "ultimo bloque: %d", ultimo_bloque);

	char *path_block = string_new();
	string_append(&path_block, datos_config->pto_de_montaje);
	string_append(&path_block, BLOCKS_DIR);
	string_append_with_format(&path_block, "%s%s%s", "/", string_itoa(ultimo_bloque), TXT_FILE_EXT);

	int file_size_block = fileSize(path_block);

	int data_size = strlen(datos_a_agregar);

	int nuevoTamanio = file_size_block + data_size; // el tamaño actual del archivo bloque + el tamaño de la linea a agregar

	if(datos_config->size_block >= nuevoTamanio) {
		log_info(logger, "SE PUEDE AGREGAR LA LINEA EN EL ULTIMO BLOQUE");
		return true;
	} else {
		log_warning(logger, "SE NECESITA UN NUEVO BLOQUE PARA LA ESCRITURA");
		return false;
	}

	// hacer free de path_metadata_file, free de path_block y config_destroy
}


int ultimo_bloque_array_blocks(char *path_directorio_pokemon) { // nos devuelve el ultimo bloque del array blocks de la metadata

	int size_new_string = strlen(path_directorio_pokemon) + strlen(METADATA_FILE) + 1;
	char *path_metadata_file = malloc(size_new_string);
	strcpy(path_metadata_file, path_directorio_pokemon);
	string_append(&path_metadata_file, METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_file);
	char **array_blocks = config_get_array_value(metadata_pokemon, "BLOCKS");

	int i = 0;

	while(array_blocks[i] != NULL) {
		i++;
	}

	config_destroy(metadata_pokemon);
	free(path_metadata_file);

	return atoi(array_blocks[i - 1]);
}


void agregar_bloque_metadata_pokemon(char *ruta_directorio_pokemon, int nro_bloque) { // agregar un nro de bloque al campo blocks de la metadata

	int size_nro_bloque = strlen(string_itoa(nro_bloque));

	int size_new_string = strlen(ruta_directorio_pokemon) + strlen(METADATA_FILE) + 1;
	char *ruta_metadata_pokemon = malloc(size_new_string);
	strcpy(ruta_metadata_pokemon, ruta_directorio_pokemon);
	string_append(&ruta_metadata_pokemon, METADATA_FILE);

	t_config *metadata_pokemon = config_create(ruta_metadata_pokemon);

	char *blocks = config_get_string_value(metadata_pokemon, "BLOCKS");
	int sizeBlocks = strlen(blocks);

	if(sizeBlocks == 2) { // si esta vacio el array de blocks
		sizeBlocks += size_nro_bloque;
	} else {
		sizeBlocks += size_nro_bloque + 1; // mas uno por la coma
	}

	log_info(logger, "AGREGANDO BLOQUE <%d> A LA ESTRUCTURA BLOCKS", nro_bloque);
	char *newBlocks = malloc(sizeBlocks + 1);
	int posicion = 0;

	if(strlen(blocks) < 3) {
		strcpy(newBlocks + posicion, "[");
		posicion ++;
		strcpy(newBlocks + posicion, string_itoa(nro_bloque));
		posicion += size_nro_bloque;
		strcpy(newBlocks + posicion, "]");
	} else {
		char *auxiliar = strdup(string_substring(blocks, 1, strlen(blocks) - 2)); // se resta 2 por los corchetes []
		strcpy(newBlocks + posicion, "[");
		posicion ++;
		strcpy(newBlocks + posicion, auxiliar);
		posicion += strlen(auxiliar);
		strcpy(newBlocks + posicion, ",");
		posicion ++;
		strcpy(newBlocks + posicion, string_itoa(nro_bloque));
		posicion += size_nro_bloque;
		strcpy(newBlocks + posicion, "]");
		free(auxiliar);
	}

	config_set_value(metadata_pokemon, "BLOCKS", newBlocks);
	config_save(metadata_pokemon);
	config_destroy(metadata_pokemon);
	free(newBlocks);
	free(ruta_metadata_pokemon);
}

// No hay memleak (funciona bien)
void crear_metadata_pokemon(char *ruta_directorio_pokemon, char *pokemon) {

    //creamos un nuevo string que va a ser el path del metadata.txt
	char *ruta_metadata_pokemon = string_new();
	string_append(&ruta_metadata_pokemon, ruta_directorio_pokemon);
    string_append(&ruta_metadata_pokemon, METADATA_FILE);

    log_info(logger, "CREANDO ARCHIVO CON RUTA <%s>", ruta_metadata_pokemon);
    FILE *metadataPointerFile = fopen(ruta_metadata_pokemon, "w+");
    if(metadataPointerFile == NULL) {
    	log_error(logger, "ERROR AL CREAR EL ARCHIVO CON RUTA <%s>", ruta_metadata_pokemon);
    	exit(1);
    }

    char *campos_metadataTxt = string_new();
    string_append(&campos_metadataTxt, "DIRECTORY=N");
    string_append(&campos_metadataTxt, "\n");
    string_append(&campos_metadataTxt, "SIZE=0");
    string_append(&campos_metadataTxt, "\n");
    string_append(&campos_metadataTxt, "BLOCKS=[]");
    string_append(&campos_metadataTxt, "\n");
    string_append(&campos_metadataTxt, "OPEN=Y");
    string_append(&campos_metadataTxt, "\n");
	
    if(fwrite(campos_metadataTxt, string_length(campos_metadataTxt), 1, metadataPointerFile) == 0) {
    	log_error(logger, "ERROR AL ESCRIBIR EL ARCHIVO CON RUTA <%s>", ruta_metadata_pokemon);
    	exit(1);
    }
	
    free(campos_metadataTxt);
    fclose(metadataPointerFile);
    free(ruta_metadata_pokemon);
}


void modificar_linea_en_archivo(char* file_memory, new_pokemon *newPokemon, char *ruta_directorio_pokemon, char *coordenada) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, ruta_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", newPokemon->name, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int posicionLinea = get_posicion_linea_en_archivo(coordenada, file_memory);

	int tamanioLineaVieja = get_tamanio_linea(file_memory + posicionLinea);
	log_info(logger, "tamanio vieja linea: %d", tamanioLineaVieja);

	char *viejaLinea = string_duplicate(string_substring(file_memory, posicionLinea, tamanioLineaVieja));
	log_info(logger, "vieja linea: %s", viejaLinea);

	char *lineaActualizada = get_linea_nueva_cantidad(viejaLinea, coordenada, newPokemon->cantidad); // devuelve la linea actualizada pero con el \n
	log_info(logger, "linea actualizada: %s", lineaActualizada);

	int diferencia = strlen(lineaActualizada) - tamanioLineaVieja;
	log_info(logger, "diferencia vieja y nueva: %d", diferencia);

	if(diferencia == 0) {

		log_info(logger, "NO ES NECESARIO MOVER LAS LINEAS DE DATOS");
		memcpy(file_memory + posicionLinea, lineaActualizada, strlen(lineaActualizada));
		actualizar_contenido_blocks(ruta_directorio_pokemon, file_memory);
		munmap(file_memory, tamArchivo);
		cambiar_valor_metadata(ruta_directorio_pokemon, "OPEN", "N");

	}

	if(hay_espacio_ultimo_bloque(ruta_directorio_pokemon, lineaActualizada)) {

		// se escribe sin nuevo bloque
		modificar_linea_pokemon(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, newPokemon->name);

	} else {
		// buscamos un bloque libre y si lo hay, se lo asignamos a la metadata
		int nuevo_nro_bloque = obtener_bloque_libre();
		// luego hacemos el resto
		if(nuevo_nro_bloque == -1) {
			log_error(logger, "NO HAY SUFICIENTE ESPACIO PARA ALMACENAR NUEVOS DATOS");
			pthread_cancel(thread_new_pokemon);
		}

		agregar_bloque_metadata_pokemon(ruta_directorio_pokemon, nuevo_nro_bloque);

		modificar_linea_pokemon(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, newPokemon->name);

	}

	free(lineaActualizada);
	free(viejaLinea);
	free(ruta_archivo_pokemon);
}


int get_posicion_linea_en_archivo(char *linea, char *mmaped) { // devuelve la posicion donde esta situada la linea dentro del mmap (se contabiliza el \n)

	int i = 0;
	int sizeLinea = 0;

	char **lineas = string_split(mmaped, "\n");

	while(!string_contains(lineas[i], linea)) {
		sizeLinea += strlen(lineas[i]) + 1; // el + 1 por el \n
		i++;
	}

	log_info(logger, "Posicion Linea: %d", sizeLinea);

	return sizeLinea;
}


int get_tamanio_linea(char *mapped) {

	int i = 0;

	while(mapped[i] != '\n') {
		i++;
	}
	i++;

	return i++;
}


char* get_linea_nueva_cantidad(char *linea, char *coordenada, int cantidad_a_agregar) { // nos devuelve un char* que tiene la cantidad modificada

	int sizeCoordenada = strlen(coordenada) + 1; // 1 por el caracter '='

	int sizeLinea = strlen(linea);

	char *viejaCantidad = string_duplicate(string_substring(linea, sizeCoordenada, sizeLinea - sizeCoordenada));

	int oldCantidad = atoi(viejaCantidad);

	int nuevaCantidad = oldCantidad + cantidad_a_agregar;

	char *newCantidad = string_itoa(nuevaCantidad);

	char *lineaModificada = string_new();
	string_append_with_format(&lineaModificada, "%s%s%s%s", coordenada, IGUAL, newCantidad, "\n");
	printf("size nueva linea: %d\n", strlen(lineaModificada));

	return lineaModificada;
}


void actualizar_contenido_blocks(char *path_directorio_pokemon, char *mapped) {

	int desplazamiento = 0;

	char **array_blocks_metadata = get_array_blocks_metadata(path_directorio_pokemon);

	int cantidadBloques = cantidad_de_bloques(array_blocks_metadata);

	for(int i = 0 ; i < cantidadBloques ; i++) {
		actualizar_bloque(mapped, desplazamiento, array_blocks_metadata[i]);
		desplazamiento += datos_config->size_block;
	}

	// hacer free del array_blocks_metadata
}


char** get_array_blocks_metadata(char *path_directorio_pokemon) {

	char *path_metadata_pokemon = string_new();
	string_append_with_format(&path_metadata_pokemon, "%s%s%s", path_directorio_pokemon,"/", METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_pokemon);

	return config_get_array_value(metadata_pokemon, "BLOCKS");
}


int cantidad_de_bloques(char **arrayBlocks) {

	int i = 0;

	while(arrayBlocks[i] != NULL) {
		i++;
	}

	return i;
}


void actualizar_bloque(char *mapped, int desplazamiento, char *bloque) {

	log_info(logger, "ACTUALIZANDO BLOQUES");

	char * path_archivo_bloque = string_new();
	string_append_with_format(&path_archivo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", bloque, ".txt");
	log_info(logger, "RUTA ARCHIVO BLOQUE <%s>", path_archivo_bloque);

	char *contenido = string_duplicate(string_substring(mapped, desplazamiento, datos_config->size_block));

	escribir_archivo(path_archivo_bloque, contenido, "w+");

	free(path_archivo_bloque);
	free(contenido);

}

// modifica el archivo pokemon con la nueva cantidad (sin importar si se agrando o no el tamaño)

void modificar_linea_pokemon(char *fileMemory, char *viejaLinea, char *lineaActualizada, int posicionLinea, char *path_directorio_pokemon, char *pokemon) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", pokemon, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int sizeViejaLinea = strlen(viejaLinea);

	int sizeNuevaLinea = strlen(lineaActualizada);

	int diferencia = sizeNuevaLinea - sizeViejaLinea;


	log_info(logger, "ES NECESARIO MOVER LAS LINEAS DE DATOS");
	int desplazamiento = 0;
	// mover los bloques para adelante
	char *buffer = malloc(tamArchivo + diferencia + 1); // + 1 por el \0

	if(posicionLinea == 0) { // esta en la primera linea

		memcpy(buffer, lineaActualizada, sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		memcpy(buffer + desplazamiento, fileMemory + sizeViejaLinea, tamArchivo - sizeViejaLinea);
		desplazamiento += tamArchivo - sizeViejaLinea;

		buffer[desplazamiento] = '\0';

		log_info(logger, "contenido actualizado:");
		log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

		int oldValorSize = valor_campo_size_metadata(path_directorio_pokemon);
		int newValorSize = oldValorSize + diferencia;
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(newValorSize));

		actualizar_contenido_blocks(path_directorio_pokemon, buffer);

		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		free(buffer);

	} else if(posicionLinea + sizeViejaLinea == tamArchivo) { // esta en la ultima linea

		memcpy(buffer, fileMemory, posicionLinea);
		desplazamiento += posicionLinea;

		memcpy(buffer + desplazamiento, lineaActualizada, sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		buffer[desplazamiento] = '\0';

		log_info(logger, "contenido actualizado:");
		log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

		int oldValorSize = valor_campo_size_metadata(path_directorio_pokemon);
		int newValorSize = oldValorSize + diferencia;
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(newValorSize));
		// actualizamos el valod de los bloques
		actualizar_contenido_blocks(path_directorio_pokemon, buffer);
		// cambiamos el valor de OPEN a N
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		free(buffer);

	} else { // esta en el medio

		memcpy(buffer, fileMemory, posicionLinea);
		desplazamiento += posicionLinea;

		memcpy(buffer + desplazamiento, lineaActualizada, sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		memcpy(buffer + desplazamiento, fileMemory + posicionLinea + sizeViejaLinea, tamArchivo - posicionLinea - sizeViejaLinea);
		desplazamiento += tamArchivo - posicionLinea - sizeViejaLinea;

		buffer[desplazamiento] = '\0';

		log_info(logger, "contenido actualizado:");
		log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

		int oldValorSize = valor_campo_size_metadata(path_directorio_pokemon);
		int newValorSize = oldValorSize + diferencia;
		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(newValorSize));
		// actualizamos el valod de los bloques
		actualizar_contenido_blocks(path_directorio_pokemon, buffer);
		// cambiamos el valor de OPEN a N
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");
	}

	munmap(fileMemory, tamArchivo);
	free(ruta_archivo_pokemon);
}


//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------
// catch_pokemon

void operacion_catch_pokemon(catch_pokemon *catchPokemon) {

	char *path_directorio_pokemon = string_new();
	string_append_with_format(&path_directorio_pokemon, "%s%s%s%s",datos_config->pto_de_montaje, FILES_DIR, "/", catchPokemon->name);
	log_info(logger, "EN BUSCA DEL DIRECTORIO DEL POKEMON CON PATH <%s>", path_directorio_pokemon);

	if(opendir(path_directorio_pokemon) == NULL) { // si esxiste o no el directorio (FAIL)
		log_error(logger, "EL DIRECTORIO <%s> NO EXISTE", path_directorio_pokemon);
	    //enviar_respuesta_a_broker
	} else {
		char *valor = get_valor_campo_metadata(path_directorio_pokemon, "DIRECTORY");
		if(string_equals_ignore_case( valor, "Y")) { // si es un directorio, se envia al broker la respuesta (FAIL)
	    	log_info(logger, "LA RUTA <%s> ES SOLO UN DIRECTORIO ", path_directorio_pokemon);
	    	// enviar_respuesta_a_broker (FAIL)
	    } else {
	    	log_info(logger, "EL DIRECTORIO <%s> EXISTE JUNTO CON EL ARCHIVO POKEMON");
	    	// el archivo existe y hay que verificar si existe la linea
	    	buscar_linea_en_el_archivo_catch(catchPokemon, path_directorio_pokemon);
	    }
	}

	free(path_directorio_pokemon);
}

void buscar_linea_en_el_archivo_catch(catch_pokemon *catchPokemon, char *path_directorio_pokemon) { // verificamos si existe o no la linea en el archivo existente

	char *path_archivo_pokemon = string_new();
	string_append(&path_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&path_archivo_pokemon, "%s%s%s", "/", catchPokemon->name, POKEMON_FILE_EXT);

	int tamanioArchivo = fileSize(path_archivo_pokemon);

	log_info(logger, "ABRIENDO ARCHIVO CON RUTA <%s>", path_archivo_pokemon);

	int fdPokemon = open(path_archivo_pokemon, O_RDWR);

	char *file_memory = mmap(0, tamanioArchivo, PROT_READ | PROT_WRITE, MAP_SHARED, fdPokemon, 0);

	close(fdPokemon);

	char *linea = string_new();
	string_append_with_format(&linea, "%s%s%s", string_itoa(catchPokemon->pos.posx), GUION, string_itoa(catchPokemon->pos.posy));

	if(string_contains(file_memory, linea)) { // buscar en el archivo y modificar

		log_info(logger, "COORDENADA <%s> ENCONTRADO", linea);
		log_info(logger, "LINEA <%s>", linea); // es la coordenada
		modificar_linea_pokemon_catch(file_memory, catchPokemon, path_directorio_pokemon, linea);

	} else { // se envia resultado al broker (FAIL)

		munmap(file_memory, tamanioArchivo);
		log_info(logger, "COORDENADA <%s> NO ENCONTRADO!", linea);

	}

	free(path_archivo_pokemon);
	free(linea);
}

void modificar_linea_pokemon_catch(char* file_memory, catch_pokemon *catchPokemon, char *ruta_directorio_pokemon, char *coordenada) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, ruta_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", catchPokemon->name, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int posicionLinea = get_posicion_linea_en_archivo(coordenada, file_memory);

	int tamanioLineaVieja = get_tamanio_linea(file_memory + posicionLinea);
	log_info(logger, "tamanio vieja linea: %d", tamanioLineaVieja);

	char *viejaLinea = string_duplicate(string_substring(file_memory, posicionLinea, tamanioLineaVieja));
	log_info(logger, "vieja linea: %s", viejaLinea);

	char *lineaActualizada = get_linea_nueva_cantidad_catch(viejaLinea, coordenada); // devuelve la linea actualizada pero con el \n
	log_info(logger, "linea actualizada: %s", lineaActualizada);

	int diferencia = tamanioLineaVieja - strlen(lineaActualizada);
	log_info(logger, "diferencia vieja y nueva: %d", diferencia);

	int sizeCoordenada = strlen(coordenada) + 1; // + 1 por el =

	int cantidadLineaActualizada = atoi(string_duplicate(string_substring(lineaActualizada, sizeCoordenada, strlen(lineaActualizada) - sizeCoordenada - 1)));
	log_info(logger, "cantidad actualizada:%d", cantidadLineaActualizada);

	// diferencia == 0
	if(cantidadLineaActualizada != 0) { // no se tiene que borrar la linea entera, solo uno o mas bytes de la misma

		if(diferencia == 0) { // no cambia nada, es solo reemplazar la misma linea con la nueva cantidad

			log_info(logger, "NO ES NECESARIO MOVER LAS LINEAS DE DATOS");
			memcpy(file_memory + posicionLinea, lineaActualizada, strlen(lineaActualizada));
			actualizar_contenido_blocks(ruta_directorio_pokemon, file_memory);
			munmap(file_memory, tamArchivo);
			cambiar_valor_metadata(ruta_directorio_pokemon, "OPEN", "N");

		} else {

			if(ultimo_bloque_queda_en_cero(diferencia, ruta_directorio_pokemon)) {
				// borro el ultimo bloque de la lista y tambien el archivo en blocks
				log_info(logger, "ES NECESARIO MOVER LOS BYTES PARA ATRAS Y BORRAR EL ULTIMO BLOQUE");

				int ultimo_bloque = ultimo_bloque_array_blocks(ruta_directorio_pokemon);
				borrar_ultimo_bloque_metadata_blocks(ruta_directorio_pokemon, ultimo_bloque);

				char *ultimoBloque = string_itoa(ultimo_bloque);
				borrar_archivo(ultimoBloque, 'B');

				//seteo a 0 el bitmap[ultimoBloque]

				modificar_archivo_pokemon_catch_con_linea(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, catchPokemon->name);

			} else {
				// solo modifico el ultimo bloque
				log_info(logger, "SOLO MODIFICAMOS EL ULTIMO BLOQUE");
				modificar_archivo_pokemon_catch_con_linea(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, catchPokemon->name);
			}
		}

	} else { // se borra la linea entera

		if(tamArchivo == strlen(lineaActualizada)) { // se borra todo: el archvio, los bloques y en la metadata queda solamente el DIRECTORY

			cambiar_archivo_a_directorio(file_memory, ruta_archivo_pokemon, ruta_directorio_pokemon);

		} else {

			if(ultimo_bloque_queda_en_cero(strlen(lineaActualizada), ruta_directorio_pokemon)) {
				// borro el ultimo bloque de la lista y tambien el archivo en blocks
				log_info(logger, "SE BORRA LA LINEA ENTERA Y EL ULTIMO BLOQUE");

				int ultimo_bloque = ultimo_bloque_array_blocks(ruta_directorio_pokemon);
				borrar_ultimo_bloque_metadata_blocks(ruta_directorio_pokemon, ultimo_bloque);

				char *ultimoBloque = string_itoa(ultimo_bloque);
				borrar_archivo(ultimoBloque, 'B');

				//seteo a 0 el bitmap[ultimoBloque]

				modificar_archivo_pokemon_catch_sin_linea(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, catchPokemon->name);

			} else {
				// solo modifico el ultimo bloque
				log_info(logger, "SOLO MODIFICAMOS EL ULTIMO BLOQUE");
				modificar_archivo_pokemon_catch_sin_linea(file_memory, viejaLinea, lineaActualizada, posicionLinea, ruta_directorio_pokemon, catchPokemon->name);
			}

		}

	}

	free(lineaActualizada);
	free(viejaLinea);
	free(ruta_archivo_pokemon);
}

// esta es para cuando se borra la linea entera
void modificar_archivo_pokemon_catch_sin_linea(char *fileMemory, char *viejaLinea, char *lineaActualizada, int posicionLinea, char *path_directorio_pokemon, char *pokemon) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", pokemon, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int sizeViejaLinea = strlen(viejaLinea);

	int sizeNuevaLinea = strlen(lineaActualizada);

	int nuevoTamArchivo = tamArchivo - sizeNuevaLinea;

	int desplazamiento = 0;
	// mover los bloques para atras
	char *buffer = malloc(nuevoTamArchivo + 1); // + 1 por el \0

	if(posicionLinea == 0) { // esta en la primera linea

		memcpy(buffer, fileMemory + sizeViejaLinea, tamArchivo - sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		buffer[desplazamiento] = '\0';

		munmap(fileMemory, tamArchivo);

		log_info(logger, "contenido actualizado:");
		log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(nuevoTamArchivo));

		// actualizamos el valor de los bloques
		actualizar_contenido_blocks(path_directorio_pokemon, buffer);
		// cambiamos el valor de OPEN a N
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		free(buffer);

	} else if(posicionLinea + sizeViejaLinea == tamArchivo) { // esta en la ultima linea

			memcpy(buffer, fileMemory, posicionLinea);
			desplazamiento += posicionLinea;

			buffer[desplazamiento] = '\0';

			munmap(fileMemory, tamArchivo);

			log_info(logger, "contenido actualizado:");
			log_info(logger, "%s", buffer);

			escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

			cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(nuevoTamArchivo));
			// actualizamos el valod de los bloques
			actualizar_contenido_blocks(path_directorio_pokemon, buffer);
			// cambiamos el valor de OPEN a N
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

			free(buffer);

		} else { // esta en el medio

			memcpy(buffer, fileMemory, posicionLinea);
			desplazamiento += posicionLinea;

			memcpy(buffer + desplazamiento, fileMemory + posicionLinea + sizeViejaLinea, tamArchivo - posicionLinea - sizeViejaLinea);
			desplazamiento += tamArchivo - posicionLinea - sizeViejaLinea;

			buffer[desplazamiento] = '\0';

			munmap(fileMemory, tamArchivo);

			log_info(logger, "contenido actualizado:");
			log_info(logger, "%s", buffer);

			escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

			cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(nuevoTamArchivo));
			// actualizamos el valod de los bloques
			actualizar_contenido_blocks(path_directorio_pokemon, buffer);
			// cambiamos el valor de OPEN a N
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		}

	free(ruta_archivo_pokemon);
}


// esta funcion es para cuando la linea se reduce 1 byte o mas. Se mantiene la coordenada
void modificar_archivo_pokemon_catch_con_linea(char *fileMemory, char *viejaLinea, char *lineaActualizada, int posicionLinea, char *path_directorio_pokemon, char *pokemon) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", pokemon, ".txt");

	int tamArchivo = fileSize(ruta_archivo_pokemon);

	int sizeViejaLinea = strlen(viejaLinea);

	int sizeNuevaLinea = strlen(lineaActualizada);

	int nuevoTamArchivo = tamArchivo - (sizeViejaLinea - sizeNuevaLinea);

	int desplazamiento = 0;

	// mover los bloques para atras
	char *buffer = malloc(nuevoTamArchivo + 1); // + 1 por el \0

	if(posicionLinea == 0) { // esta en la primera linea

		memcpy(buffer, lineaActualizada, sizeNuevaLinea);
		desplazamiento += sizeNuevaLinea;

		memcpy(buffer + desplazamiento, fileMemory + sizeViejaLinea, tamArchivo - sizeViejaLinea);
		desplazamiento += tamArchivo - sizeViejaLinea;

		buffer[desplazamiento] = '\0';

		munmap(fileMemory, tamArchivo);

		log_info(logger, "contenido actualizado:");
		log_info(logger, "%s", buffer);

		escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

		cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(nuevoTamArchivo));

		// actualizamos el valor de los bloques
		actualizar_contenido_blocks(path_directorio_pokemon, buffer);
		// cambiamos el valor de OPEN a N
		cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		free(buffer);

	} else if(posicionLinea + sizeViejaLinea == tamArchivo) { // esta en la ultima linea

			memcpy(buffer, fileMemory, posicionLinea);
			desplazamiento += posicionLinea;

			memcpy(buffer + desplazamiento, lineaActualizada, sizeNuevaLinea);
			desplazamiento += sizeNuevaLinea;

			buffer[desplazamiento] = '\0';

			munmap(fileMemory, tamArchivo);

			log_info(logger, "contenido actualizado:");
			log_info(logger, "%s", buffer);

			escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

			cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(nuevoTamArchivo));
			// actualizamos el valod de los bloques
			actualizar_contenido_blocks(path_directorio_pokemon, buffer);
			// cambiamos el valor de OPEN a N
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

			free(buffer);

		} else { // esta en el medio

			memcpy(buffer, fileMemory, posicionLinea);
			desplazamiento += posicionLinea;

			memcpy(buffer + desplazamiento, lineaActualizada, sizeNuevaLinea);
			desplazamiento += sizeNuevaLinea;

			buffer[desplazamiento] = '\0';

			munmap(fileMemory, tamArchivo);

			log_info(logger, "contenido actualizado:");
			log_info(logger, "%s", buffer);

			escribir_archivo(ruta_archivo_pokemon, buffer, "w+");

			cambiar_valor_metadata(path_directorio_pokemon, "SIZE", string_itoa(nuevoTamArchivo));
			// actualizamos el valod de los bloques
			actualizar_contenido_blocks(path_directorio_pokemon, buffer);
			// cambiamos el valor de OPEN a N
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

		}

	free(ruta_archivo_pokemon);
}


char* get_linea_nueva_cantidad_catch(char *linea, char *coordenada) { // nos devuelve un char* que tiene la cantidad modificada pero disminuido en 1 (x - 1)

	int sizeCoordenada = strlen(coordenada) + 1; // 1 por el caracter '='

	int sizeLinea = strlen(linea);

	char *viejaCantidad = string_duplicate(string_substring(linea, sizeCoordenada, sizeLinea - sizeCoordenada));

	int oldCantidad = atoi(viejaCantidad);

	int nuevaCantidad = oldCantidad - 1;

	char *newCantidad = string_itoa(nuevaCantidad);

	char *lineaModificada = string_new();
	string_append_with_format(&lineaModificada, "%s%s%s%s", coordenada, IGUAL, newCantidad, "\n");
	printf("size nueva linea: %d\n", strlen(lineaModificada));

	return lineaModificada;
}


bool ultimo_bloque_queda_en_cero(int bytes_a_mover, char *path_directorio_pokemon) { // nos va a determinar si el ultimo bloque queda vacio o no

	char **array_blocks = get_array_blocks_metadata(path_directorio_pokemon);

	char *ultimoBloque = string_itoa(ultimo_bloque_array_blocks(path_directorio_pokemon));

	char *ruta_archivo_bloque = string_new();
	string_append_with_format(&ruta_archivo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", ultimoBloque, ".txt");

	int fileSizeLastBlock = fileSize(ruta_archivo_bloque);

	if(bytes_a_mover >= fileSizeLastBlock) {
		// puedo asumir que el ultimo bloque queda vacio
		return true;

	} else {
		// asumo que va a seguir habiendo datos en el ultimo bloque
		return false;
	}
}


void cambiar_archivo_a_directorio(char *file_memory, char *path_archivo_pokemon, char *path_directorio_pokemon) {

	// revisar por que diantres no me borra el bloque que resta

	int tamArchivo = fileSize(path_archivo_pokemon);

	munmap(file_memory, tamArchivo);
	remove(path_archivo_pokemon);
	int ultimo_bloque = ultimo_bloque_array_blocks(path_directorio_pokemon);
	// se actualiza el bitmap, se limpia el bit de la posicion ultimo_bloque
	char *path_ultimo_bloque = string_new();
	string_append_with_format(&path_ultimo_bloque, "%s%s%s%s%s", datos_config->pto_de_montaje, BLOCKS_DIR, "/", string_itoa(ultimo_bloque), ".txt");
	remove(path_ultimo_bloque);

	cambiar_metadata_archivo_a_directorio(path_directorio_pokemon);
	cambiar_valor_metadata(path_directorio_pokemon, "DIRECTORY", "Y");

}


void borrar_archivo(char *nombre, char flag) {

	char *ruta_archivo = string_new();
	string_append(&ruta_archivo, datos_config->pto_de_montaje);

	if(flag == 'P') { // quiero borrar un archivo pokemon
		string_append_with_format(&ruta_archivo, "%s%s%s", POKEMON_DIR, nombre, ".txt");
		remove(ruta_archivo);
	} else {
		string_append_with_format(&ruta_archivo, "%s%s%s%s", BLOCKS_DIR, "/", nombre, ".txt");
		remove(ruta_archivo);
	}

	free(ruta_archivo);
}


void cambiar_metadata_archivo_a_directorio(char *path_directorio_pokemon) {

	char *ruta_metadata_pokemon = string_new();
	string_append_with_format(&ruta_metadata_pokemon, "%s%s", path_directorio_pokemon, METADATA_FILE);

	t_config *metadataArchivo = config_create(ruta_metadata_pokemon);

	config_remove_key(metadataArchivo, "OPEN");

	config_remove_key(metadataArchivo, "BLOCKS");

	config_remove_key(metadataArchivo, "SIZE");

	config_save_in_file(metadataArchivo, ruta_metadata_pokemon);

	config_destroy(metadataArchivo);
}


void borrar_ultimo_bloque_metadata_blocks(char *ruta_directorio_pokemon, int nro_bloque) { // borra el ultimo bloque en el campo blocks de la metadata

	int size_nro_bloque = strlen(string_itoa(nro_bloque));

	int size_new_string = strlen(ruta_directorio_pokemon) + strlen(METADATA_FILE) + 1;
	char *ruta_metadata_pokemon = malloc(size_new_string);
	strcpy(ruta_metadata_pokemon, ruta_directorio_pokemon);
	string_append(&ruta_metadata_pokemon, METADATA_FILE);

	t_config *metadata_pokemon = config_create(ruta_metadata_pokemon);

	char *blocks = config_get_string_value(metadata_pokemon, "BLOCKS");
	int sizeBlocks = strlen(blocks);

	int newSizeBlocks = sizeBlocks - size_nro_bloque - 1; // -1 por la coma

	char *newBlocks = malloc(newSizeBlocks + 1);

	char *auxiliar = strdup(string_substring(blocks, 1, newSizeBlocks - 2)); // se resta 2 por los corchetes []
	log_info(logger, "auxiliar: %s", auxiliar);

	int posicion = 0;

	strcpy(newBlocks, "[");
	posicion++;

	strcpy(newBlocks + posicion, auxiliar);
	posicion += strlen(auxiliar);

	strcpy(newBlocks + posicion, "]");
	posicion++;

	config_set_value(metadata_pokemon, "BLOCKS", newBlocks);
	config_save(metadata_pokemon);
	config_destroy(metadata_pokemon);
	free(newBlocks);
	free(ruta_metadata_pokemon);
}


//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------

int fileSize(char* file) { 

	FILE* fp = fopen(file, "r");
    if (fp == NULL) {
    	log_error(logger, "No existe el archivo");
        exit(1);
    } 

    fseek(fp, 0L, SEEK_END);
    int res = (int) ftell(fp); 
    fclose(fp); 
    return res; 
} 

char *read_file_into_buf (char * source,  FILE *fp){
   if (fseek(fp, 0L, SEEK_END) == 0) {
        long bufsize = ftell(fp);
        if (bufsize == -1) { 
			log_error(logger, "EL archivo buscado NO EXISTE");
		 }

        /*Le da al buffer el mismo tamaño del archivo */
        source = malloc(sizeof(bufsize + 1));
		//log_info(logger, "Malloc tam archivo %d",bufsize);
		fseek (fp, 0, SEEK_SET); //vuelve al principio
        if (fseek(fp, 0L, SEEK_SET) != 0) { /* Error */
			log_error(logger, "Error al volver al inicio del archivo");
			 }

        /* Read the entire file into memory. */
		
		fgets(source,bufsize,fp);
		log_info(logger,"Lo que se escaneo :: %s",source);
		return source;
        if ( ferror( fp ) != 0 ) {
            fputs("Error reading file", stderr);

        } 
    }
}

/*
	get_pokemon* pepa = malloc(sizeof(get_pokemon));
	pepa->name_size = 6;
	pepa->name= "pepa";
	operacion_get_Pokemon(pepa);*/

void operacion_get_Pokemon(get_pokemon *GETpokemon){
	rtaGet* respuesta = malloc(sizeof(rtaGet));
    respuesta->id_mensaje = 000; //Preguntar a Patricio como pasan el id @TODO
    respuesta->name = GETpokemon->name;
	char *path_directorio_pokemon = string_new();
		string_append_with_format(&path_directorio_pokemon, "%s%s%s%s",datos_config->pto_de_montaje, FILES_DIR, "/", GETpokemon->name);
		log_info(logger, "EN BUSCA DEL DIRECTORIO DEL POKEMON CON PATH <%s>", path_directorio_pokemon);

		if(opendir(path_directorio_pokemon) == NULL) { // si existe o no el directorio (FAIL)
			log_error(logger, "EL DIRECTORIO <%s> NO EXISTE", path_directorio_pokemon);
		    //si no existe se devuelve el nombre y el id, los cuales ya estan cargados
			//enviar_respuesta_a_broker(respuesta)
		} else {
			char *valor = get_valor_campo_metadata(path_directorio_pokemon, "DIRECTORY");
			if(string_equals_ignore_case( valor, "Y")) { // si es un directorio, se envia al broker la respuesta (FAIL)
		    	log_info(logger, "LA RUTA <%s> ES SOLO UN DIRECTORIO ", path_directorio_pokemon);
		    } else {
		    	log_info(logger, "EL DIRECTORIO <%s> EXISTE JUNTO CON EL ARCHIVO POKEMON");
			
		    	//mutex_lock
    		char *valor_open = get_valor_campo_metadata(path_directorio_pokemon, "DIRECTORY");
    		if(string_equals_ignore_case(valor_open, "N")) {
    			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "Y");
    			// mutex_unlock
		
            
            char* pathFILE = string_new();
            string_append(&pathFILE,path_directorio_pokemon);
            string_append_with_format(&pathFILE,"%s%s%s", "/",GETpokemon->name, POKEMON_FILE_EXT);
			//log_info(logger, "Path pokemon %s", pathFILE);
			FILE *fp =fopen(pathFILE, "r");
			int fplen = fileSize(pathFILE);
            char* scaneo = NULL;
			//revisar como entran las cosas al buffer
            read_file_into_buf (scaneo, fp);
			//mutex_lock
			cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");
			log_info(logger, "Cerro archivo pokemon");
    		// mutex_unlock
			free(pathFILE);
			free(fp);
            //falta separar uno a uno las lineas
			respuesta->posiYcant = list_create();
			log_info(logger, "Hizo los frees  y Crea lista");
       
			int desplazamiento = 0;
			while(desplazamiento < fplen){
				//REPENSAR ESTO COMO ME DIJO PATRICIO
			posYcant* posicionesCant = malloc(sizeof(posYcant));
			log_info(logger, "zona de memcpy");
	//TENGO PROBLEMAS CON LOS MEMCPY REVISAR
			memcpy(&(posicionesCant->posX), scaneo, sizeof(int));
			log_info(logger, "Entrando a memcpy");
			log_info(logger, "%d Posicione en x", posicionesCant->posX);
			desplazamiento = desplazamiento + sizeof(int) + sizeof(char);//el char del -
			memcpy(&(posicionesCant->posY), scaneo, sizeof(int));
			desplazamiento = desplazamiento + sizeof(int) + sizeof(char);//el char del =
			memcpy(&(posicionesCant->cant), scaneo, sizeof(int));
			list_add(respuesta->posiYcant, posicionesCant);
			desplazamiento = desplazamiento + sizeof(int) + 1; //por el /n
			free(posicionesCant);
			}
            
            free(scaneo);
			//enviar_respuesta_a_broker(respuesta)
		    } else{// en caso de que este abierto
				  //finalizar hilo y reintentar despues del tiempo que dice el config
/*
				log_warning(logger, "HILO EN STANDBY DE OPERACION");
    			pthread_cancel(thread_get_pokemon);
				sleep(datos_config->tiempo_retardo_operacion);
	//	pthread_create(&atender_get_pokemon, NULL, (void *) operacion_get_pokemon, (void *) GETpokemon);
    		//	pthread_join(atender_get_pokemon, NULL);
    			log_info(logger, "HILO RETOMANDO LA OPERACION");
				*/
			}
				
		}

		    free(path_directorio_pokemon);
			
    }

    
}


//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------------------------
// Funciones de sockets (segui bajando si es que necesitas ver algo)


void suscripcion_colas_broker() {

	log_info(logger, "ESTABLECIENDO CONEXION CON EL BROKER");
	suscribirse_a_new_pokemon();
	suscribirse_a_catch_pokemon();
	suscribirse_a_get_pokemon();

	if(socket_cliente_np == -1 && socket_cliente_cp == -1 && socket_cliente_gp == -1) {
			log_error(logger, "BROKER NO ESTA DISPONIBLE PARA LA CONEXION");
			pthread_create(&servidor_gamecard, NULL, (void *)iniciar_servidor, NULL);
			pthread_join(servidor_gamecard, NULL);
	} else {
		log_info(logger, "GAMECARD CONECTADO AL BROKER");

		//pthread_create(&thread_new_pokemon, NULL, (void *)recibir_mensajes_new_pokemon, NULL);
		//pthread_detach(thread_new_pokemon);

		//pthread_create(&thread_catch_pokemon, NULL, (void *)recibir_mensajes_catch_pokemon, NULL);
		//pthread_detach(thread_catch_pokemon);

		//pthread_create(&thread_get_pokemon, NULL, (void *)recibir_mensajes_get_pokemon, NULL);
		//pthread_detach(thread_get_pokemon);
	}
}

void suscribirse_a_new_pokemon() {

	socket_cliente_np = crear_conexion(datos_config->ip_broker, string_itoa(datos_config->puerto_broker));
	if(socket_cliente_np != -1) {
		//enviar_mensaje_suscripcion(NEW_POKEMON, socket_cliente_np, pid_gamecard);
		enviar_info_suscripcion(NEW_POKEMON, socket_cliente_np, pid_gamecard);
		recv(socket_cliente_np, &(acks_gamecard.ack_new), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA NEW_POKEMON: %d", acks_gamecard.ack_new);
	}
}

void suscribirse_a_catch_pokemon() {
	socket_cliente_cp = crear_conexion(datos_config->ip_broker, string_itoa(datos_config->puerto_broker));
	if(socket_cliente_cp != -1) {
		//enviar_mensaje_suscripcion(CATCH_POKEMON, socket_cliente_cp, pid_gamecard);
		enviar_info_suscripcion(CATCH_POKEMON, socket_cliente_cp, pid_gamecard);
		recv(socket_cliente_cp, &(acks_gamecard.ack_catch), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA CATCH_POKEMON: %d", acks_gamecard.ack_catch);
	}
}

void suscribirse_a_get_pokemon() {
	socket_cliente_gp = crear_conexion(datos_config->ip_broker, string_itoa(datos_config->puerto_broker));
	if(socket_cliente_gp != -1) {
		//enviar_mensaje_suscripcion(GET_POKEMON, socket_cliente_gp, pid_gamecard);
		enviar_info_suscripcion(GET_POKEMON, socket_cliente_gp, pid_gamecard);
		recv(socket_cliente_gp, &(acks_gamecard.ack_get), sizeof(int), MSG_WAITALL);
		log_info(logger, "ACK RECIVIDO PARA COLA GET_POKEMON: %d", acks_gamecard.ack_get);
	}
}

void iniciar_servidor(void) {

	int socket_servidor;

    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    getaddrinfo(IP_SERVIDOR, PUERTO_SERVIDOR, &hints, &servinfo);

    for (p=servinfo; p != NULL; p = p->ai_next)
    {
        if ((socket_servidor = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (bind(socket_servidor, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_servidor);
            continue;
        }
        break;
    }

	listen(socket_servidor, SOMAXCONN);
	log_info(logger, "GAMECARD INICIADO COMO SERVIDOR PARA GAMEBOY, ESPERANDO MENSAJES...");
    freeaddrinfo(servinfo);

    while(1)
    	esperar_cliente(socket_servidor);
}

void esperar_cliente(int socket_servidor) {
	struct sockaddr_in dir_cliente;
	int tam_direccion = sizeof(struct sockaddr_in);

	int socket_cliente = accept(socket_servidor, (void*) &dir_cliente, (socklen_t *)&tam_direccion);
	log_info(logger, "GAMEBOY CONECTADO!",socket_cliente);

	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) == -1) {
		cod_op = -1;
	}
	log_info(logger, "codigo de operacion: %d", cod_op);

	atender_peticion(socket_cliente, cod_op);
}

void atender_peticion(int socket_cliente, int cod_op) {

	int sizeStream;
	if(recv(socket_cliente, &sizeStream, sizeof(int), MSG_WAITALL) == -1) {
		sizeStream = -1;
	}
	printf("size_stream: %d\n", sizeStream);

	void *stream = malloc(sizeStream);
	if(recv(socket_cliente, stream, sizeStream, MSG_WAITALL) == -1) {
	}

	switch(cod_op) {
		case NEW_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION NEW_POKEMON");

			int id_mensaje_new;
			memcpy(&(id_mensaje_new), stream, sizeof(int));
			printf("ID MENSAJE: %d\n", id_mensaje_new);

			new_pokemon *newPokemon = deserializar_new(stream + sizeof(int));

			printf("POKEMON: %s\n", newPokemon->name);
			printf("PosX: %d\n", newPokemon->pos.posx);
			printf("PosY: %d\n", newPokemon->pos.posy);
			printf("Cantidad: %d\n", newPokemon->cantidad);

			pthread_t hilo_new_pokemon;
			pthread_create(&hilo_new_pokemon, NULL, (void *) operacion_new_pokemon, (void *) newPokemon);

			free(stream);
		break;

		case CATCH_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION CATCH_POKEMON");

			int id_mensaje_catch;
			memcpy(&(id_mensaje_catch), stream, sizeof(int));
			printf("ID MENSAJE: %d\n", id_mensaje_catch);

			catch_pokemon *catchPokemon = deserializar_catch(stream + sizeof(int));

			printf("POKEMON: %s\n", catchPokemon->name);
			printf("PosX: %d\n", catchPokemon->pos.posx);
			printf("PosY: %d\n", catchPokemon->pos.posy);

			pthread_t hilo_catch_pokemon;
			pthread_create(&hilo_catch_pokemon, NULL, (void *) operacion_catch_pokemon, (void *) catchPokemon);

			free(stream);
		break;

		case GET_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION GET_POKEMON");
			get_pokemon *getPokemon = malloc(sizeof(get_pokemon));
			int id_mensaje;
			memcpy(&(id_mensaje), stream, sizeof(int));
			printf("ID MENSAJE: %d\n", id_mensaje);
			//deserealizar_get_pokemon_gameboy(stream, getPokemon);
			getPokemon = deserializar_get(stream + sizeof(int));

			printf("POKEMON: %s\n", getPokemon->name);
			/*
			pthread_t hilo_get_pokemon;
			pthread_create(&hilo_get_pokemon, NULL, (void *) operacion_get_pokemon, (void *) getPokemon);
*/
			free(getPokemon);
			free(stream);
		break;

		default:
			log_warning(logger, "NO SE RECIBIO NINGUNA DE LAS ANTERIORES");
		break;
	}
}
