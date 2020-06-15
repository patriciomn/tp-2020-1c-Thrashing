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
	//datos_config->tiempo_retardo_operacion = config_get_int_value(config_tall_grass, "TIEMPO_RETARDO_OPERACION");

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
    log_info(logger, "TAMAÑO DEL BITMAP: %d", size_bitmap);

    fclose(bitmap_fp);

    int fd = open(path_bitmap_bin, O_RDWR , (mode_t)0600);
    if(fd != -1) {
    	bitmap_memoria = mmap(0, size_bitmap, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    	if(bitmap_memoria == MAP_FAILED) {
    		log_error(logger, "ERROR AL CARGAR BITMAP A MEMORIA");
    		close(fd);
    		exit(1);
    	}

    close(fd);

    bitarray = bitarray_create_with_mode(bitmap_memoria, size_bitmap, LSB_FIRST);

    }

    //void *buffer_bitmap = malloc(size_bitmap);

    //log_info(logger, "LEYENDO BITMAP EXISTENTE");
    //if(fread(buffer_bitmap, sizeof(char), size_bitmap, bitmap_fp) == 0) {
    	//log_error(logger, "NO SE HA PODIDO LEER EL CONTENIDO DEL ARCHIVO bitmap.bin");
    	//exit(1);
    //}

    //bitarray = bitarray_create_with_mode(buffer_bitmap, size_bitmap, LSB_FIRST);

    for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++) {
        printf("%d",bitarray_test_bit(bitarray,i));
    }
    printf("\n");

    //fclose(bitmap_fp);
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
    	log_info(logger, "CREANDO EL DIRECTORIO <%s>", path_directorio_pokemon);
    	mkdir(path_directorio_pokemon,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    	crear_pokemon(newPokemon, path_directorio_pokemon);
    }

    //if(stat(path_metadata_pokemon, &estado) == -1) { // si es igual a -1, entonces el archivo no existe
        //crear_archivos_pokemon(newPokemon->nombre, newPokemon->posicion, newPokemon->cantidad);
    //} else {
        // aca hay que ver dos situaciones. Si existe el archivo pero no tiene la linea o existe y tiene la linea
    //}

}

// esta funcion sirve solamente para cuando no existe el archivo pokemon
void crear_pokemon(new_pokemon *newPokemon, char *path_directorio_pokemon) {

	char *ruta_archivo_pokemon = string_new();
	string_append(&ruta_archivo_pokemon, path_directorio_pokemon);
	string_append_with_format(&ruta_archivo_pokemon, "%s%s%s", "/", newPokemon->name, POKEMON_FILE_EXT);

	log_info(logger, "CREANDO ARCHIVO CON RUTA <%s>", ruta_archivo_pokemon);
    FILE *pointerFile = fopen(ruta_archivo_pokemon,"w");
    if(pointerFile == NULL) {
    	log_error(logger, "ERROR AL CREAR ARCHIVO CON RUTA <%s>", ruta_archivo_pokemon);
    	exit(1);
    }

	char *linea = string_new();
	string_append_with_format(&linea, "%s%s%s%s%s", string_itoa(newPokemon->pos.posx), GUION, string_itoa(newPokemon->pos.posy), IGUAL, string_itoa(newPokemon->cantidad));
	string_append(&linea, "\n");
    log_info(logger, "LINEA A AGREGAR EN EL ARCHIVO <%s>", linea);

    // aca determino la cantidad de bloques en base a lo que vaya a escribir en el archivo
	int coordenadas_size = strlen(linea);
	int cant_blocks = (int) ceil((double)coordenadas_size / datos_config->size_block); // se divide el tamaño de la linea a agregar por el tamaño del bloque. Se redondea para arriba

    // lo siguiente es para testear si hay la cantidad de bloques que se necesita. Si hay, que haga la escritura, sino muere el hilo
    // semaforo
	if(hay_cantidad_de_bloques_libres(cant_blocks)) { // repensar esta parte, se puede revisar y devolver ya con el bitmap modificado asi otro hilo puede leerlo

		int tamanio_linea = strlen(linea);

		crear_metadata_pokemon(path_directorio_pokemon, newPokemon->name, tamanio_linea); // creamos la metadata del archivo pokemon y solo rellenamos el campo DIRECTORY y OPEN

		if(fwrite(linea, strlen(linea), 1, pointerFile) == 0) {
			log_error(logger, "NO SE PUDO ESCRIBIR LA LINEA <%s> ", linea);
		    exit(1);
		} else {
			log_info(logger, "LINEA ESCRITA CORRECTAMENTE <%s>", linea);
		}

		escribir_nueva_linea_en_archivo(cant_blocks, path_directorio_pokemon, linea);

    // liberar semaforo
    } else {
        // aca muere el hilo
    }

	cambiar_valor_metadata(path_directorio_pokemon, "OPEN", "N");

	free(pointerFile);
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

bool hay_cantidad_de_bloques_libres(int cantidad_de_bloques_necesaria) {
    int cantidad_disponible = 0;
    for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++) {
        if(bitarray_test_bit(bitarray, i) == 0) {
            cantidad_disponible++;
        }
    }

    if(cantidad_disponible >= cantidad_de_bloques_necesaria) {
        log_info(logger, "HAY CANTIDAD SUFICIENTE DE BLOQUES PARA ESCRIBIR EN EL ARCHIVO");
        return true;
    } else {
        log_warning(logger, "NO HAY CANTIDAD SUFICIENTE DE BLOQUES PARA ESCRIBIR EN EL ARCHIVO");
        return false;
    }
}

void escribir_nueva_linea_en_archivo(int cantidad_bloques_necesarios, char *ruta_directorio_pokemon, char *linea) {

	int desplazamiento = 0;

	if(cantidad_bloques_necesarios == 1) {
		escribir_bitmap_metadata_block(ruta_directorio_pokemon, linea, desplazamiento);
	} else {
		for(int i = 0 ; i < cantidad_bloques_necesarios ; i++) {
			escribir_bitmap_metadata_block(ruta_directorio_pokemon, linea, desplazamiento);
			desplazamiento += datos_config->size_block;
		}
	}
}

void escribir_bitmap_metadata_block(char *ruta_directorio_pokemon, char *linea, int desplazamiento) {

	int bloque = obtener_bloque_libre();

	bitarray_set_bit(bitarray, bloque);
	//bitmapFilePointer = fopen(ruta_archivo_bitmap, "wb+");
	//if(fwrite(bitarray->bitarray, sizeof(char), bitarray->size, bitmapFilePointer) == 0) { // a modode prueba
	    //log_error(logger, "NO SE PUDO ESCRIBIR EL BITARRAY EN EL ARCHIVO bitmap.bin");
	    //exit(1);
	//}
	//fclose(bitmapFilePointer);

	agregar_bloque_metadata_pokemon(ruta_directorio_pokemon, bloque);

	char *auxiliar = strdup(string_substring(linea, desplazamiento, datos_config->size_block)); //strlen(linea)

	char *ruta_directorio_blocks = string_new();
	string_append_with_format(&ruta_directorio_blocks, "%s%s", datos_config->pto_de_montaje, BLOCKS_DIR);

	escribir_datos_bloque(ruta_directorio_blocks, auxiliar, bloque);

	free(ruta_directorio_blocks);
}

// nos devuelve el nro de bloque libre
int obtener_bloque_libre() {
	int bloque = 0;
	for(int i = 0 ; i < bitarray_get_max_bit(bitarray) ; i++ ) {
		if(bitarray_test_bit(bitarray, i) == 0) {
			log_info(logger, "NUMERO DE BLOQUE LIBRE:%d",i);
			bloque = i;
			break;
		}
	}

	return bloque;
}
/*
void verificar_espacio_bloque_de_datos(char *path_metadata_pokemon, char *datos_a_agregar) { // chequea que haya espacio en el bloque , si supera el maximo se tiene que crear otro archivo

	char *path_block = string_new();
	string_append(&path_block, datos_config->pto_de_montaje);
	string_append(&path_block, BLOCKS_DIR);
	string_append(&path_block, "/");

	int size_new_string = strlen(path_metadata_pokemon) + strlen(METADATA_FILE) + 1;
	char *path_metadata_file = malloc(size_new_string);
	strcpy(path_metadata_file, path_metadata_pokemon);
	string_append(&path_metadata_file, METADATA_FILE);

	t_config *metadata_pokemon = config_create(path_metadata_file);
	char **array_blocks = config_get_array_values(metadata_pokemon, "BLOCKS");

	int data_size = strlen(datos_a_agregar);
	int cant_blocks = (int) ceil((double)data_size / datos_config->size_block);

	if(array_blocks == NULL) {
		for(int i = 0 ; i < cant_blocks ; i++) {
			int nro_bloque = obtener_bloque_libre();
			if(nro_bloque == -1)
				log_warning(logger, "NO HAY BLOQUE DE DATOS DISPONIBLE");
				break;
			int size_to_write = data_size - datos_config->size_block;
			if(size_to_write < 0) { // este if es para escribir datos con lengitud menor al tamaño del bloque de datos
				escribir_datos_bloque(path_block, datos_a_agregar, nro_bloque);
			}
			escribir_datos_bloque();
		}

	}
}
*/
void escribir_datos_bloque(char *ruta_directorio_blocks, char *datos_a_agregar, int nro_bloque) {

	char *ruta_archivo_bloque = string_new();
	string_append(&ruta_archivo_bloque, ruta_directorio_blocks);
    string_append(&ruta_archivo_bloque, "/");
	string_append(&ruta_archivo_bloque, string_itoa(nro_bloque));
	string_append(&ruta_archivo_bloque, ".txt"); //por ahora txt para ver que los datos se escribieron correctamente, despues se cambia a bin

	log_info(logger, "CREANDO ARCHIVO CON RUTA <%s>", ruta_archivo_bloque);
	FILE *pf = fopen(ruta_archivo_bloque, "ab"); //
	if(pf == NULL) {
		log_error(logger, "ERROR AL CREAR EL ARCHIVO POKEMON");
		exit(1);
	}

	log_info(logger, "ESCRIBIENDO DATOS EN EL BLOQUE <%d> EN EL DIRECTORIO <%s>", nro_bloque, ruta_archivo_bloque);
	if(fwrite(datos_a_agregar, strlen(datos_a_agregar), 1, pf) == 0)
		log_error(logger, "NO SE HA PODIDO ESCRIBIR DATOS EN EL DIRECTORIO <%s>", ruta_archivo_bloque);

	fclose(pf);
	free(ruta_archivo_bloque);
}

void agregar_bloque_metadata_pokemon(char *ruta_directorio_pokemon, int nro_bloque) {

	int size_nro_bloque = strlen(string_itoa(nro_bloque));

	int size_new_string = strlen(ruta_directorio_pokemon) + strlen(METADATA_FILE) + 1;
	char *ruta_metadata_pokemon = malloc(size_new_string);
	strcpy(ruta_metadata_pokemon, ruta_directorio_pokemon);
	string_append(&ruta_metadata_pokemon, METADATA_FILE);

	t_config *metadata_pokemon = config_create(ruta_metadata_pokemon);

	char *blocks = config_get_string_value(metadata_pokemon, "BLOCKS");
	int sizeBlocks = strlen(blocks);

	if(sizeBlocks == 2) {
		sizeBlocks += size_nro_bloque;
	} else {
		sizeBlocks += size_nro_bloque + 1;
	}

	log_info(logger, "AGREGANDO BLOQUE %d A LA ESTRUCTURA BLOCKS", nro_bloque);
	char *newBlocks = malloc(sizeBlocks);
	int posicion = 0;

	if(strlen(blocks) < 3) {
		strcpy(newBlocks + posicion, "[");
		posicion ++;
		strcpy(newBlocks + posicion, string_itoa(nro_bloque));
		posicion += size_nro_bloque;
		strcpy(newBlocks + posicion, "]");
	} else {
		//[10] --->
		char *auxiliar = strdup(string_substring(blocks,1, strlen(blocks)-2));
		strcpy(newBlocks + posicion, "[");
		posicion ++;
		strcpy(newBlocks + posicion, auxiliar);
		posicion += strlen(auxiliar); // revisar esta linea
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
}

// No hay memleak (funciona bien)
void crear_metadata_pokemon(char *ruta_directorio_pokemon, char *pokemon, int file_size) {

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
    
    //int tamanio = fileSize(path_file);

    char *campos_metadataTxt = string_new();
    string_append(&campos_metadataTxt, "DIRECTORY=N");
    string_append(&campos_metadataTxt, "\n");
    string_append(&campos_metadataTxt, "SIZE=");
    string_append(&campos_metadataTxt, string_itoa(file_size));
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

int fileSize(char* file) { 
        FILE* fp = fopen(file, "r"); 
        if (fp == NULL) { 
       log_error(logger, "No existe el archivo.");
        return -1; 
    } 
      fseek(fp, 0L, SEEK_END);  
    int res = (int) ftell(fp); 
    fclose(fp); 
    return res; 
} 
FILE* existePokemon(char* nombrePokemon){
    char* pathArchivo = string_new();
    string_append(&pathArchivo,PATH_POKECARPETA);
    string_append_with_format(&pathArchivo,"%s%s%s%s", nombrePokemon, "/",nombrePokemon,POKEMON_FILE_EXT);
    FILE *fp = fopen(pathArchivo,"r");
    printf("%s\n",pathArchivo);
    printf("%p",fp);
    printf("\nDeberia impreso el puntero\n");
    return fp;

}
char *read_file_into_buf (char **filebuf, long fplen, FILE *fp)
{
    fseek (fp, 0, SEEK_END);
    if ((fplen = ftell (fp)) == -1) {  /* get file length */
        fprintf (stderr, "error: unable to determine file length.\n");
        return NULL;
    }
    fseek (fp, 0, SEEK_SET);  /* allocate memory for file */
    if (!(*filebuf = calloc (fplen, sizeof *filebuf))) {
        fprintf (stderr, "error: virtual memory exhausted.\n");
        return NULL;
    }

    /* read entire file into filebuf */
    if (!fread (*filebuf, sizeof *filebuf, fplen, fp)) {
        fprintf (stderr, "error: file read failed.\n");
        return NULL;
    }

    return *filebuf;
}

//CAMBIE  NOMBRE PORQUE ROMPE AHORA QUE ESTAN LOS UTILS :)

void catchPokemon(char* pokemon,int posx,int posy){}

/*rtaGet* getPokemon(int idMensaje, char* pokemon){
    rtaGet* respuesta = malloc(sizeof(rtaGet));
    respuesta->id_mensaje = idMensaje;
    respuesta->name = pokemon;
    FILE *fp = existePokemon(pokemon);
    printf("%p",fp);
    if(fp != NULL){
        char* pathMetadata = string_new();
        string_append(&pathMetadata,PATH_POKECARPETA);
        string_append_with_format(&pathMetadata,"%s%s", pokemon ,METADATA_FILE);
        t_config *metadata_pokemon = config_create(pathMetadata);
        
        char* abierto = config_get_string_value(metadata_pokemon,"OPEN");
        if(abierto == "Y"){
            //finalizar hilo y reintentar despues del tiempo que dice el config
        } else{
            //settear OPEN = Y
            log_warning(logger, "NO HAY ESPACIO SUFICIENTE EN EL BITMAP");
            char* pathFILE = string_new();
            string_append(&pathFILE,PATH_POKECARPETA);
            string_append_with_format(&pathFILE,"%s%s%s%s", pokemon, "/",pokemon, POKEMON_FILE_EXT);
            char* scaneo = NULL;
            long fplen = (long) fileSize(pathFILE);
            read_file_into_buf (&scaneo, fplen, fp);
            log_info(logger, "AGREGANDO BLOQUE %d A LA ESTRUCTURA BLOCKS", string_length(scaneo));
              printf("deberia saber el length\n");
            //settear OPEN = N
            //falta separar uno a uno las lineas

            //setear la respuesta

            free(scaneo);
        }
    
    free(abierto);
    free(pathMetadata);
    }

    return respuesta;
}*/


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
			new_pokemon *newPokemon = malloc(sizeof(new_pokemon));
			int id_mensaje_new;
			memcpy(&(id_mensaje_new), stream, sizeof(int));
			printf("ID MENSAJE: %d\n", id_mensaje_new);

			//deserealizar_new_pokemon_gameboy(stream, newPokemon);
			newPokemon = deserializar_new(stream + sizeof(int));

			printf("POKEMON: %s\n", newPokemon->name);
			printf("PosX: %d\n", newPokemon->pos.posx);
			printf("PosY: %d\n", newPokemon->pos.posy);
			printf("Cantidad: %d\n", newPokemon->cantidad);

			pthread_create(&atender_new_pokemon, NULL, (void *) operacion_new_pokemon, (void *) newPokemon);

			free(newPokemon);
			free(stream);
		break;

		case CATCH_POKEMON:
			log_info(logger, "SE RECIBIO UN MENSAJE CON OPERACION CATCH_POKEMON");
			catch_pokemon *catchPokemon = malloc(sizeof(catch_pokemon));
			int id_mensaje_catch;
			memcpy(&(id_mensaje_catch), stream, sizeof(int));
			printf("ID MENSAJE: %d\n", id_mensaje_catch);
			//deserealizar_catch_pokemon_gameboy(stream, catchPokemon);
			catchPokemon = deserializar_catch(stream + sizeof(int));

			printf("POKEMON: %s\n", catchPokemon->name);
			printf("PosX: %d\n", catchPokemon->pos.posx);
			printf("PosY: %d\n", catchPokemon->pos.posy);

			free(catchPokemon);
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

			free(getPokemon);
			free(stream);
		break;

		default:
			log_warning(logger, "NO SE RECIBIO NINGUNA DE LAS ANTERIORES");
		break;
	}
}
